; extra.nsh — injected into the electron-builder NSIS installer
; Runs after the Electron app files are copied

; ── Copy nipovpn core binary and DLLs ──
!macro customInstall
  ; Source: the nipovpn_windows package extracted next to the installer
  SetOutPath "$INSTDIR"
  File /nonfatal "${BUILD_NIPOVPN_DIR}\nipovpn.exe"
  File /nonfatal /r "${BUILD_NIPOVPN_DIR}\*.dll"

  ; ── Create config dir and default config ──
  CreateDirectory "$INSTDIR\etc\nipovpn"
  CreateDirectory "$INSTDIR\logs"

  ; Write default config.yaml only if not present (preserve user config)
  IfFileExists "$INSTDIR\etc\nipovpn\config.yaml" config_exists config_missing
  config_missing:
    File /oname="$INSTDIR\etc\nipovpn\config.yaml" "${BUILD_NIPOVPN_DIR}\etc\nipovpn\config.yaml"
  config_exists:

  ; Create empty log file
  IfFileExists "$INSTDIR\logs\nipovpn.log" +2
    FileOpen $0 "$INSTDIR\logs\nipovpn.log" w
    FileClose $0

  ; ── Register nipovpn:// URI scheme ──
  WriteRegStr HKCR "nipovpn" "" "URL:NipoVPN Protocol"
  WriteRegStr HKCR "nipovpn" "URL Protocol" ""
  WriteRegStr HKCR "nipovpn\shell\open\command" "" '"$INSTDIR\NipoVPN.exe" "--link=%1"'

  ; ── Auto-start with Windows (system tray) ──
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN" '"$INSTDIR\NipoVPN.exe" "--minimized"'

  ; ── Firewall rule ──
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN" dir=out action=allow program="$INSTDIR\nipovpn.exe" enable=yes'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN Agent" dir=in action=allow program="$INSTDIR\nipovpn.exe" protocol=tcp localport=8080 enable=yes'
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
