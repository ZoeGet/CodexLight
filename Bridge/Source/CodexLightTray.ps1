param(
  [string]$Python = "",
  [string]$WorkDir = "",
  [ValidateSet("AUTO", "WIRED", "WIRELESS")]
  [string]$ConnectionMode = "WIRELESS",
  [string]$SerialPort = "auto",
  [int]$SerialBaud = 115200,
  [int]$UdpPort = 4210
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$createdNew = $false
$singleInstanceMutex = New-Object System.Threading.Mutex($true, "Local\CodexLightTray", [ref]$createdNew)
if (-not $createdNew) {
  $singleInstanceMutex.Dispose()
  exit 0
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($WorkDir)) {
  $WorkDir = Split-Path -Parent $scriptDir
}

$monitorScript = Join-Path $scriptDir "codex_light_monitor.py"
$monitorConfig = Join-Path $scriptDir "config.local.json"
$logDir = Join-Path $scriptDir "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stdoutLog = Join-Path $logDir "codex_light_monitor.out.log"
$stderrLog = Join-Path $logDir "codex_light_monitor.err.log"
$wifiSetupOutLog = Join-Path $logDir "wifi_setup.out.log"
$wifiSetupErrLog = Join-Path $logDir "wifi_setup.err.log"

function Get-RecentLogText {
  param([string[]]$Paths)

  $lines = New-Object System.Collections.Generic.List[string]
  foreach ($path in $Paths) {
    if (Test-Path $path) {
      $lines.Add("--- $path ---")
      foreach ($line in (Get-Content -LiteralPath $path -Tail 20 -ErrorAction SilentlyContinue)) {
        $lines.Add($line)
      }
    }
  }

  if ($lines.Count -eq 0) {
    return "No setup log was written."
  }
  return ($lines -join [Environment]::NewLine)
}

function Resolve-PythonPath {
  param([string]$Requested)

  if (-not [string]::IsNullOrWhiteSpace($Requested)) {
    return $Requested
  }

  $pythonCmd = Get-Command python -ErrorAction SilentlyContinue
  if ($pythonCmd -and $pythonCmd.Source) {
    $pythonw = Join-Path (Split-Path -Parent $pythonCmd.Source) "pythonw.exe"
    if (Test-Path $pythonw) {
      return $pythonw
    }
    return $pythonCmd.Source
  }

  $pythonwCmd = Get-Command pythonw -ErrorAction SilentlyContinue
  if ($pythonwCmd -and $pythonwCmd.Source) {
    return $pythonwCmd.Source
  }

  throw "Python was not found. Install Python or pass -Python C:\path\to\pythonw.exe."
}

$pythonPath = Resolve-PythonPath -Requested $Python
$monitorProcess = $null
$currentMode = $ConnectionMode
$wifiSetupState = $null

function Start-BridgeProcess {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$ProcessArguments,
    [Parameter(Mandatory = $true)]
    [string]$OutputLog,
    [Parameter(Mandatory = $true)]
    [string]$ErrorLog
  )

  $argumentLine = ($ProcessArguments | ForEach-Object {
    $value = [string]$_
    if ($value -match '[\s"]') {
      return '"' + $value.Replace('"', '\"') + '"'
    }
    return $value
  }) -join " "

  $startParameters = @{
    FilePath = $pythonPath
    ArgumentList = $argumentLine
    WorkingDirectory = $WorkDir
    WindowStyle = "Hidden"
    PassThru = $true
    RedirectStandardOutput = $OutputLog
    RedirectStandardError = $ErrorLog
  }
  return Start-Process @startParameters
}

function Stop-BridgeProcess {
  param([System.Diagnostics.Process]$Process)

  if (-not $Process) {
    return
  }
  try {
    $Process.Refresh()
    if ($Process.HasExited) {
      return
    }
    $Process.Kill()
    $Process.WaitForExit(3000) | Out-Null
  } catch {
    # Process may already be gone.
  }
}

