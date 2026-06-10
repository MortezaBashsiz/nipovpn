; extra.nsh — injected into the electron-builder NSIS installer
; nipovpn.exe, DLLs, etc/ and logs/ arrive via extraResources.

!macro customInstall
  ; ── Create required directories ──
  CreateDirectory "$INSTDIR\etc\nipovpn"
  CreateDirectory "$INSTDIR\logs"

  ; ── Create empty log file ──
  FileOpen $0 "$INSTDIR\logs\nipovpn.log" w
  FileClose $0

  ; ── Write Windows-compatible config.yaml ──
  ; Only write if config doesn't already exist (preserve user config on upgrade)
  IfFileExists "$INSTDIR\etc\nipovpn\config.yaml" config_exists config_missing
  config_missing:
    FileOpen $0 "$INSTDIR\etc\nipovpn\config.yaml" w
    FileWrite $0 "general:$\r$\n"
    FileWrite $0 "  token: $\"af445adb-2434-4975-9445-2c1b2231$\"$\r$\n"
    FileWrite $0 "  protocol: http$\r$\n"
    FileWrite $0 "  fakeUrls:$\r$\n"
    FileWrite $0 "    - nipo.ciron.net$\r$\n"
    FileWrite $0 "    - sudoer.ir$\r$\n"
    FileWrite $0 "    - google.com$\r$\n"
    FileWrite $0 "  methods:$\r$\n"
    FileWrite $0 "    - GET$\r$\n"
    FileWrite $0 "    - POST$\r$\n"
    FileWrite $0 "  endPoints:$\r$\n"
    FileWrite $0 "    - api$\r$\n"
    FileWrite $0 "    - login$\r$\n"
    FileWrite $0 "  timeout: 10$\r$\n"
    FileWrite $0 "  pullTimeout: 50$\r$\n"
    FileWrite $0 "  tunnelEnable: false$\r$\n"
    FileWrite $0 "  connectionReuse: true$\r$\n"
    FileWrite $0 "  tlsEnable: false$\r$\n"
    FileWrite $0 "  tlsVerifyPeer: false$\r$\n"
    FileWrite $0 "  tlsCertFile: $\"$INSTDIR\etc\nipovpn\server.crt$\"$\r$\n"
    FileWrite $0 "  tlsKeyFile: $\"$INSTDIR\etc\nipovpn\server.key$\"$\r$\n"
    FileWrite $0 "  tlsCaFile: $\"$\"$\r$\n"
    FileWrite $0 "log:$\r$\n"
    FileWrite $0 "  logLevel: $\"INFO$\"$\r$\n"
    FileWrite $0 "  logFile: $\"$INSTDIR\logs\nipovpn.log$\"$\r$\n"
    FileWrite $0 "server:$\r$\n"
    FileWrite $0 "  threads: 4$\r$\n"
    FileWrite $0 "  listenIp: $\"0.0.0.0$\"$\r$\n"
    FileWrite $0 "  listenPort: 80$\r$\n"
    FileWrite $0 "agent:$\r$\n"
    FileWrite $0 "  threads: 4$\r$\n"
    FileWrite $0 "  listenIp: $\"127.0.0.1$\"$\r$\n"
    FileWrite $0 "  listenPort: 8080$\r$\n"
    FileWrite $0 "  serverIp: $\"127.0.0.10$\"$\r$\n"
    FileWrite $0 "  serverPort: 80$\r$\n"
    FileWrite $0 "  httpVersion: $\"1.1$\"$\r$\n"
    FileWrite $0 "  userAgent: $\"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Gecko/20100101 Firefox/132.0$\"$\r$\n"
    FileClose $0
  config_exists:

  ; ── Register nipovpn:// URI scheme ──
  WriteRegStr HKCR "nipovpn" "" "URL:NipoVPN Protocol"
  WriteRegStr HKCR "nipovpn" "URL Protocol" ""
  WriteRegStr HKCR "nipovpn\shell\open\command" "" '"$INSTDIR\NipoVPN.exe" "--link=%1"'

  ; ── Auto-start with Windows ──
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN" '"$INSTDIR\NipoVPN.exe" "--minimized"'

  ; ── Firewall rules ──
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN" dir=out action=allow program="$INSTDIR\nipovpn.exe" enable=yes'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN Agent" dir=in action=allow program="$INSTDIR\nipovpn.exe" protocol=tcp localport=8080 enable=yes'
!macroend

!macro customUnInstall
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Internet Settings" "ProxyEnable" 0
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN"
  DeleteRegKey HKCR "nipovpn"
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN Agent"'
!macroend
