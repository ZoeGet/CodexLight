param(
  [string]$Python = "",
  [string]$WorkDir = "",
  [ValidateSet("AUTO", "WIRED", "WIRELESS")]
  [string]$ConnectionMode = "AUTO",
  [string]$SerialPort = "auto",
  [int]$SerialBaud = 115200,
  [int]$UdpPort = 4210
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($WorkDir)) {
  $WorkDir = Split-Path -Parent $scriptDir
}

$monitorScript = Join-Path $scriptDir "codex_light_monitor.py"
$logDir = Join-Path $scriptDir "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stdoutLog = Join-Path $logDir "codex_light_monitor.out.log"
$stderrLog = Join-Path $logDir "codex_light_monitor.err.log"

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
        "--serial-setup-only"
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
  foreach ($arg in (Get-MonitorArguments)) {
    $args.Add($arg)
  }

  $script:monitorProcess = Start-Process `
    -FilePath $pythonPath `
    -ArgumentList $args.ToArray() `
    -WorkingDirectory $WorkDir `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $stderrLog `
    -PassThru
}

function Stop-Monitor {
  if ($script:monitorProcess -and -not $script:monitorProcess.HasExited) {
    try {
      $script:monitorProcess.Kill()
      $script:monitorProcess.WaitForExit(3000) | Out-Null
    } catch {
      # Process may already be gone.
    }
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

Update-ModeDisplay
Start-Monitor

[System.Windows.Forms.Application]::Run()
