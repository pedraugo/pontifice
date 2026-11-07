# Builds on Windows with MSVC: .\build_win.ps1 [debug|release] [package]
param([string]$Config = "debug", [string]$Action = "")
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if ($env:OS -ne "Windows_NT") { throw "build_win.ps1 only runs on Windows" }
$preset = "windows-$($Config.ToLower())"

# Outside a Developer PowerShell, load the newest Visual Studio with the C++ tools.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) { throw "Visual Studio not found: install 'Desktop development with C++'" }
  $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { throw "Visual Studio without the C++ tools: install 'Desktop development with C++'" }
  Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
  Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
}

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Action -eq "package") {
  cpack --preset $preset
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
