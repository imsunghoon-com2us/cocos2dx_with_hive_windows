# Copies the Hive SDK (Windows, x64) runtime next to DodgeGame.exe. Safe to re-run: only changed files are copied.
#   tools\setup_hive.ps1 [-Config Debug|Release] [-HiveRoot <dir>] [-Target <dir>]
#
# Layout (mirrors HiveSDKWindows.Build.cs of the Unreal plugin):
#   assets\additional\**              -> <exe dir>\**           (sub folders kept, "hercules" excluded)
#   assets\additional\hivePlugins\hercules\* -> <exe dir>\      (flattened)
#   assets\config\*                   -> <exe dir>\Resources\   (hive_config.xml)
# The SDK package is proprietary and contains keys, so it is never committed (HiveSDKv4_unreal/ is git-ignored).
# Note: the package is large (CEF, about 0.5 GB per configuration).
# This file is saved as UTF-8 with BOM (Windows PowerShell 5.1).
param(
    [ValidateSet('Debug', 'Release')][string]$Config = 'Debug',
    [string]$HiveRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'HiveSDKv4_unreal'),
    [string]$Target
)
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Target) { $Target = Join-Path $RepoRoot "DodgeGame\build-x64\bin\DodgeGame\$Config" }

if (-not (Test-Path $HiveRoot)) { throw "Hive SDK package not found: $HiveRoot (use -HiveRoot)" }
if (-not (Test-Path (Join-Path $Target 'DodgeGame.exe'))) {
    throw "DodgeGame.exe not found in $Target. Build first: tools\build.ps1 -Config $Config -Arch x64"
}

# Find ...\HiveSDKWindows\assets (prefer the plugin copy over the sample project's identical copy).
$assets = Get-ChildItem $HiveRoot -Recurse -Directory -Filter 'assets' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\HiveSDKWindows\\assets$' -and (Test-Path (Join-Path $_.FullName 'additional')) } |
    Sort-Object { if ($_.FullName -match '\\sample\\') { 1 } else { 0 } } |
    Select-Object -First 1
if (-not $assets) { throw "HiveSDKWindows\assets\additional not found under $HiveRoot" }
$assets = $assets.FullName
Write-Host "SDK assets : $assets"
Write-Host "Target     : $Target"

function Sync-Dir($from, $to, $extraArgs) {
    if (-not (Test-Path $from)) { return }
    New-Item -ItemType Directory -Force $to | Out-Null
    $roboArgs = @($from, $to, '/E', '/XF', '*.meta', '/NJH', '/NJS', '/NDL', '/NP', '/R:1', '/W:1') + $extraArgs
    $out = & robocopy @roboArgs
    # robocopy exit codes 0-7 are success (bit flags), 8+ means failures
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $from" }
    $copied = @($out | Where-Object { $_ -match '\S' }).Count
    Write-Host ("  {0,-60} {1} file(s) copied" -f (Split-Path $from -Leaf), $copied)
}

$additional = Join-Path $assets 'additional'
Sync-Dir $additional $Target @('/XD', 'hercules')
Sync-Dir (Join-Path $additional 'hivePlugins\hercules') $Target @()
Sync-Dir (Join-Path $assets 'config') (Join-Path $Target 'Resources') @()

# Verify the essentials and that the entry DLL is x64.
$dll = Join-Path $Target 'HIVE_PLUGIN_CPP.dll'
if (-not (Test-Path $dll)) { throw "HIVE_PLUGIN_CPP.dll missing after copy: $dll" }
$fs = [IO.File]::OpenRead($dll)
try {
    $br = New-Object IO.BinaryReader($fs)
    $fs.Seek(0x3C, 'Begin') | Out-Null
    $pe = $br.ReadInt32()
    $fs.Seek($pe + 4, 'Begin') | Out-Null
    $machine = $br.ReadUInt16()
} finally { $fs.Dispose() }
if ($machine -ne 0x8664) { throw ("HIVE_PLUGIN_CPP.dll is not x64 (machine 0x{0:X})" -f $machine) }
$cfg = Join-Path $Target 'Resources\hive_config.xml'
Write-Host ("OK: HIVE_PLUGIN_CPP.dll (x64), hive_config.xml {0}" -f $(if (Test-Path $cfg) { 'present' } else { 'MISSING' }))
