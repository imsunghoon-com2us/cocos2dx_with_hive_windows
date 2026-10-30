# Copies the Hive SDK (Windows, x64) runtime next to DodgeGame.exe. Safe to re-run: only changed files are copied.
#   tools\setup_hive.ps1 [-Config Debug|Release] [-HiveRoot <dir>] [-Target <dir>] [-AppId <id>] [-Zone real|test|sandbox]
#
# AppId: written into <exe dir>\Resources\hive_config.xml after the copy (the SDK package itself is never modified).
#   Resolution order: -AppId, env HIVE_APP_ID, "appId" in <repo>\hive.local.json (git-ignored). If none is set the SDK
#   sample value is kept. Every run re-copies the sample config first, so the value is applied again each time.
# Zone: same mechanism for <zone> (-Zone, then "zone" in hive.local.json; NOT the env var HIVE_ZONE, which the game uses
#   with the native names SANDBOX|REAL at runtime). The SDK sample uses "test"; the config editor writes "real"/"sandbox".
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
    [string]$Target,
    [string]$AppId,
    [string]$Zone
)
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Target) { $Target = Join-Path $RepoRoot "DodgeGame\build-x64\bin\DodgeGame\$Config" }

if (-not $AppId) { $AppId = $env:HIVE_APP_ID }
if (-not $AppId) {
    $localSettings = Join-Path $RepoRoot 'hive.local.json'
    if (Test-Path $localSettings) { $AppId = [string](Get-Content $localSettings -Raw | ConvertFrom-Json).appId }
}
if ($AppId -and $AppId -notmatch '^[A-Za-z0-9._-]+$') { throw "invalid AppId (allowed: letters, digits, . _ -): $AppId" }

if (-not $Zone) {
    $localSettings = Join-Path $RepoRoot 'hive.local.json'
    if (Test-Path $localSettings) { $Zone = [string](Get-Content $localSettings -Raw | ConvertFrom-Json).zone }
}
if ($Zone) {
    $Zone = $Zone.Trim().ToLowerInvariant()
    if ($Zone -notin @('real', 'test', 'sandbox')) { throw "invalid Zone '$Zone' (allowed: real, test, sandbox)" }
}

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

# Apply AppId / Zone to the copied config: only the active element line is replaced (commented alternatives start with "<!--").
# Each value was validated above (letters/digits/._- or real|test|sandbox), so it is safe to use in the replacement text.
if ($AppId -or $Zone) {
    $cfgPath = Join-Path $Target 'Resources\hive_config.xml'
    if (-not (Test-Path $cfgPath)) { throw "hive_config.xml missing after copy: $cfgPath" }
    $bytes = [IO.File]::ReadAllBytes($cfgPath)
    $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    $xml = [Text.Encoding]::UTF8.GetString($bytes, $(if ($hasBom) { 3 } else { 0 }), $bytes.Length - $(if ($hasBom) { 3 } else { 0 }))
    $applied = @()
    foreach ($item in @(@{ Tag = 'appId'; Value = $AppId }, @{ Tag = 'zone'; Value = $Zone })) {
        if (-not $item.Value) { continue }
        $re = "(?m)^(\s*<$($item.Tag)>)[^<]*(</$($item.Tag)>)"
        if ([regex]::Matches($xml, $re).Count -ne 1) { throw "expected exactly one active <$($item.Tag)> in $cfgPath" }
        $xml = [regex]::Replace($xml, $re, ('${1}' + $item.Value + '${2}'))
        $applied += "$($item.Tag)=$($item.Value)"
    }
    [IO.File]::WriteAllText($cfgPath, $xml, (New-Object Text.UTF8Encoding($hasBom)))
    Write-Host "Config     : $($applied -join ', ') (applied to Resources\hive_config.xml)"
}

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
