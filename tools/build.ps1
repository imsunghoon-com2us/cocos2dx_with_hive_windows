# DodgeGame 빌드 (CMake + VS 2022 Build Tools, MSVC v141, x64 전용: Hive SDK 네이티브가 x64뿐이다).
#   tools\build.ps1 -Config Debug|Release [-Clean]      산출물: DodgeGame\build-x64
# 이 파일은 UTF-8 BOM으로 저장해야 한다(Windows PowerShell 5.1이 한글 주석을 ANSI로 읽는 문제 방지).
# [CmdletBinding]: 모르는 인자(예: 예전의 -Arch Win32)를 조용히 무시하지 않고 오류로 막는다.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Config = 'Debug',
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path $PSScriptRoot -Parent
$Src      = Join-Path $RepoRoot 'DodgeGame'
$BuildDir = Join-Path $Src 'build-x64'

if (-not (Test-Path (Join-Path $Src 'cocos2d\CMakeLists.txt'))) {
    throw 'engine not found. Run tools\setup_engine.ps1 first.'
}

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $cmake) { $cmake = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe' }
if (-not (Test-Path $cmake)) { throw 'cmake not found. Run tools\check_env.ps1.' }

# 64비트 MSBuild는 64비트 레지스트리 뷰의 KitsRoot10을 읽는데, x64 디버거 도구만 설치된 PC에서는
# 그 경로(C:\Program Files\Windows Kits\10)에 Lib가 없어 LNK1104(ucrtd.lib)가 난다.
# 32비트 뷰의 KitsRoot10(실제 SDK 위치)을 UCRTContentRoot로 지정해 우회한다.
$kits32 = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry32').OpenSubKey('SOFTWARE\Microsoft\Windows Kits\Installed Roots')
if ($kits32 -and $kits32.GetValue('KitsRoot10')) { $env:UCRTContentRoot = $kits32.GetValue('KitsRoot10') }

if ($Clean -and (Test-Path $BuildDir)) { Remove-Item $BuildDir -Recurse -Force }

# configure가 중간에 실패하면 CMakeCache.txt만 남으므로 생성된 솔루션 유무로 판단한다.
if (-not (Test-Path (Join-Path $BuildDir 'ALL_BUILD.vcxproj'))) {
    if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
    # 엔진 패치의 COCOS_USE_NETWORK / COCOS_USE_BULLET_BOX2D 기본값이 OFF다(x64용 curl/openssl/websockets/uv/Box2D/Bullet 빌드가 없음, docs/05).
    $cfgArgs = @('-S', $Src, '-B', $BuildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-T', 'v141')
    & $cmake @cfgArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }
}

& $cmake --build $BuildDir --config $Config -- /m /v:minimal
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

$exe = Get-ChildItem $BuildDir -Recurse -Filter 'DodgeGame.exe' | Where-Object { $_.FullName -like "*\$Config\*" } | Select-Object -First 1
if ($exe) { Write-Host "built: $($exe.FullName)" } else { Write-Host 'build finished, but DodgeGame.exe was not found' }
