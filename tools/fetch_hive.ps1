# Downloads the Hive SDK (Windows package for Unreal Engine 5, x64) pinned in tools\hive_sdk.json, verifies its SHA256 and
# unpacks it under HiveSDKv4_unreal\ (git-ignored: proprietary binaries and keys, never commit it).
#   tools\fetch_hive.ps1 [-IncludeInterface] [-Force] [-HiveRoot <dir>]   install the PINNED version (skipped when already installed)
#   tools\fetch_hive.ps1 -CheckLatest                                      only report the newest version on the release page
#   tools\fetch_hive.ps1 -Update [-Version x.y.z] [-IncludeInterface]      fetch a new version and re-pin tools\hive_sdk.json
#
# Policy: the build never follows "latest" by itself. The pinned version and hash make a build reproducible and make a replaced
# package visible. -CheckLatest only tells you; -Update is the single command that changes the pin (review it with git diff,
# then build, run the tests and the real SDK before committing; see docs\10-sha256-pinning.md).
# The interface package (Unreal C++ sources) is only a reference for the API. The build and setup_hive.ps1 need only the Windows package.
# This file is saved as UTF-8 with BOM (Windows PowerShell 5.1).
param(
    [switch]$IncludeInterface,
    [switch]$Force,
    [switch]$CheckLatest,
    [switch]$Update,
    [string]$Version,
    [string]$HiveRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'HiveSDKv4_unreal')
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

if ($CheckLatest -and $Update) { throw '-CheckLatest and -Update cannot be combined' }
if ($Version -and -not $Update) { throw '-Version is only used with -Update' }
if ($Version -and $Version -notmatch '^\d+\.\d+\.\d+$') { throw "invalid -Version '$Version' (expected x.y.z)" }

$Manifest = Join-Path $PSScriptRoot 'hive_sdk.json'
$Tar      = Join-Path $env:SystemRoot 'System32\tar.exe'
$DlDir    = Join-Path $env:TEMP 'hive_sdk_dl'

if (-not (Test-Path $Manifest)) { throw "manifest not found: $Manifest" }
$m = Get-Content $Manifest -Raw | ConvertFrom-Json

# Returns @{ windows = @{ '26.7.0' = <url> ... }; interface = @{ ... } } from the release page, or $null when it cannot be read/parsed.
function Get-ReleaseLinks([string]$Page) {
    try { $html = (Invoke-WebRequest -Uri $Page -UseBasicParsing).Content }
    catch { Write-Warning "release page not readable: $($_.Exception.Message)"; return $null }
    $links = @{ windows = @{}; interface = @{} }
    foreach ($hit in [regex]::Matches($html, 'href="([^"]+\.zip[^"]*)"')) {
        $u = $hit.Groups[1].Value -replace '&amp;', '&'
        if ($u -match 'Hive%20SDK%20v4%20Windows%20UnrealEngine5%20(\d+\.\d+\.\d+)\.zip') { $links.windows[$Matches[1]] = $u }
        elseif ($u -match 'Hive_SDK_v4_UE5_Interface_(\d+\.\d+\.\d+)\.zip') { $links.interface[$Matches[1]] = $u }
    }
    if ($links.windows.Count -eq 0) { Write-Warning 'no Windows SDK link found on the release page (page layout changed?)'; return $null }
    return $links
}

function Get-LatestVersion($Links) {
    return ($Links.windows.Keys | Sort-Object { [version]$_ } -Descending | Select-Object -First 1)
}

function Save-Download([string]$Url, [string]$Dest) {
    New-Item -ItemType Directory -Force (Split-Path $Dest -Parent) | Out-Null
    if (Test-Path $Dest) { Remove-Item $Dest -Force }
    Write-Host "  download $Url"
    Invoke-WebRequest -Uri $Url -OutFile "$Dest.part" -UseBasicParsing
    Move-Item "$Dest.part" $Dest
}