function Complete-WifiSetup {
  param([bool]$TimedOut = $false)

  $state = $script:wifiSetupState
  if (-not $state -or $state.Completed) {
    return
  }

  $state.Completed = $true
  $state.Timer.Stop()

  $processExited = $false
  try {
    $state.Process.Refresh()
    $processExited = $state.Process.HasExited
  } catch {
    $processExited = $true
  }

  if ($TimedOut -and -not $processExited) {
    Stop-BridgeProcess -Process $state.Process
    Add-Content -LiteralPath $wifiSetupErrLog -Value "WIFI_SETUP_ERROR TRAY_TIMEOUT" -ErrorAction SilentlyContinue
  }

  $succeeded = $false
  try {
    $state.Process.Refresh()
    $succeeded = $state.Process.HasExited -and $state.Process.ExitCode -eq 0
  } catch {
    $succeeded = $false
  }

  Remove-Item -LiteralPath $state.ConfigPath -Force -ErrorAction SilentlyContinue
  try {
    Start-Monitor
  } catch {
    Add-Content -LiteralPath $wifiSetupErrLog -Value ("MONITOR_RESTART_ERROR " + $_.Exception.Message) -ErrorAction SilentlyContinue
  }

  $state.SaveButton.Enabled = $true
  $state.CancelButton.Enabled = $true
  $script:wifiSetupState = $null

  if ($succeeded) {
    [System.Windows.Forms.MessageBox]::Show("WiFi saved and connected.", "CodexLight WiFi", "OK", "Information") | Out-Null
    $state.Form.Close()
    return
  }

  $details = Get-RecentLogText -Paths @($wifiSetupOutLog, $wifiSetupErrLog)
  [System.Windows.Forms.MessageBox]::Show("WiFi setup failed." + [Environment]::NewLine + [Environment]::NewLine + $details, "CodexLight WiFi", "OK", "Error") | Out-Null
  $state.StatusLabel.Text = if ($TimedOut) { "Setup timed out." } else { "Setup failed." }
}

