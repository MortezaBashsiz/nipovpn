#!/usr/bin/env python3

import argparse
import base64
import json
import sys

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required: pip install pyyaml (or install python-yaml)")

DEFAULT_CONFIG = "/etc/nipovpn/config.yaml"


def to_str(value):
    return "" if value is None else str(value)


def to_lines(value):
    if isinstance(value, (list, tuple)):
        return "\n".join(str(v) for v in value)
    return to_str(value)


def build_payload(cfg, name=None, server_ip=None):
    general = cfg.get("general", {}) or {}
    log = cfg.get("log", {}) or {}
    agent = cfg.get("agent", {}) or {}

    server_ip = server_ip or to_str(agent.get("serverIp"))
    config = {
        "token": to_str(general.get("token")),
        "protocol": to_str(general.get("protocol")),
        "fakeUrls": to_lines(general.get("fakeUrls")),
        "methods": to_lines(general.get("methods")),
        "endPoints": to_lines(general.get("endPoints")),
        "timeout": to_str(general.get("timeout")),
        "pullTimeout": to_str(general.get("pullTimeout")),
        "tunnelEnable": bool(general.get("tunnelEnable", False)),
        "connectionReuse": bool(general.get("connectionReuse", False)),
        "tlsEnable": bool(general.get("tlsEnable", False)),
        "tlsVerifyPeer": bool(general.get("tlsVerifyPeer", False)),
        "tlsCertFile": to_str(general.get("tlsCertFile")),
        "tlsKeyFile": to_str(general.get("tlsKeyFile")),
        "tlsCaFile": to_str(general.get("tlsCaFile")),
        "logLevel": to_str(log.get("logLevel")),
        "listenIp": to_str(agent.get("listenIp")),
        "listenPort": to_str(agent.get("listenPort")),
        "serverIp": server_ip,
        "serverPort": to_str(agent.get("serverPort")),
        "httpVersion": to_str(agent.get("httpVersion")),
        "userAgent": to_str(agent.get("userAgent")),
    }
    return {"name": name or server_ip, "config": config}


def encode(payload):
    text = json.dumps(payload, separators=(",", ":"), ensure_ascii=False)
    text = text.replace("/", "\\/")
    return "nipovpn://" + base64.b64encode(text.encode("utf-8")).decode("ascii")


def decode(url):
    data = url.strip()
    if data.startswith("nipovpn://"):
        data = data[len("nipovpn://"):]
    data += "=" * (-len(data) % 4)
    return json.loads(base64.b64decode(data).decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description="Convert nipovpn config.yaml to a nipovpn:// URL")
    parser.add_argument("config", nargs="?", default=DEFAULT_CONFIG,
                        help=f"path to config.yaml (default: {DEFAULT_CONFIG})")
    parser.add_argument("--name", help="profile name (default: serverIp)")
    parser.add_argument("--server-ip", help="override agent.serverIp (e.g. the public IP of the server)")
    parser.add_argument("--decode", metavar="URL", help="decode a nipovpn:// URL and print its JSON")
    args = parser.parse_args()

    if args.decode:
        print(json.dumps(decode(args.decode), indent=2, ensure_ascii=False))
        return

    try:
        with open(args.config, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f) or {}
    except OSError as e:
        sys.exit(f"Cannot read {args.config}: {e}")

    print(encode(build_payload(cfg, args.name, args.server_ip)))


if __name__ == "__main__":
    main()
