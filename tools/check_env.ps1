# 개발 환경 점검. 모든 항목이 OK이면 exit 0, 하나라도 실패하면 exit 1.
$ErrorActionPreference = 'Continue'
$fail = 0

function Report($name, $ok, $detail) {
    $tag = if ($ok) { 'OK  ' } else { 'FAIL' }
    Write-Host ("[{0}] {1,-14} {2}" -f $tag, $name, $detail)
    if (-not $ok) { $script:fail++ }
}

function Find-Exe($name, $extra) {
    $c = Get-Command $name -ErrorAction SilentlyContinue | Where-Object { $_.Source -notlike '*\WindowsApps\*' } | Select-Object -First 1
    if ($c) { return $c.Source }
    foreach ($p in $extra) { if ($p -and (Test-Path $p)) { return $p } }
    return $null
}

# git
$git = Find-Exe 'git' @()
Report 'git' ([bool]$git) $(if ($git) { (& $git --version) } else { 'not found' })

# GitHub CLI (PATH에 없어도 기본 설치 경로를 본다)
$gh = Find-Exe 'gh' @("$env:ProgramFiles\GitHub CLI\gh.exe")
Report 'gh' ([bool]$gh) $(if ($gh) { (& $gh --version | Select-Object -First 1) } else { 'not found' })

# CMake 3.31.x (4.x 금지)
$cmake = Find-Exe 'cmake' @("$env:ProgramFiles\CMake\bin\cmake.exe")
$ver = if ($cmake) { ((& $cmake --version | Select-Object -First 1) -replace 'cmake version ', '') } else { '' }
Report 'cmake 3.31.x' ($ver -like '3.31.*') $(if ($cmake) { $ver } else { 'not found' })

# Python 2.7.18 (Store 스텁은 제외)
$py = Find-Exe 'python' @('C:\Python27\python.exe')
$pyVer = if ($py) { (cmd /c "`"$py`" --version 2>&1" | Out-String).Trim() } else { '' }
Report 'python 2.7.18' ($pyVer -like '*2.7.18*') $(if ($py) { "$pyVer ($py)" } else { 'not found' })

# MSVC v141 x86 툴셋
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.v141.x86.x64 -property installationPath
}
Report 'MSVC v141' ([bool]$vsPath) $(if ($vsPath) { $vsPath } else { 'VC.v141.x86.x64 component not found' })

# cdb (Debugging Tools for Windows)
$cdb = Find-Exe 'cdb' @("${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86\cdb.exe")
Report 'cdb' ([bool]$cdb) $(if ($cdb) { $cdb } else { 'not found' })

if ($fail -gt 0) { Write-Host "`n$fail item(s) failed."; exit 1 }
Write-Host "`nAll OK."; exit 0
