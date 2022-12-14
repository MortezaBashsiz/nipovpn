#!/bin/bash  -
#===============================================================================
#
#          FILE: external.sh
#
#         USAGE: ./external.sh
#
#   DESCRIPTION: 
#
#       OPTIONS: ---
#  REQUIREMENTS: ---
#          BUGS: ---
#         NOTES: ---
#        AUTHOR: Morteza Bashsiz (mb), morteza.bashsiz@gmail.com
#  ORGANIZATION: Linux
#       CREATED: 10/30/2022 08:48:41 PM
#      REVISION:  ---
#===============================================================================

set -o nounset                                  # Treat unset variables as an error

# Function fncSetupExternalCommon
# Setup external host 
function fncSetupExternalCommon {
	scp -r -P "$_EXTERNAL_SSH_PORT" ../tools root@"$_EXTERNAL_IP":/opt/
	echo "${_EXTERNAL_IPTABLES_CFG}" > /tmp/external_iptables
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_iptables root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_iptables /etc/iptables/rules.v4"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl restart iptables.service; systemctl enable iptables.service;"
	echo "${_FAIL2BAN_CFG}" > /tmp/external_fail2ban
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_fail2ban root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_fail2ban /etc/fail2ban/jail.d/sshd.conf"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl restart fail2ban.service; systemctl enable fail2ban.service;"
}
# End of Function fncSetupExternalCommon

# Function fncSetupExternalV2raySystemd
# Setup external host 
function fncSetupExternalV2raySystemd {
	if [[ "$_DIST" == "UBUNTU" ]]; then
		echo "${_V2RAY_VMESS_SYSTEMD_CFG}" > /tmp/external_v2rayvmess_systemd
		scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_v2rayvmess_systemd root@"$_EXTERNAL_IP":/root/
		fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mkdir -p /etc/systemd/system/v2ray.service.d"
		fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_v2rayvmess_systemd /etc/systemd/system/v2ray.service.d/v2ray.conf"
	fi
}
# End of Function fncSetupExternalV2raySystemd

# Function fncSetupExternalShadowsocks
# Setup external host with Shadowsocks
function fncSetupExternalShadowsocks {
	tempHost="NULL"
	tempPort="NULL"
	if [[ "$_BOTH_OR_EXTERNAL" == "external" ]]; then
		tempHost="$_EXTERNAL_IP"
		tempPort="$_EXTERNAL_VPN_PORT"
	elif [[ "$_BOTH_OR_EXTERNAL" == "both" ]]; then
		tempHost="$_INTERNAL_IP"
		tempPort="$_INTERNAL_VPN_PORT"
	fi
	fncSetupExternalCommon
	echo "${_SHADOWSOCKS_CFG}" > /tmp/external_shadowsocks
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_shadowsocks root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_shadowsocks /etc/shadowsocks-libev/config.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl restart shadowsocks-libev.service; systemctl enable shadowsocks-libev.service;"
	echo ""
	echo "> External Host is configured"
	echo "> use the following configuration for your android client"
	# shellcheck disable=SC2154
	echo "
		server: $tempHost
		server_port: $tempPort
		password: $_pass
		method: chacha20-ietf-poly1305
		plugin_opts: obfs=http;obfs-host=www.google.com
	"
}
# End of Function fncSetupExternalShadowsocks

# Function fncSetupExternalV2rayVmess
# Setup external host with V2rayVmess
function fncSetupExternalV2rayVmess {
	tempHost="NULL"
	tempPort="NULL"
	if [[ "$_BOTH_OR_EXTERNAL" == "external" ]]; then
		tempHost="$_EXTERNAL_IP"
		tempPort="$_EXTERNAL_VPN_PORT"
	elif [[ "$_BOTH_OR_EXTERNAL" == "both" ]]; then
		tempHost="$_INTERNAL_IP"
		tempPort="$_INTERNAL_VPN_PORT"
	fi
	fncSetupExternalV2raySystemd
	echo "${_V2RAY_VMESS_CFG}" > /tmp/external_v2rayvmess
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_v2rayvmess root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_v2rayvmess /etc/v2ray/config.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl daemon-reload; systemctl restart v2ray.service; systemctl enable v2ray.service;"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "python3 /opt/tools/conf2vmess.py -c /etc/v2ray/config.json -s $tempHost -p $tempPort -o /opt/tools/output-vmess.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "python3 /opt/tools/vmess2sub.py /opt/tools/output-vmess.json /opt/tools/output-vmess_v2rayN.html -l /opt/tools/output-vmess_v2rayN.lnk"
	_vmessurl=$(fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "cat /opt/tools/output-vmess_v2rayN.lnk")
	echo ""
	echo "> Your VMESS url is as following inport it to your client device"
	echo "$_vmessurl"
}
# End of Function fncSetupExternalV2rayVmess

