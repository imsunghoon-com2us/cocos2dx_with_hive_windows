# Hive SDK v4 cocos2d-x Windows 배포판 zip을 만든다(로컬 산출물, git에는 올리지 않는다).
#   tools\package_cocos_windows.ps1 [-SampleRoot <HIVE_SDK_Cocos_Sample>] [-Output <zip 경로>] [-Version 26.7.0]
# 입력은 Android/iOS 배포판 샘플 폴더(HIVE_SDK_Cocos_Sample)에 Windows 파일(proj.win32, PluginSource\Windows)을 더한 것이다.
# 포함: Classes, Resources, PluginSource\Windows, proj.win32 (빌드 산출물 build-x64 제외). v1, c2s, proj.android, proj.ios, PluginSource\Android/iOS는 제외.
# Hive SDK 런타임(DLL)과 엔진은 넣지 않는다(proj.win32\README.md 참고).
# 샘플 폴더에는 Hive 독점 소스와 실제 키가 들어 있으므로 결과 zip도 저장소에 올리지 않는다(.gitignore가 Hive_SDK_v4_Cocos2d-x_*.zip 제외).
# zip 항목 이름은 '/' 구분자를 쓴다(Windows PowerShell 5.1의 Compress-Archive는 '\'를 쓴다).
# 이 파일은 UTF-8 BOM으로 저장해야 한다(Windows PowerShell 5.1).
[CmdletBinding()]
param(
    [string]$SampleRoot,
    [string]$Output,
    [string]$Version = '26.7.0'
)
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $SampleRoot) { $SampleRoot = Join-Path $RepoRoot 'HIVE_SDK_Cocos_Sample' }
if (-not $Output) { $Output = Join-Path $RepoRoot "Hive_SDK_v4_Cocos2d-x_Windows_$Version.zip" }
$SampleRoot = (Resolve-Path $SampleRoot).Path
$rootName = 'HIVE_SDK_Cocos_Sample'

$parts = @('Classes', 'Resources', 'PluginSource\Windows', 'proj.win32')
foreach ($p in $parts) { if (-not (Test-Path (Join-Path $SampleRoot $p))) { throw "missing: $SampleRoot\$p" } }
foreach ($lib in 'Debug', 'Release') {
    if (-not (Test-Path (Join-Path $SampleRoot "proj.win32\hivesdk\lib\v141\x64\$lib\HIVE_SDK.lib"))) {
        throw "prebuilt HIVE_SDK.lib ($lib) missing: run proj.win32\build_hivesdk_lib.ps1 first"
    }
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $Output) { Remove-Item $Output -Force }

$count = 0
$zip = [IO.Compression.ZipFile]::Open($Output, 'Create')
try {
    foreach ($p in $parts) {
        $base = Join-Path $SampleRoot $p
        Get-ChildItem $base -Recurse -File -Force | ForEach-Object {
            $rel = $_.FullName.Substring($SampleRoot.Length + 1)
            if ($rel -match '(^|\\)(build-x64|lib_build\\build-x64)(\\|$)') { return }
            $entryName = $rootName + '/' + ($rel -replace '\\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $entryName, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
            $script:count++
        }
    }
} finally { $zip.Dispose() }

$hash = (Get-FileHash $Output -Algorithm SHA256).Hash
Write-Host ("created: {0}" -f $Output)
Write-Host ("  {0} files, {1:N1} MB, sha256 {2}" -f $count, ((Get-Item $Output).Length / 1MB), $hash)
