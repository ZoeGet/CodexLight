Option Explicit

Dim shell, fso, scriptDir, command, mode

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
mode = "AUTO"
If WScript.Arguments.Count > 0 Then
  mode = UCase(WScript.Arguments(0))
End If

command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File " & _
          Quote(scriptDir & "\CodexLightTray.ps1") & _
          " -ConnectionMode " & Quote(mode) & _
          " -SerialPort auto -SerialBaud 115200 -UdpPort 4210"

shell.Run command, 0, False

Function Quote(value)
  Quote = Chr(34) & value & Chr(34)
End Function
