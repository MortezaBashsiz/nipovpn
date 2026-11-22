#!/usr/bin/env python3
"""End-to-end tests for NipoVPN.

Starts a real server and agent on localhost for each test, sends traffic
through them with curl and checks that every byte arrives intact.

    python3 tests/e2e.py --binary build/core/nipovpn
    python3 tests/e2e.py --binary build/core/nipovpn -k matrix   # only matching tests
    python3 tests/e2e.py --binary build/core/nipovpn --list

Needs: python3 with PyYAML, curl and openssl.
"""

import argparse
import copy
import hashlib
import http.server
import itertools
import os
import re
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import traceback
from urllib.parse import urlsplit

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required: pip install pyyaml (or apt install python3-yaml)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLE_CONFIG = os.path.join(ROOT, "nipovpn", "etc", "nipovpn", "config.yaml")

CRASH_MARKERS = (
    "terminate called",
    "Segmentation fault",
    "Aborted",
    "AddressSanitizer",
    "runtime error:",
)


class TestFailure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise TestFailure(message)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def new_token():
    return secrets.token_hex(20)


# ---------------------------------------------------------------- origin server


class Origin:
    """Local web server the traffic is sent to.

    GET /<name> returns a fixed random blob, GET /slow streams the big blob
    over about 3 seconds, and POST /upload returns the SHA-256
    of the request body. Accepts absolute-form request targets
    (GET http://host/path), which an HTTP proxy forwards unchanged.
    """

    SIZES = {"small": 64 * 1024, "mid": 512 * 1024, "big": 2 * 1024 * 1024}

    def __init__(self):
        self.blobs = {name: os.urandom(size) for name, size in self.SIZES.items()}
        blobs = self.blobs

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):
                pass

            def _send(self, status, body):
                self.send_response(status)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                name = urlsplit(self.path).path.strip("/")
                if name == "slow":
                    body = blobs["big"]
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    try:
                        for i in range(0, len(body), 64 * 1024):
                            self.wfile.write(body[i:i + 64 * 1024])
                            self.wfile.flush()
                            time.sleep(0.1)
                    except OSError:
                        pass
                elif name in blobs:
                    self._send(200, blobs[name])
                else:
                    self._send(404, b"not found")

            def do_POST(self):
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                self._send(200, sha256(body).encode())

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def url(self, name):
        return f"http://127.0.0.1:{self.port}/{name}"

    def close(self):
        self.server.shutdown()
        self.server.server_close()


# ---------------------------------------------------------------- processes


