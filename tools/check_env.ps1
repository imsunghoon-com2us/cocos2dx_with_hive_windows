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

# CMake의 'Visual Studio 17 2022' 생성기는 v170 MSBuild 파일(VC.Tools.x86.x64 컴포넌트)을 요구한다.
$v170 = if ($vsPath) { Join-Path $vsPath 'MSBuild\Microsoft\VC\v170\Microsoft.Cpp.Default.props' } else { $null }
Report 'MSBuild v170' ($v170 -and (Test-Path $v170)) $(if ($v170 -and (Test-Path $v170)) { 'found' } else { 'VC.Tools.x86.x64 component missing (see docs/01-environment-setup.md)' })

# Windows SDK 10.0.19041.0 (32비트 레지스트리 뷰 기준 경로에 Lib가 있어야 한다)
$kitsKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32').OpenSubKey('SOFTWARE\Microsoft\Windows Kits\Installed Roots')
$kits = if ($kitsKey) { $kitsKey.GetValue('KitsRoot10') } else { $null }
$ucrt = if ($kits) { Join-Path $kits 'Lib\10.0.19041.0\ucrt\x86\ucrtd.lib' } else { $null }
Report 'Win SDK 19041' ($ucrt -and (Test-Path $ucrt)) $(if ($ucrt -and (Test-Path $ucrt)) { $kits } else { 'Lib\10.0.19041.0\ucrt not found' })

# x64 빌드 도구: v141 x64 컴파일러/링커와 UCRT x64 라이브러리
$cl64 = if ($vsPath) { Get-ChildItem (Join-Path $vsPath 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '14.16.*' } | ForEach-Object { Join-Path $_.FullName 'bin\HostX64\x64\cl.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1 } else { $null }
Report 'MSVC v141 x64' ([bool]$cl64) $(if ($cl64) { $cl64 } else { 'bin\HostX64\x64\cl.exe (14.16.*) not found' })
$ucrt64 = if ($kits) { Join-Path $kits 'Lib\10.0.19041.0\ucrt\x64\ucrtd.lib' } else { $null }
Report 'UCRT x64' ($ucrt64 -and (Test-Path $ucrt64)) $(if ($ucrt64 -and (Test-Path $ucrt64)) { 'found' } else { 'Lib\10.0.19041.0\ucrt\x64 not found' })

# cdb (Debugging Tools for Windows)
$cdb = Find-Exe 'cdb' @("${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86\cdb.exe")
Report 'cdb' ([bool]$cdb) $(if ($cdb) { $cdb } else { 'not found' })

if ($fail -gt 0) { Write-Host "`n$fail item(s) failed."; exit 1 }
Write-Host "`nAll OK."; exit 0