function Invoke-WifiSetup {
  $form = New-Object System.Windows.Forms.Form
  $form.Text = "CodexLight WiFi"
  $form.StartPosition = "CenterScreen"
  $form.FormBorderStyle = "FixedDialog"
  $form.MaximizeBox = $false
  $form.MinimizeBox = $false
  $form.ClientSize = New-Object System.Drawing.Size(360, 170)

  $ssidLabel = New-Object System.Windows.Forms.Label
  $ssidLabel.Text = "SSID"
  $ssidLabel.Location = New-Object System.Drawing.Point(16, 18)
  $ssidLabel.Size = New-Object System.Drawing.Size(80, 22)
  [void]$form.Controls.Add($ssidLabel)

  $ssidBox = New-Object System.Windows.Forms.TextBox
  $ssidBox.Location = New-Object System.Drawing.Point(104, 16)
  $ssidBox.Size = New-Object System.Drawing.Size(236, 24)
  [void]$form.Controls.Add($ssidBox)

  $passwordLabel = New-Object System.Windows.Forms.Label
  $passwordLabel.Text = "Password"
  $passwordLabel.Location = New-Object System.Drawing.Point(16, 56)
  $passwordLabel.Size = New-Object System.Drawing.Size(80, 22)
  [void]$form.Controls.Add($passwordLabel)

  $passwordBox = New-Object System.Windows.Forms.TextBox
  $passwordBox.Location = New-Object System.Drawing.Point(104, 54)
  $passwordBox.Size = New-Object System.Drawing.Size(236, 24)
  $passwordBox.UseSystemPasswordChar = $true
  [void]$form.Controls.Add($passwordBox)

  $statusLabel = New-Object System.Windows.Forms.Label
  $statusLabel.Text = "USB must be connected."
  $statusLabel.Location = New-Object System.Drawing.Point(16, 92)
  $statusLabel.Size = New-Object System.Drawing.Size(324, 22)
  [void]$form.Controls.Add($statusLabel)

  $saveButton = New-Object System.Windows.Forms.Button
  $saveButton.Text = "Save"
  $saveButton.Location = New-Object System.Drawing.Point(184, 126)
  $saveButton.Size = New-Object System.Drawing.Size(76, 28)
  [void]$form.Controls.Add($saveButton)

  $cancelButton = New-Object System.Windows.Forms.Button
  $cancelButton.Text = "Cancel"
  $cancelButton.Location = New-Object System.Drawing.Point(264, 126)
  $cancelButton.Size = New-Object System.Drawing.Size(76, 28)
  $cancelButton.Add_Click({ $form.Close() })
  [void]$form.Controls.Add($cancelButton)

  $setupTimer = New-Object System.Windows.Forms.Timer
  $setupTimer.Interval = 250
  $setupTimer.Add_Tick({
    $state = $script:wifiSetupState
    if (-not $state -or $state.Completed) {
      return
    }

    try {
      $state.Process.Refresh()
      if ($state.Process.HasExited) {
        Complete-WifiSetup
        return
      }
    } catch {
      Complete-WifiSetup
      return
    }

    $elapsed = [Math]::Max(0, [int]((Get-Date) - $state.StartedAt).TotalSeconds)
    $state.StatusLabel.Text = "Connecting... ${elapsed}s"
    if ((Get-Date) -ge $state.Deadline) {
      Complete-WifiSetup -TimedOut $true
    }
  })

  $form.Add_FormClosing({
    param($sender, $eventArgs)
    if ($script:wifiSetupState -and -not $script:wifiSetupState.Completed) {
      $eventArgs.Cancel = $true
      $statusLabel.Text = "Connection is still being checked..."
    }
  })

  $form.Add_FormClosed({
    $setupTimer.Stop()
    $setupTimer.Dispose()
  })

  $saveButton.Add_Click({
    $ssid = $ssidBox.Text.Trim()
    $password = $passwordBox.Text
    if ([string]::IsNullOrWhiteSpace($ssid) -or $ssid.Length -gt 32 -or $password.Length -gt 64 -or ($password.Length -gt 0 -and $password.Length -lt 8)) {
      [System.Windows.Forms.MessageBox]::Show("SSID or password format is invalid.", "CodexLight WiFi", "OK", "Warning") | Out-Null
      return
    }

    $statusLabel.Text = "Opening USB connection..."
    $saveButton.Enabled = $false
    $cancelButton.Enabled = $false

    $wifiConfigPath = Join-Path $env:TEMP ("codexlight_wifi_{0}.json" -f ([Guid]::NewGuid().ToString("N")))
    try {
      Stop-Monitor
      Remove-Item -LiteralPath $wifiSetupOutLog, $wifiSetupErrLog -Force -ErrorAction SilentlyContinue

      @{ ssid = $ssid; password = $password } |
        ConvertTo-Json -Compress |
        Set-Content -LiteralPath $wifiConfigPath -Encoding UTF8

      $args = @(
        $monitorScript,
        "--config", $monitorConfig,
        "--serial", $SerialPort,
        "--baud", $SerialBaud.ToString(),
        "--wifi-config", $wifiConfigPath
      )

      $process = Start-BridgeProcess `
        -ProcessArguments $args `
        -OutputLog $wifiSetupOutLog `
        -ErrorLog $wifiSetupErrLog

      $script:wifiSetupState = [pscustomobject]@{
        Form = $form
        StatusLabel = $statusLabel
        SaveButton = $saveButton
        CancelButton = $cancelButton
        Timer = $setupTimer
        Process = $process
        ConfigPath = $wifiConfigPath
        StartedAt = Get-Date
        Deadline = (Get-Date).AddSeconds(60)
        Completed = $false
      }
      $setupTimer.Start()
    } catch {
      Remove-Item -LiteralPath $wifiConfigPath -Force -ErrorAction SilentlyContinue
      Add-Content -LiteralPath $wifiSetupErrLog -Value ("WIFI_SETUP_ERROR TRAY_START_FAILED " + $_.Exception.Message) -ErrorAction SilentlyContinue
      try {
        Start-Monitor
      } catch {
        Add-Content -LiteralPath $wifiSetupErrLog -Value ("MONITOR_RESTART_ERROR " + $_.Exception.Message) -ErrorAction SilentlyContinue
      }
      $saveButton.Enabled = $true
      $cancelButton.Enabled = $true
      $statusLabel.Text = "Setup could not start."
      $details = Get-RecentLogText -Paths @($wifiSetupOutLog, $wifiSetupErrLog)
      [System.Windows.Forms.MessageBox]::Show("WiFi setup failed." + [Environment]::NewLine + [Environment]::NewLine + $details, "CodexLight WiFi", "OK", "Error") | Out-Null
    }
  })

  [void]$form.ShowDialog()
}

function Get-MonitorArguments {
  switch ($script:currentMode) {
    "WIRED" {
      return @(
        "--serial", $SerialPort,
        "--baud", $SerialBaud.ToString(),
        "--firmware-mode", "WIRED"
      )
    }
    "WIRELESS" {
      return @(
        "--serial", $SerialPort,
        "--baud", $SerialBaud.ToString(),
        "--udp", "--udp-port", $UdpPort.ToString(),
        "--firmware-mode", "WIRELESS",
        "--serial-setup-only",
        "--reset-on-connect"
      )
    }
    default {
      return @(
        "--serial", $SerialPort,
        "--baud", $SerialBaud.ToString(),
        "--udp", "--udp-port", $UdpPort.ToString(),
        "--firmware-mode", "AUTO"
      )
    }
  }
}

function Start-Monitor {
  if ($script:monitorProcess -and -not $script:monitorProcess.HasExited) {
    return
  }

  $args = New-Object System.Collections.Generic.List[string]
  $args.Add($monitorScript)
  $args.Add("--config")
  $args.Add($monitorConfig)
  foreach ($arg in (Get-MonitorArguments)) {
    $args.Add($arg)
  }

  $script:monitorProcess = Start-BridgeProcess `
    -ProcessArguments $args.ToArray() `
    -OutputLog $stdoutLog `
    -ErrorLog $stderrLog
}

function Stop-Monitor {
  if ($script:monitorProcess -and -not $script:monitorProcess.HasExited) {
    Stop-BridgeProcess -Process $script:monitorProcess
  }
  $script:monitorProcess = $null
}

function Restart-Monitor {
  Stop-Monitor
  Start-Monitor
}

function Update-ModeDisplay {
  $notifyIcon.Text = "CodexLight Bridge ($script:currentMode)"
  $statusItem.Text = "CodexLight Bridge: $script:currentMode"
  $autoModeItem.Checked = $script:currentMode -eq "AUTO"
  $wiredModeItem.Checked = $script:currentMode -eq "WIRED"
  $wirelessModeItem.Checked = $script:currentMode -eq "WIRELESS"
}

function Set-ConnectionMode {
  param([ValidateSet("AUTO", "WIRED", "WIRELESS")][string]$Mode)

  if ($script:currentMode -eq $Mode) {
    return
  }
  $script:currentMode = $Mode
  Update-ModeDisplay
  Restart-Monitor
}

$notifyIcon = New-Object System.Windows.Forms.NotifyIcon
$notifyIcon.Icon = [System.Drawing.SystemIcons]::Application
$notifyIcon.Visible = $true

$menu = New-Object System.Windows.Forms.ContextMenuStrip

$statusItem = New-Object System.Windows.Forms.ToolStripMenuItem
$statusItem.Enabled = $false
[void]$menu.Items.Add($statusItem)

$modeMenu = New-Object System.Windows.Forms.ToolStripMenuItem
$modeMenu.Text = "Connection mode"

$autoModeItem = New-Object System.Windows.Forms.ToolStripMenuItem
$autoModeItem.Text = "Auto (wired + wireless)"
$autoModeItem.Add_Click({ Set-ConnectionMode -Mode "AUTO" })
[void]$modeMenu.DropDownItems.Add($autoModeItem)

$wiredModeItem = New-Object System.Windows.Forms.ToolStripMenuItem
$wiredModeItem.Text = "Wired only"
$wiredModeItem.Add_Click({ Set-ConnectionMode -Mode "WIRED" })
[void]$modeMenu.DropDownItems.Add($wiredModeItem)

$wirelessModeItem = New-Object System.Windows.Forms.ToolStripMenuItem
$wirelessModeItem.Text = "Wireless only"
$wirelessModeItem.Add_Click({ Set-ConnectionMode -Mode "WIRELESS" })
[void]$modeMenu.DropDownItems.Add($wirelessModeItem)

[void]$menu.Items.Add($modeMenu)

$wifiSetupItem = New-Object System.Windows.Forms.ToolStripMenuItem
$wifiSetupItem.Text = "Configure WiFi"
$wifiSetupItem.Add_Click({ Invoke-WifiSetup })
[void]$menu.Items.Add($wifiSetupItem)

$openLogItem = New-Object System.Windows.Forms.ToolStripMenuItem
$openLogItem.Text = "Open log folder"
$openLogItem.Add_Click({ Start-Process explorer.exe $logDir })
[void]$menu.Items.Add($openLogItem)

$restartItem = New-Object System.Windows.Forms.ToolStripMenuItem
$restartItem.Text = "Restart monitor"
$restartItem.Add_Click({ Restart-Monitor })
[void]$menu.Items.Add($restartItem)

[void]$menu.Items.Add((New-Object System.Windows.Forms.ToolStripSeparator))

$exitItem = New-Object System.Windows.Forms.ToolStripMenuItem
$exitItem.Text = "Exit"
$exitItem.Add_Click({
  Stop-Monitor
  $notifyIcon.Visible = $false
  $notifyIcon.Dispose()
  [System.Windows.Forms.Application]::Exit()
})
[void]$menu.Items.Add($exitItem)

$notifyIcon.ContextMenuStrip = $menu
$notifyIcon.Add_DoubleClick({ Start-Process explorer.exe $logDir })

try {
  Update-ModeDisplay
  Start-Monitor
  [System.Windows.Forms.Application]::Run()
} finally {
  $wifiSetupRunning = $false
  if ($script:wifiSetupState) {
    try {
      $wifiSetupRunning = -not $script:wifiSetupState.Process.HasExited
    } catch {
      $wifiSetupRunning = $false
    }
  }
  if ($wifiSetupRunning) {
    Stop-BridgeProcess -Process $script:wifiSetupState.Process
    Remove-Item -LiteralPath $script:wifiSetupState.ConfigPath -Force -ErrorAction SilentlyContinue
  }
  Stop-Monitor
  if ($createdNew) {
    $singleInstanceMutex.ReleaseMutex()
  }
  $singleInstanceMutex.Dispose()
}