# On mismatch the download is deleted and nothing is installed. There is deliberately no switch to skip this.
function Assert-Sha256([string]$Path, [string]$Expected, [string]$Url) {
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actual -ne $Expected.ToUpperInvariant()) {
        Remove-Item -LiteralPath $Path -Force
        throw ("SHA256 mismatch for $Url`n  expected: $($Expected.ToUpperInvariant())`n  actual  : $actual`n" +
               "  The download was deleted and nothing was installed. Cause: the file behind this URL was replaced " +
               "(Hive may re-upload a package under the same version number) or tampered with. " +
               "Do not just edit the hash: check the release page, then use -Update (see docs\10-sha256-pinning.md).")
    }
}

# Unpack into <folder>.tmp first, then swap it in, so a failed extraction never leaves a half-installed folder.
function Install-Package([string]$Zip, [string]$Folder, [string]$Sha256) {
    if ($Folder -notmatch '^[A-Za-z0-9_. -]+$' -or $Folder -match '\.\.') { throw "unsafe folder name in manifest: '$Folder'" }
    $dest = Join-Path $HiveRoot $Folder
    $tmp  = "$dest.tmp"
    New-Item -ItemType Directory -Force $HiveRoot | Out-Null
    if (Test-Path $tmp) { [IO.Directory]::Delete($tmp, $true) }
    New-Item -ItemType Directory -Force $tmp | Out-Null
    & $Tar -xf $Zip -C $tmp
    if ($LASTEXITCODE -ne 0) { throw "tar failed for $Zip" }
    if (Test-Path $dest) { Write-Host "  replacing existing folder: $dest"; [IO.Directory]::Delete($dest, $true) }
    Move-Item $tmp $dest
    # The marker records which (verified) zip this folder came from.
    [IO.File]::WriteAllText((Join-Path $dest '.fetched'), $Sha256.ToUpperInvariant(), (New-Object Text.UTF8Encoding($false)))
}

# ---------------------------------------------------------------- -CheckLatest
if ($CheckLatest) {
    Write-Host "pinned : $($m.version)"
    $links = Get-ReleaseLinks $m.releasePage
    if (-not $links) { Write-Host 'latest : unknown (could not read the release page; this is not an error)'; return }
    $latest = Get-LatestVersion $links
    Write-Host "latest : $latest ($($m.releasePage))"
    if ([version]$latest -gt [version]$m.version) {
        Write-Host "A newer SDK exists. Nothing was changed. To adopt it: tools\fetch_hive.ps1 -Update (then build, test, review git diff, commit)." -ForegroundColor Yellow
    } elseif ([version]$latest -eq [version]$m.version) { Write-Host 'The pinned version is the latest.' }
    else { Write-Host 'The pinned version is newer than the release page shows (page may list only some versions).' }
    return
}