class Node:
    """One nipovpn process (server or agent)."""

    def __init__(self, env, mode, config, name):
        self.env = env
        self.mode = mode
        self.name = name
        self.config = config
        self.config_path = os.path.join(env.dir, f"{name}.yaml")
        self.log_path = config["log"]["logFile"]
        self.out_path = os.path.join(env.dir, f"{name}.out")
        self.proc = None
        self.write_config(config)

    def write_config(self, config):
        self.config = config
        tmp = self.config_path + ".tmp"
        with open(tmp, "w") as f:
            yaml.safe_dump(config, f, sort_keys=False)
        os.replace(tmp, self.config_path)

    @property
    def port(self):
        return self.config[self.mode]["listenPort"]

    def start(self):
        open(self.log_path, "a").close()
        self.out = open(self.out_path, "w")
        self.proc = subprocess.Popen(
            [self.env.binary, self.mode, self.config_path],
            stdout=self.out,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        self.env.nodes.append(self)
        deadline = time.time() + 15
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise TestFailure(f"{self.name} exited at startup:\n{self.output()}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return self
            except OSError:
                time.sleep(0.05)
        raise TestFailure(f"{self.name} did not start listening on port {self.port}")

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def send_signal(self, sig):
        self.proc.send_signal(sig)

    def stop(self):
        if self.proc is None:
            return
        if self.alive():
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        self.out.close()

    def output(self):
        try:
            with open(self.out_path, errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    def log(self):
        try:
            with open(self.log_path, errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    def wait_for_log(self, pattern, count=1, timeout=15):
        regex = re.compile(pattern)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if len(regex.findall(self.log())) >= count:
                return
            time.sleep(0.1)
        raise TestFailure(f"{self.name} log never showed /{pattern}/ (x{count})")

    def crash_report(self):
        text = self.output()
        for marker in CRASH_MARKERS:
            if marker in text:
                start = max(0, text.find("ERROR:") if "ERROR:" in text else text.find(marker))
                return f"{self.name} crashed ({marker}):\n{text[start:start + 6000]}"
        if self.proc is not None and self.proc.returncode not in (None, 0, -signal.SIGTERM):
            return f"{self.name} exited with code {self.proc.returncode}:\n{text[-2000:]}"
        return None


class Env:
    """Per-test working directory, configs and processes."""

    def __init__(self, runner, name):
        self.binary = runner.binary
        self.origin = runner.origin
        self.cert, self.key = runner.cert, runner.key
        self.dir = tempfile.mkdtemp(prefix=re.sub(r"[^A-Za-z0-9]+", "_", name)[:60] + "_",
                                    dir=runner.workdir)
        self.nodes = []
        with open(SAMPLE_CONFIG) as f:
            self.sample = yaml.safe_load(f)

    # -- configs

    def base(self, tls=True, tunnel=False, aes=True, reuse=True, protocol="socks5",
             users=None, agent_token=None):
        """A config derived from the shipped sample config."""
        cfg = copy.deepcopy(self.sample)
        server_port, agent_port = free_port(), free_port()
        if users is None:
            users = [{"name": "alice", "token": new_token(), "enabled": True}]
        cfg["general"].update(aesEnable=aes, tunnelEnable=tunnel, tlsEnable=tls, timeout=10)
        cfg["log"]["logLevel"] = "INFO"
        cfg["server"].update(
            threads=4,
            listenIp="127.0.0.1",
            listenPort=server_port,
            tlsCertFile=self.cert,
            tlsKeyFile=self.key,
            usage={"file": os.path.join(self.dir, "usage.yaml"), "resetDay": 1},
            users=users,
        )
        cfg["agent"].update(
            threads=8,
            listenIp="127.0.0.1",
            listenPort=agent_port,
            serverIp="127.0.0.1",
            serverPort=server_port,
            protocol=protocol,
            connectionReuse=reuse,
            tlsVerifyPeer=False,
            tlsCaFile="",
            token=agent_token or users[0]["token"],
        )
        return cfg

    def server(self, cfg, name="server"):
        cfg = copy.deepcopy(cfg)
        cfg["log"]["logFile"] = os.path.join(self.dir, f"{name}.log")
        return Node(self, "server", cfg, name).start()

    def agent(self, cfg, name="agent", token=None):
        cfg = copy.deepcopy(cfg)
        cfg["log"]["logFile"] = os.path.join(self.dir, f"{name}.log")
        cfg["agent"]["listenPort"] = free_port()
        if token:
            cfg["agent"]["token"] = token
        return Node(self, "agent", cfg, name).start()

    # -- traffic

    def curl(self, agent, url, protocol=None, connect=True, data_file=None, timeout=60):
        protocol = protocol or agent.config["agent"].get("protocol") or agent.config["general"]["protocol"]
        cmd = ["curl", "-sS", "--fail", "--max-time", str(timeout), "-o", "-"]
        if protocol == "socks5":
            cmd += ["--socks5-hostname", f"127.0.0.1:{agent.port}"]
        else:
            cmd += ["-x", f"http://127.0.0.1:{agent.port}"]
            if connect:
                cmd.append("-p")
        if data_file:
            cmd += ["--data-binary", "@" + data_file, "-H", "Content-Type: application/octet-stream"]
        cmd.append(url)
        result = subprocess.run(cmd, capture_output=True, timeout=timeout + 10)
        return result.returncode, result.stdout, result.stderr.decode(errors="replace").strip()

    @staticmethod
    def how(agent, kw):
        protocol = kw.get("protocol") or agent.config["agent"].get("protocol") or agent.config["general"]["protocol"]
        if protocol == "socks5":
            return "over SOCKS5"
        return "over HTTP CONNECT" if kw.get("connect", True) else "as a plain HTTP proxy request"

    def download_ok(self, agent, name="big", **kw):
        rc, body, err = self.curl(agent, self.origin.url(name), **kw)
        what = f"download of /{name} {self.how(agent, kw)}"
        check(rc == 0, f"{what} failed (curl exit {rc}: {err})")
        check(sha256(body) == sha256(self.origin.blobs[name]),
              f"{what} corrupted: got {len(body)} bytes, "
              f"expected {len(self.origin.blobs[name])}")

    def download_fails(self, agent, name="small", **kw):
        rc, body, _ = self.curl(agent, self.origin.url(name), timeout=15, **kw)
        check(rc != 0 or sha256(body) != sha256(self.origin.blobs[name]),
              f"download of /{name} succeeded but should have been refused")

    def upload_ok(self, agent, size=1024 * 1024, **kw):
        data = os.urandom(size)
        path = os.path.join(self.dir, f"upload_{secrets.token_hex(4)}.bin")
        with open(path, "wb") as f:
            f.write(data)
        rc, body, err = self.curl(agent, self.origin.url("upload"), data_file=path, **kw)
        what = f"upload of {size} bytes {self.how(agent, kw)}"
        check(rc == 0, f"{what} failed (curl exit {rc}: {err})")
        check(body.decode(errors="replace") == sha256(data), f"{what} corrupted on the way")

    def parallel_downloads_ok(self, agent, count=4, name="mid", **kw):
        errors = []

        def one():
            try:
                self.download_ok(agent, name, **kw)
            except Exception as e:
                errors.append(str(e))

        threads = [threading.Thread(target=one) for _ in range(count)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        check(not errors, f"{len(errors)} of {count} parallel downloads failed: {errors[0] if errors else ''}")

    def usage(self):
        path = os.path.join(self.dir, "usage.yaml")
        check(os.path.exists(path), "usage file was not written")
        with open(path) as f:
            doc = yaml.safe_load(f) or {}
        return {name: entry.get("used", 0) for name, entry in (doc.get("users") or {}).items()}

    # -- teardown

    def close(self):
        crashes = []
        for node in reversed(self.nodes):
            node.stop()
            report = node.crash_report()
            if report:
                crashes.append(report)
        return crashes


# ---------------------------------------------------------------- tests

TESTS = []


def test(name):
    def register(fn):
        TESTS.append((name, fn))
        return fn

    return register


def onoff(value):
    return "on" if value else "off"


def make_matrix_test(tls, tunnel, reuse, aes, protocol):
    def run(env):
        cfg = env.base(tls=tls, tunnel=tunnel, reuse=reuse, aes=aes, protocol=protocol)
        env.server(cfg)
        agent = env.agent(cfg)
        if protocol == "socks5":
            env.download_ok(agent, "big")
            env.download_ok(agent, "small")
            env.upload_ok(agent)
            env.parallel_downloads_ok(agent)
        else:
            env.download_ok(agent, "big", connect=True)
            env.download_ok(agent, "small", connect=True)
            env.upload_ok(agent, connect=True)
            env.parallel_downloads_ok(agent, connect=True)

    return run


for _tls, _tunnel, _reuse, _aes, _protocol in itertools.product(
        (True, False), (False, True), (True, False), (True, False), ("socks5", "http")):
    TESTS.append((
        f"matrix tls={onoff(_tls)} tunnel={onoff(_tunnel)} reuse={onoff(_reuse)} "
        f"aes={onoff(_aes)} protocol={_protocol}",
        make_matrix_test(_tls, _tunnel, _reuse, _aes, _protocol),
    ))


KNOWN_BUG_PLAIN_HTTP = (
    "the server returns only its first read (up to 16 KB) from the destination as the "
    "response to a plain HTTP proxy request; also in 1.1.60")

for _tunnel, _tls in itertools.product((False, True), (True, False)):
    def _plain_http_test(env, tunnel=_tunnel, tls=_tls):
        cfg = env.base(tls=tls, tunnel=tunnel, protocol="http")
        env.server(cfg)
        agent = env.agent(cfg)
        env.download_ok(agent, "small", connect=False)
        env.download_ok(agent, "big", connect=False)
        env.upload_ok(agent, connect=False)

    _plain_http_test.known_bug = KNOWN_BUG_PLAIN_HTTP
    TESTS.append((f"plain HTTP proxy request tls={onoff(_tls)} tunnel={onoff(_tunnel)}",
                  _plain_http_test))


@test("http version 1.0 between agent and server")
def _(env):
    for tunnel in (False, True):
        cfg = env.base(tunnel=tunnel)
        cfg["agent"]["httpVersion"] = "1.0"
        env.server(cfg, name=f"server_{tunnel}")
        agent = env.agent(cfg, name=f"agent_{tunnel}")
        env.download_ok(agent)
        env.upload_ok(agent)


def two_users(env, **kw):
    users = [
        {"name": "alice", "token": new_token(), "enabled": True},
        {"name": "bob", "token": new_token(), "enabled": True},
    ]
    return env.base(users=users, **kw), users


for _aes, _tunnel in itertools.product((True, False), (False, True)):
    def _users_test(env, aes=_aes, tunnel=_tunnel):
        cfg, users = two_users(env, aes=aes, tunnel=tunnel)
        server = env.server(cfg)
        alice = env.agent(cfg, name="agent_alice", token=users[0]["token"])
        bob = env.agent(cfg, name="agent_bob", token=users[1]["token"])

        errors = []

        def traffic(agent, name):
            try:
                env.download_ok(agent, name)
            except Exception as e:
                errors.append(str(e))

        threads = [threading.Thread(target=traffic, args=(bob, "big")),
                   threading.Thread(target=traffic, args=(bob, "mid")),
                   threading.Thread(target=traffic, args=(alice, "small"))]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        check(not errors, f"traffic of two users at once failed: {errors}")

        server.stop()
        log = server.log()
        check("[USER alice]" in log and "[USER bob]" in log,
              "server log does not name both users")
        used = env.usage()
        big = len(env.origin.blobs["big"]) + len(env.origin.blobs["mid"])
        check(used.get("bob", 0) >= big, f"bob's usage {used.get('bob')} < {big} bytes downloaded")
        check(0 < used.get("alice", 0) < big,
              f"alice's usage {used.get('alice')} does not match her small download")

    TESTS.append((f"users identified and counted aes={onoff(_aes)} tunnel={onoff(_tunnel)}",
                  _users_test))


for _aes in (True, False):
    def _unknown_test(env, aes=_aes):
        cfg = env.base(aes=aes)
        server = env.server(cfg)
        stranger = env.agent(cfg, name="agent_unknown", token=new_token())
        env.download_fails(stranger)
        check(server.alive(), "server died after an unknown token")
        good = env.agent(cfg, name="agent_alice")
        env.download_ok(good, "small")

    TESTS.append((f"unknown token refused aes={onoff(_aes)}", _unknown_test))


@test("disabled user refused")
def _(env):
    cfg, users = two_users(env)
    users[1]["enabled"] = False
    env.server(cfg)
    env.download_fails(env.agent(cfg, name="agent_bob", token=users[1]["token"]))
    env.download_ok(env.agent(cfg, name="agent_alice", token=users[0]["token"]), "small")


@test("reload disables and re-enables a user")
def _(env):
    cfg, users = two_users(env)
    server = env.server(cfg)
    bob = env.agent(cfg, name="agent_bob", token=users[1]["token"])
    alice = env.agent(cfg, name="agent_alice", token=users[0]["token"])
    env.download_ok(bob, "small")

    users[1]["enabled"] = False
    server.write_config(server.config | {"server": cfg["server"]})
    server.send_signal(signal.SIGHUP)
    server.wait_for_log(r"config reloaded.*access revoked for: bob")
    env.download_fails(bob)
    env.download_ok(alice, "small")

    users[1]["enabled"] = True
    server.write_config(server.config | {"server": cfg["server"]})
    server.send_signal(signal.SIGHUP)
    server.wait_for_log(r"config reloaded", count=2)
    env.download_ok(bob, "small")


@test("reload revokes a user in the middle of a download")
def _(env):
    for tunnel in (False, True):
        cfg, users = two_users(env, tunnel=tunnel)
        server = env.server(cfg, name=f"server_{tunnel}")
        bob = env.agent(cfg, name=f"agent_bob_{tunnel}", token=users[1]["token"])
        result = {}

        def slow_download():
            result["rc"], result["body"], _ = env.curl(bob, env.origin.url("slow"), timeout=60)

        t = threading.Thread(target=slow_download)
        t.start()
        time.sleep(0.8)
        users[1]["enabled"] = False
        server.write_config(server.config | {"server": cfg["server"]})
        server.send_signal(signal.SIGHUP)
        server.wait_for_log(r"access revoked for: bob")
        t.join()
        check(result.get("rc") != 0 or len(result.get("body", b"")) < len(env.origin.blobs["big"]),
              f"tunnel={onoff(tunnel)}: the download continued after bob was revoked")
        env.download_fails(bob)


@test("reload with an invalid config keeps the old one")
def _(env):
    cfg = env.base()
    server = env.server(cfg)
    agent = env.agent(cfg)
    with open(server.config_path, "w") as f:
        f.write("general: [this is: not valid\n")
    server.send_signal(signal.SIGHUP)
    server.wait_for_log(r"config not reloaded")
    check(server.alive(), "server died on an invalid reload")
    env.download_ok(agent, "small")


for _tunnel in (False, True):
    def _limit_test(env, tunnel=_tunnel):
        cfg, users = two_users(env, tunnel=tunnel)
        users[1]["limit"] = "1MB"
        server = env.server(cfg)
        bob = env.agent(cfg, name="agent_bob", token=users[1]["token"])
        alice = env.agent(cfg, name="agent_alice", token=users[0]["token"])

        rc, body, _ = env.curl(bob, env.origin.url("big"), timeout=30)
        check(rc != 0 or len(body) < len(env.origin.blobs["big"]),
              "bob downloaded 2 MB despite a 1MB limit")
        server.wait_for_log(r"user bob reached their limit")
        env.download_fails(bob)
        env.download_ok(alice, "small")

        users[1]["limit"] = "100MB"
        server.write_config(server.config | {"server": cfg["server"]})
        server.send_signal(signal.SIGHUP)
        server.wait_for_log(r"user bob is below their limit again")
        env.download_ok(bob, "small")

    TESTS.append((f"usage limit blocks and reload unblocks tunnel={onoff(_tunnel)}", _limit_test))


@test("usage survives a server restart")
def _(env):
    cfg = env.base()
    server = env.server(cfg)
    agent = env.agent(cfg)
    env.download_ok(agent, "big")
    server.stop()
    first = env.usage().get("alice", 0)
    check(first >= len(env.origin.blobs["big"]), f"usage {first} lower than the download")

    server = env.server(cfg, name="server2")
    env.download_ok(agent, "small")
    server.stop()
    second = env.usage().get("alice", 0)
    check(second >= first + len(env.origin.blobs["small"]),
          f"usage went from {first} to {second}; the restart lost it")


def legacy_config(env, aes=True, tunnel=False, token=True):
    """The 1.1.x layout: everything shared in general, one general.token."""
    cfg = env.base(aes=aes, tunnel=tunnel)
    legacy = copy.deepcopy(cfg)
    for key in ("protocol", "fakeUrls", "methods", "endPoints", "pullTimeout",
                "connectionReuse", "tlsVerifyPeer", "tlsCaFile"):
        if key in legacy["agent"]:
            legacy["general"][key] = legacy["agent"].pop(key)
    for key in ("bufferSize", "tlsCertFile", "tlsKeyFile"):
        if key in legacy["server"]:
            legacy["general"][key] = legacy["server"].pop(key)
    agent_token = legacy["agent"].pop("token")
    legacy["server"].pop("users")
    legacy["server"].pop("usage")
    if token:
        legacy["general"]["token"] = agent_token
    return legacy, agent_token


for _aes, _tunnel in itertools.product((True, False), (False, True)):
    def _legacy_test(env, aes=_aes, tunnel=_tunnel):
        cfg, _ = legacy_config(env, aes=aes, tunnel=tunnel)
        server = env.server(cfg)
        agent = env.agent(cfg)
        env.download_ok(agent, "big")
        env.upload_ok(agent)
        check("moved out of general" in server.log(), "server did not warn about the old layout")
        check("moved out of general" in agent.log(), "agent did not warn about the old layout")
        check("general.token is deprecated" in agent.log(), "agent did not warn about general.token")

    TESTS.append((f"1.1.x config layout still works aes={onoff(_aes)} tunnel={onoff(_tunnel)}",
                  _legacy_test))


@test("aes off server without users or token")
def _(env):
    cfg, token = legacy_config(env, aes=False, token=False)
    env.server(cfg)
    agent_cfg = copy.deepcopy(cfg)
    agent_cfg["agent"]["token"] = token
    agent = env.agent(agent_cfg)
    env.download_ok(agent, "big")
    env.upload_ok(agent)


# ---------------------------------------------------------------- validation


def mutate(path, value):
    def apply(cfg):
        node = cfg
        for key in path[:-1]:
            node = node[key]
        if value is DELETE:
            node.pop(path[-1], None)
        else:
            node[path[-1]] = value

    return apply


DELETE = object()

VALIDATION_CASES = [
    ("server", mutate(("server", "bufferSize"), 70000), "server.bufferSize must be between 1 and 65536"),
    ("server", mutate(("server", "bufferSize"), 0), "server.bufferSize must be between 1 and 65536"),
    ("agent", mutate(("agent", "protocol"), "ftp"), "agent.protocol must be one of"),
    ("agent", mutate(("agent", "fakeUrls"), {"a": 1}), "Error in agent.fakeUrls"),
    ("agent", mutate(("agent", "token"), "short"), "agent.token must be at least 32 bytes"),
    ("agent", mutate(("agent", "token"), DELETE), "agent.token is required"),
    ("server", mutate(("log", "logLevel"), "LOUD"), "logLevel must be one of"),
    ("server", mutate(("server", "users"), []), "server.users must have at least one entry"),
    ("server", mutate(("server", "users"), "alice"), "server.users must be a list"),
    ("server", mutate(("server", "users"), [{"name": "bad name!", "token": "x" * 40}]),
     "name must be 1-64 characters"),
    ("server", mutate(("server", "users"), [{"name": "a", "token": "short"}]),
     "token of user a must be at least"),
    ("server", mutate(("server", "users"), [{"name": "a", "token": "x" * 40},
                                            {"name": "a", "token": "y" * 40}]),
     "user name a is used more than once"),
    ("server", mutate(("server", "users"), [{"name": "a", "token": "x" * 40},
                                            {"name": "b", "token": "x" * 40}]),
     "have the same token"),
    ("server", mutate(("server", "users"), [{"name": "a", "token": "x" * 40, "limit": "2048"}]),
     "limit must be a size with a unit"),
    ("server", mutate(("server", "usage"), {"resetDay": 40}), "resetDay must be between 1 and 28"),
]


def _validation_test(env):
    failures = []
    for mode, apply, expected in VALIDATION_CASES:
        cfg = env.base()
        cfg["log"]["logFile"] = os.path.join(env.dir, "validation.log")
        apply(cfg)
        path = os.path.join(env.dir, "invalid.yaml")
        with open(path, "w") as f:
            yaml.safe_dump(cfg, f, sort_keys=False)
        open(cfg["log"]["logFile"], "a").close()
        try:
            result = subprocess.run([env.binary, mode, path], capture_output=True, timeout=10)
        except subprocess.TimeoutExpired:
            failures.append(f"{mode}: expected '{expected}' but the program started")
            continue
        output = (result.stdout + result.stderr).decode(errors="replace")
        if result.returncode == 0 or expected not in output:
            failures.append(f"{mode}: expected '{expected}', got exit {result.returncode}: "
                            f"{output.strip()[-300:]}")

    for mode, text, expected in [
        ("server", "users:\n  - name: a\n", "users belongs under the server block"),
        ("server", "general: [broken\n", "Error parsing config file"),
    ]:
        cfg = env.base()
        cfg["log"]["logFile"] = os.path.join(env.dir, "validation.log")
        path = os.path.join(env.dir, "invalid.yaml")
        with open(path, "w") as f:
            if text.startswith("users"):
                yaml.safe_dump(cfg, f, sort_keys=False)
            f.write(text)
        result = subprocess.run([env.binary, mode, path], capture_output=True, timeout=10)
        output = (result.stdout + result.stderr).decode(errors="replace")
        if result.returncode == 0 or expected not in output:
            failures.append(f"{mode}: expected '{expected}', got exit {result.returncode}: "
                            f"{output.strip()[-300:]}")

    check(not failures, "\n".join(failures))


TESTS.append(("invalid configs are rejected with a clear message", _validation_test))


@test("sample config passes validation")
def _(env):
    for mode in ("server", "agent"):
        cfg = copy.deepcopy(env.sample)
        cfg["log"]["logFile"] = os.path.join(env.dir, "sample.log")
        cfg["server"]["usage"] = {"file": os.path.join(env.dir, "usage.yaml")}
        cfg["server"]["listenIp"], cfg["server"]["listenPort"] = "127.0.0.1", free_port()
        cfg["agent"]["listenIp"], cfg["agent"]["listenPort"] = "127.0.0.1", free_port()
        cfg["server"]["tlsCertFile"], cfg["server"]["tlsKeyFile"] = env.cert, env.key
        node = Node(env, mode, cfg, f"sample_{mode}")
        node.start()


# ---------------------------------------------------------------- runner


class Runner:
    def __init__(self, args):
        self.binary = os.path.abspath(args.binary)
        if not os.access(self.binary, os.X_OK):
            sys.exit(f"{self.binary} is not an executable")
        for tool in ("curl", "openssl"):
            if not shutil.which(tool):
                sys.exit(f"{tool} is required")
        self.workdir = tempfile.mkdtemp(prefix="nipovpn-e2e-")
        self.artifacts = args.artifacts
        self.cert = os.path.join(self.workdir, "server.crt")
        self.key = os.path.join(self.workdir, "server.key")
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", self.key, "-out", self.cert, "-days", "2",
                        "-subj", "/CN=nipovpn-test"], check=True, capture_output=True)
        self.origin = Origin()

    def run(self, tests):
        passed, failed, known, fixed = [], [], [], []
        started = time.time()
        for index, (name, fn) in enumerate(tests, 1):
            env = Env(self, name)
            t0 = time.time()
            error = None
            try:
                fn(env)
            except TestFailure as e:
                error = str(e)
            except Exception:
                error = traceback.format_exc()
            crashes = env.close()
            if crashes:
                error = (error + "\n" if error else "") + "\n".join(crashes)
            duration = time.time() - t0
            known_bug = getattr(fn, "known_bug", None)
            if known_bug and error is not None and not crashes:
                status = "KNOWN BUG"
            elif known_bug and error is None:
                status = "FIXED?"
            else:
                status = "PASS" if error is None else "FAIL"
            print(f"[{index:>2}/{len(tests)}] {status} {name} ({duration:.1f}s)", flush=True)
            if status == "KNOWN BUG":
                known.append(name)
                print(f"       {known_bug}\n       {error.splitlines()[0]}", flush=True)
                shutil.rmtree(env.dir, ignore_errors=True)
            elif status == "FIXED?":
                fixed.append(name)
                print("       passes now: remove its known_bug marker", flush=True)
                shutil.rmtree(env.dir, ignore_errors=True)
            elif error is None:
                passed.append(name)
                shutil.rmtree(env.dir, ignore_errors=True)
            else:
                failed.append(name)
                print("       " + error.replace("\n", "\n       "), flush=True)
                if self.artifacts:
                    dest = os.path.join(self.artifacts, os.path.basename(env.dir))
                    shutil.copytree(env.dir, dest, dirs_exist_ok=True)

        print(f"\n{len(passed)} passed, {len(failed)} failed, {len(known)} known bugs, "
              f"{len(fixed)} known bugs passing in {time.time() - started:.0f}s")
        for name in failed:
            print(f"  FAILED: {name}")
        self.origin.close()
        if not failed:
            shutil.rmtree(self.workdir, ignore_errors=True)
        return not failed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", default=os.path.join(ROOT, "build", "core", "nipovpn"),
                        help="nipovpn binary to test (default: build/core/nipovpn)")
    parser.add_argument("-k", dest="filter", action="append", default=[],
                        help="run only tests whose name contains this text (repeatable)")
    parser.add_argument("--list", action="store_true", help="list tests and exit")
    parser.add_argument("--artifacts", help="copy logs and configs of failed tests here")
    args = parser.parse_args()

    tests = [(n, f) for n, f in TESTS if not args.filter or any(k in n for k in args.filter)]
    if args.list:
        for name, _ in tests:
            print(name)
        return
    if not tests:
        sys.exit("no tests match")
    sys.exit(0 if Runner(args).run(tests) else 1)


if __name__ == "__main__":
    main()
