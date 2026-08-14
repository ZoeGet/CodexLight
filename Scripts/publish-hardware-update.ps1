[CmdletBinding()]
param(
  [ValidatePattern('^(build|chore|ci|docs|feat|fix|perf|refactor|revert|style|test)(\([a-z0-9._-]+\))?!?: .+$')]
  [string]$CommitMessage = 'feat(hardware): refresh PCB production files',

  [switch]$DryRun,

  [switch]$Yes
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Git {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$Arguments
  )

  & git @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
  }
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$publishPaths = @(
  '.gitignore'
  'Scripts/publish-hardware-update.ps1'
  'Hardware/BOM/BOM.xls'
  'Hardware/BOM/BOM.xlsx'
  'Hardware/PCB/Assembly/CodexLight_PCB_CPL.csv'
  'Hardware/PCB/Gerber/CodexLight_PCB_Gerber.zip'
  'Hardware/PCB/Source/CodexLight.epro2'
)

Push-Location $repositoryRoot
try {
  $actualRoot = (& git rev-parse --show-toplevel).Trim()
  if ($LASTEXITCODE -ne 0) {
    throw 'This script must be run inside the CodexLight Git repository.'
  }
  if ([System.IO.Path]::GetFullPath($actualRoot) -ne $repositoryRoot) {
    throw "Unexpected repository root: $actualRoot"
  }

  $branch = (& git branch --show-current).Trim()
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
    throw 'Unable to determine the current Git branch.'
  }
  if ($branch -ne 'main') {
    throw "Current branch is '$branch'. Switch to 'main' before publishing this hardware update."
  }

  & git remote get-url origin *> $null
  if ($LASTEXITCODE -ne 0) {
    throw "Git remote 'origin' is not configured."
  }

  & git diff --cached --quiet
  if ($LASTEXITCODE -eq 1) {
    throw 'The index already contains staged changes. Commit or unstage them before running this script.'
  }
  if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect the Git index.'
  }

  $statusLines = @(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all)
  if ($LASTEXITCODE -ne 0) {
    throw 'Unable to inspect working-tree changes.'
  }

  $unexpectedChanges = @()
  foreach ($line in $statusLines) {
    if ($line.Length -lt 4) {
      continue
    }

    $path = $line.Substring(3).Replace('\', '/')
    if ($publishPaths -notcontains $path) {
      $unexpectedChanges += $line
    }
  }

  if ($unexpectedChanges.Count -gt 0) {
    Write-Host 'Unexpected working-tree changes:' -ForegroundColor Yellow
    $unexpectedChanges | ForEach-Object { Write-Host "  $_" }
    throw 'Only the intended hardware update may be present when this script runs.'
  }

  if ($DryRun) {
    Write-Host 'Dry run passed. Intended changes:' -ForegroundColor Green
    $statusLines | ForEach-Object { Write-Host "  $_" }
    Write-Host "Commit: $CommitMessage" -ForegroundColor Green
    Write-Host 'No files were staged, committed, rebased, or pushed.'
    return
  }

  Write-Host "Fetching origin/$branch..." -ForegroundColor Cyan
  Invoke-Git @('fetch', 'origin', $branch)

  Write-Host 'Staging the hardware release files...' -ForegroundColor Cyan
  Invoke-Git (@('add', '--') + $publishPaths)

  & git diff --cached --quiet
  if ($LASTEXITCODE -eq 0) {
    throw 'There are no hardware changes to commit.'
  }
  if ($LASTEXITCODE -ne 1) {
    throw 'Unable to inspect the staged changes.'
  }

  Invoke-Git @('diff', '--cached', '--check')

  Write-Host ''
  Write-Host 'Staged changes:' -ForegroundColor Green
  Invoke-Git @('-c', 'core.quotepath=false', 'diff', '--cached', '--name-status')
  Invoke-Git @('diff', '--cached', '--stat')
  Write-Host ''
  Write-Host "Commit: $CommitMessage" -ForegroundColor Green

  if (-not $Yes) {
    $confirmation = Read-Host 'Create this commit, rebase onto origin/main, and push? [y/N]'
    if ($confirmation -notmatch '^(y|yes)$') {
      throw 'Publishing cancelled. The files remain staged for review.'
    }
  }

  Invoke-Git @(
    'commit',
    '-m', $CommitMessage,
    '-m', 'Replace the legacy XLS BOM with the updated XLSX export.',
    '-m', 'Refresh the CPL, Gerber, and EasyEDA source files for the ETA6093S2F hardware revision.',
    '-m', 'Remove the obsolete BOM and CPL warning from the EasyEDA project description.'
  )

  Write-Host "Rebasing onto origin/$branch..." -ForegroundColor Cyan
  Invoke-Git @('pull', '--rebase', 'origin', $branch)

  Write-Host "Pushing to origin/$branch..." -ForegroundColor Cyan
  Invoke-Git @('push', 'origin', $branch)

  Write-Host ''
  Write-Host 'Published successfully:' -ForegroundColor Green
  Invoke-Git @('--no-pager', 'log', '-1', '--oneline', '--decorate')
}
finally {
  Pop-Location
}
