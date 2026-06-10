; extra.nsh — injected into the electron-builder NSIS installer
; nipovpn-core.exe, DLLs, etc/ and logs/ arrive via extraResources (electron-builder
; copies them into $INSTDIR before this macro runs).
No external path needed.

!macro customInstall
  ; ── Create required directories ──
  CreateDirectory "$INSTDIR\etc\nipovpn"
  CreateDirectory "$INSTDIR\logs"

  ; ── Create empty log file if missing ──
  IfFileExists "$INSTDIR\logs\nipovpn.log" log_exists log_missing
  log_missing:
    FileOpen $0 "$INSTDIR\logs\nipovpn.log" w
    FileClose $0
  log_exists:

  ; ── Register nipovpn:// URI scheme ──
  WriteRegStr HKCR "nipovpn" "" "URL:NipoVPN Protocol"
  WriteRegStr HKCR "nipovpn" "URL Protocol" ""
  WriteRegStr HKCR "nipovpn\shell\open\command" "" '"$INSTDIR\NipoVPN.exe" "--link=%1"'

  ; ── Auto-start with Windows (system tray) ──
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN" '"$INSTDIR\NipoVPN.exe" "--minimized"'

  ; ── Firewall rules ──
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN" dir=out action=allow program="$INSTDIR\nipovpn-core.exe" enable=yes'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN Agent" dir=in action=allow program="$INSTDIR\nipovpn-core.exe" protocol=tcp localport=8080 enable=yes'
!macroend

!macro customUnInstall
  ; ── Remove proxy if set ──
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Internet Settings" "ProxyEnable" 0

  ; ── Remove autostart ──
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN"

  ; ── Remove URI scheme ──
  DeleteRegKey HKCR "nipovpn"

  ; ── Remove firewall rules ──
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN Agent"'

  ; NOTE: config.yaml and logs are NOT deleted (user data preserved)
!macroend