# Function fncSetupExternalV2rayVmessWs
# Setup external host with V2ray Vmess WS
function fncSetupExternalV2rayVmessWs {
	tempHost="NULL"
	tempPort="NULL"
	if [[ "$_BOTH_OR_EXTERNAL" == "external" ]]; then
		tempHost="$_EXTERNAL_IP"
		tempPort="$_EXTERNAL_VPN_PORT"
	elif [[ "$_BOTH_OR_EXTERNAL" == "both" ]]; then
		tempHost="$_INTERNAL_IP"
		tempPort="$_INTERNAL_VPN_PORT"
	fi
	fncSetupExternalV2raySystemd
	echo "${_V2RAY_VMESS_WS_CFG}" > /tmp/external_v2rayvmessws
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_v2rayvmessws root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_v2rayvmessws /etc/v2ray/config.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl daemon-reload; systemctl restart v2ray.service; systemctl enable v2ray.service;"
	echo "${_V2RAY_VMESS_WS_NGINX_CFG}" > /tmp/external_v2rayvmesswsnginx
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_v2rayvmesswsnginx root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_v2rayvmesswsnginx /etc/nginx/conf.d/vmess.conf"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl restart nginx.service; systemctl enable nginx.service;"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "python3 /opt/tools/conf2vmess.py -c /etc/v2ray/config.json -s $tempHost -p $tempPort -o /opt/tools/output-vmess.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "python3 /opt/tools/vmess2sub.py /opt/tools/output-vmess.json /opt/tools/output-vmess_v2rayN.html -l /opt/tools/output-vmess_v2rayN.lnk"
	_vmessurl=$(fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "cat /opt/tools/output-vmess_v2rayN.lnk")
	echo ""
	echo "> Your VMESS url is as following inport it to your client device"
	echo "$_vmessurl"
}
# End of Function fncSetupExternalV2rayVmessWs

# Function fncSetupExternalTrojan
# Setup external host with Trojan
function fncSetupExternalTrojan {
	tempHost="NULL"
	tempPort="NULL"
	if [[ "$_BOTH_OR_EXTERNAL" == "external" ]]; then
		tempHost="$_EXTERNAL_IP"
		tempPort="$_EXTERNAL_VPN_PORT"
	elif [[ "$_BOTH_OR_EXTERNAL" == "both" ]]; then
		tempHost="$_INTERNAL_IP"
		tempPort="$_INTERNAL_VPN_PORT"
	fi
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "openssl req -new -newkey rsa:4096 -days 365 -nodes -x509 -subj \"/C=US/ST=Denial/L=Springfield/O=Dis/CN=www.sudoer.online\" -keyout /etc/trojan/ssl.key  -out /etc/trojan/ssl.cert"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "chmod 644 /etc/trojan/ssl.*"
	echo "${_TROJAN_CFG}" > /tmp/external_trojan
	scp -r -P "$_EXTERNAL_SSH_PORT" /tmp/external_trojan root@"$_EXTERNAL_IP":/root/
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "mv /root/external_trojan /etc/trojan/config.json"
	fncExecCmd "$_EXTERNAL_IP" "$_EXTERNAL_SSH_PORT" "systemctl restart trojan.service; systemctl enable trojan.service;"
	echo "
 	server: $tempHost
 	server_port: $tempPort
 	password: $_pass
 "
}
# End of Function fncSetupExternalTrojan
