; extra.nsh

!macro customInstall
  ; ── Register nipovpn:// URI scheme ──
  WriteRegStr HKCR "nipovpn" "" "URL:NipoVPN Protocol"
  WriteRegStr HKCR "nipovpn" "URL Protocol" ""
  WriteRegStr HKCR "nipovpn\shell\open\command" "" '"$INSTDIR\NipoVPN.exe" "--link=%1"'

  ; ── Auto-start with Windows (system tray) ──
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "NipoVPN" '"$INSTDIR\NipoVPN.exe" "--minimized"'

  ; ── Firewall rules (آدرس دقیق فایل در پوشه resources) ──
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NipoVPN"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN" dir=out action=allow program="$INSTDIR\resources\nipovpn-core.exe" enable=yes'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NipoVPN Agent" dir=in action=allow program="$INSTDIR\resources\nipovpn-core.exe" protocol=tcp localport=8080 enable=yes'
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
!macroend