# ---------------------------------------------------------------- -Update
if ($Update) {
    $links = Get-ReleaseLinks $m.releasePage
    if (-not $links) { throw 'cannot determine the SDK version/links from the release page; nothing changed' }
    $target = if ($Version) { $Version } else { Get-LatestVersion $links }
    if (-not $links.windows.ContainsKey($target)) { throw "Windows SDK $target is not on the release page (available: $(($links.windows.Keys | Sort-Object) -join ', '))" }
    Write-Host "Updating pin: $($m.version) -> $target"

    $packages = [ordered]@{}
    $winUrl = $links.windows[$target]
    $winZip = Join-Path $DlDir "windows-$target.zip"
    Save-Download $winUrl $winZip
    $packages.windows = @{ Url = $winUrl; Zip = $winZip; Folder = "Hive SDK v4 Windows UnrealEngine5 $target" }
    if ($links.interface.ContainsKey($target)) {
        $ifUrl = $links.interface[$target]
        $ifZip = Join-Path $DlDir "interface-$target.zip"
        Save-Download $ifUrl $ifZip
        $packages.interface = @{ Url = $ifUrl; Zip = $ifZip; Folder = "Hive_SDK_v4_UE5_Interface_$target" }
    } else { Write-Warning "no interface package for $target on the release page; the manifest will not pin one" }

    foreach ($name in @($packages.Keys)) {
        $p = $packages[$name]
        $p.Sha256 = (Get-FileHash -LiteralPath $p.Zip -Algorithm SHA256).Hash
        $p.Size   = (Get-Item -LiteralPath $p.Zip).Length
        Write-Host ("  {0,-9} {1} B  sha256 {2}" -f $name, $p.Size, $p.Sha256)
        $old = $m.$name
        if ($old -and $target -eq $m.version -and $old.sha256 -ne $p.Sha256) {
            Write-Warning "SAME VERSION $target but the hash CHANGED for '$name': the package was re-uploaded under the same version number. Review before committing."
        }
    }

    foreach ($name in @($packages.Keys)) {
        $p = $packages[$name]
        Install-Package $p.Zip $p.Folder $p.Sha256
        Remove-Item -LiteralPath $p.Zip -Force
        Write-Host "  installed: $(Join-Path $HiveRoot $p.Folder)"
    }

    function Format-Entry($p) {
        if (-not $p) { return 'null' }
        return @"
{
    "url": "$($p.Url)",
    "sha256": "$($p.Sha256)",
    "size": $($p.Size),
    "folder": "$($p.Folder)"
  }
"@
    }
    $ifEntry = if ($packages.Contains('interface')) { Format-Entry $packages.interface } else { 'null' }
    $json = "{`n  `"version`": `"$target`",`n  `"releasePage`": `"$($m.releasePage)`",`n  `"windows`": $(Format-Entry $packages.windows),`n  `"interface`": $ifEntry`n}`n"
    [IO.File]::WriteAllText($Manifest, $json, (New-Object Text.UTF8Encoding($false)))
    Write-Host "Manifest updated: $Manifest"
    Write-Host 'Next: review "git diff tools/hive_sdk.json", run tools\setup_hive.ps1, build, tests (HiveCocosTests) and a real-SDK run (the terms screen may reappear), then commit.' -ForegroundColor Yellow
    Write-Host 'Older version folders under HiveSDKv4_unreal\ are kept (about 0.6 GB each); delete them by hand when no longer needed.'
    return
}

# ---------------------------------------------------------------- default: install the pinned version
$targets = @(@{ Name = 'windows'; Pkg = $m.windows })
if ($IncludeInterface) {
    if (-not $m.interface) { throw 'the manifest pins no interface package' }
    $targets += @{ Name = 'interface'; Pkg = $m.interface }
}
Write-Host "Hive SDK pinned version: $($m.version)"
foreach ($t in $targets) {
    $pkg = $t.Pkg
    foreach ($field in 'url', 'sha256', 'folder') { if (-not $pkg.$field) { throw "manifest entry '$($t.Name)' has no '$field'" } }
    if ($pkg.sha256 -notmatch '^[0-9A-Fa-f]{64}$') { throw "manifest entry '$($t.Name)' has an invalid sha256" }
    $dest   = Join-Path $HiveRoot $pkg.folder
    $marker = Join-Path $dest '.fetched'
    Write-Host "[$($t.Name)]"
    if (-not $Force -and (Test-Path $marker) -and ((Get-Content $marker -Raw).Trim() -eq $pkg.sha256.ToUpperInvariant())) {
        Write-Host "  skip (already installed and verified): $dest"; continue
    }
    $zip = Join-Path $DlDir "$($t.Name)-$($m.version).zip"
    Save-Download $pkg.url $zip
    Assert-Sha256 $zip $pkg.sha256 $pkg.url
    Write-Host '  SHA256 OK'
    Install-Package $zip $pkg.folder $pkg.sha256
    Remove-Item -LiteralPath $zip -Force
    Write-Host "  installed: $dest"
}
