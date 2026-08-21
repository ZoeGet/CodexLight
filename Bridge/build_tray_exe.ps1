[CmdletBinding()]
param(
  [string]$Python = "python",
  [string]$OutputDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$bridgeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Split-Path -Parent $bridgeDir
$buildRoot = Join-Path $repositoryRoot "tmp\tray-exe-build"
$runtimeDir = Join-Path $buildRoot "python-runtime"
$runtimeZip = Join-Path $buildRoot "python-runtime.zip"
$iconPath = Join-Path $buildRoot "CodexLight.ico"
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
  $OutputDirectory = Join-Path $bridgeDir "dist"
}

function Remove-BuildDirectory {
  param([string]$Path)

  $resolvedBuildRoot = [System.IO.Path]::GetFullPath($buildRoot)
  $resolvedPath = [System.IO.Path]::GetFullPath($Path)
  if (-not $resolvedPath.StartsWith($resolvedBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected build path: $resolvedPath"
  }
  if (Test-Path -LiteralPath $resolvedPath) {
    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
  }
}

Remove-BuildDirectory -Path $runtimeDir
New-Item -ItemType Directory -Force -Path $buildRoot, $runtimeDir, $OutputDirectory | Out-Null
Remove-Item -LiteralPath $runtimeZip -Force -ErrorAction SilentlyContinue

$pythonRoot = (& $Python -c "import sys; print(sys.base_prefix)").Trim()
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pythonRoot)) {
  throw "Unable to locate the Python installation."
}
$pythonBits = (& $Python -c "import struct; print(struct.calcsize('P') * 8)").Trim()
if ($pythonBits -ne "64") {
  throw "The packaged tray currently requires a 64-bit Python build."
}
$pythonDllName = (& $Python -c "import sys; print(f'python{sys.version_info.major}{sys.version_info.minor}.dll')").Trim()

foreach ($name in @("python.exe", "pythonw.exe", "python3.dll", $pythonDllName, "vcruntime140.dll", "vcruntime140_1.dll")) {
  $source = Join-Path $pythonRoot $name
  if (Test-Path -LiteralPath $source) {
    Copy-Item -LiteralPath $source -Destination $runtimeDir
  }
}
Copy-Item -LiteralPath (Join-Path $pythonRoot "DLLs") -Destination $runtimeDir -Recurse

$runtimeLib = Join-Path $runtimeDir "Lib"
New-Item -ItemType Directory -Force -Path $runtimeLib | Out-Null
$excludedLibDirectories = @(
  "__pycache__",
  "distutils",
  "ensurepip",
  "idlelib",
  "lib2to3",
  "site-packages",
  "test",
  "tkinter",
  "turtledemo",
  "venv"
)
Get-ChildItem -LiteralPath (Join-Path $pythonRoot "Lib") -Force | ForEach-Object {
  if ($_.PSIsContainer -and $excludedLibDirectories -contains $_.Name) {
    return
  }
  Copy-Item -LiteralPath $_.FullName -Destination $runtimeLib -Recurse
}

$sitePackages = Join-Path $runtimeLib "site-packages"
New-Item -ItemType Directory -Force -Path $sitePackages | Out-Null
$serialPackage = (& $Python -c "import pathlib, serial; print(pathlib.Path(serial.__file__).parent)").Trim()
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $serialPackage)) {
  throw "pyserial is required to build the standalone tray executable."
}
Copy-Item -LiteralPath $serialPackage -Destination $sitePackages -Recurse

$runtimePython = Join-Path $runtimeDir "python.exe"
& $runtimePython -I -c "import argparse, ipaddress, json, serial, socket, sqlite3; print('portable runtime OK')"
if ($LASTEXITCODE -ne 0) {
  throw "The portable Python runtime validation failed."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
  $runtimeDir,
  $runtimeZip,
  [System.IO.Compression.CompressionLevel]::Optimal,
  $false
)

Add-Type -AssemblyName System.Drawing
$iconStream = [System.IO.File]::Create($iconPath)
try {
  [System.Drawing.SystemIcons]::Application.Save($iconStream)
} finally {
  $iconStream.Dispose()
}

$csc = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path -LiteralPath $csc)) {
  throw "The Windows .NET Framework C# compiler was not found."
}

$outputExe = Join-Path $OutputDirectory "CodexLightTray.exe"
$launcherSource = Join-Path $bridgeDir "CodexLightLauncher.cs"
$trayScript = Join-Path $bridgeDir "CodexLightTray.ps1"
$monitorScript = Join-Path $bridgeDir "codex_light_monitor.py"
$compilerArguments = @(
  "/nologo",
  "/target:winexe",
  "/platform:x64",
  "/optimize+",
  "/win32icon:$iconPath",
  "/out:$outputExe",
  "/reference:System.dll",
  "/reference:System.Core.dll",
  "/reference:System.Windows.Forms.dll",
  "/reference:System.IO.Compression.dll",
  "/reference:System.IO.Compression.FileSystem.dll",
  "/resource:$trayScript,CodexLight.TrayScript",
  "/resource:$monitorScript,CodexLight.MonitorScript",
  "/resource:$runtimeZip,CodexLight.PythonRuntime",
  $launcherSource
)
& $csc $compilerArguments
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputExe)) {
  throw "The tray executable build failed."
}

Write-Host "Built successfully: $outputExe" -ForegroundColor Green
