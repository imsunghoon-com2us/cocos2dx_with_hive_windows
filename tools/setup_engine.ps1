# cocos2d-x 3.17.2 엔진 복원. 이미 끝난 단계는 건너뛴다(재실행 안전).
#   1) GitHub 태그 압축본 다운로드·압축 해제
#   2) 서브모듈(cocos2d-console, bindings-generator) 고정 커밋 다운로드
#   3) download-deps.py로 외부 프리빌트 라이브러리 설치
#   4) DodgeGame\cocos2d junction 생성 (DodgeGame 폴더가 있을 때)
# 이 파일은 UTF-8 BOM으로 저장해야 한다(Windows PowerShell 5.1이 한글 주석을 ANSI로 읽는 문제 방지).
param(
    [string]$EngineRoot = (Join-Path (Split-Path $PSScriptRoot -Parent | Split-Path -Parent) 'cocos2d-x-3.17.2'),
    [string]$Python = 'C:\Python27\python.exe'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$RepoRoot = Split-Path $PSScriptRoot -Parent
$Tar      = Join-Path $env:SystemRoot 'System32\tar.exe'   # Git Bash의 GNU tar는 'C:' 경로를 못 읽으므로 bsdtar 고정
$Cache    = Join-Path $env:TEMP 'cocos2dx_setup'
$EngineUrl = 'https://github.com/cocos2d/cocos2d-x/archive/refs/tags/cocos2d-x-3.17.2.zip'
# 3.17.2 태그가 가리키는 서브모듈 커밋
$Submodules = @(
    @{ Path = 'tools\cocos2d-console';    Repo = 'cocos2d/cocos2d-console';    Sha = '2eb407382b0bb42096e30a8a3759e5ae9fe2072f' },
    @{ Path = 'tools\bindings-generator'; Repo = 'cocos2d/bindings-generator'; Sha = 'f85b28b482192e6a26b121d687567098ff4db562' }
)

function Get-Zip($url, $dest) {
    if (-not (Test-Path $dest)) {
        Write-Host "download $url"
        Invoke-WebRequest $url -OutFile $dest -UseBasicParsing
    }
}

function Expand-Zip($zip, $target) {
    # 압축 루트 폴더 하나를 벗겨서 $target으로 옮긴다.
    $tmp = Join-Path $Cache ([IO.Path]::GetRandomFileName())
    New-Item -ItemType Directory $tmp | Out-Null
    & $Tar -xf $zip -C $tmp
    if ($LASTEXITCODE -ne 0) { throw "tar failed: $zip" }
    $root = Get-ChildItem $tmp -Directory | Select-Object -First 1
    New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
    Move-Item $root.FullName $target
    Remove-Item $tmp -Recurse -Force
}

New-Item -ItemType Directory -Force $Cache | Out-Null

# 1) 엔진
if (Test-Path (Join-Path $EngineRoot 'CMakeLists.txt')) {
    Write-Host "[skip] engine: $EngineRoot"
} else {
    $zip = Join-Path $Cache 'cocos2d-x-3.17.2.zip'
    Get-Zip $EngineUrl $zip
    Expand-Zip $zip $EngineRoot
    Write-Host "[done] engine: $EngineRoot"
}

# 2) 서브모듈
foreach ($s in $Submodules) {
    $dir = Join-Path $EngineRoot $s.Path
    if ((Test-Path $dir) -and (Get-ChildItem $dir -Force -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        Write-Host "[skip] $($s.Path)"
        continue
    }
    $zip = Join-Path $Cache ("{0}-{1}.zip" -f ($s.Repo -replace '/', '_'), $s.Sha.Substring(0, 7))
    Get-Zip "https://github.com/$($s.Repo)/archive/$($s.Sha).zip" $zip
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    Expand-Zip $zip $dir
    Write-Host "[done] $($s.Path)"
}

# 3) 외부 프리빌트 라이브러리
$versionFile = Join-Path $EngineRoot 'external\version.json'
if (Test-Path $versionFile) {
    Write-Host '[skip] external deps'
} else {
    if (-not (Test-Path $Python)) { throw "Python 2.7 not found: $Python" }
    Push-Location $EngineRoot
    try {
        & cmd /c "`"$Python`" download-deps.py -r yes"
        if ($LASTEXITCODE -ne 0) { throw "download-deps.py failed ($LASTEXITCODE)" }
    } finally { Pop-Location }
    Write-Host '[done] external deps'
}

# 3-1) x64 / optional-module patches (tools\patches\engine-3.17.2-x64\*.patch)
# The engine tree lives outside the repo, so the patch files are the source of truth for our engine changes.
# They only add branches (x64 paths, COCOS_USE_* options), so the Win32 build keeps working unchanged.
# core.autocrlf=false: git apply would otherwise rewrite the (LF) engine files with CRLF.
$patchDir = Join-Path $PSScriptRoot 'patches\engine-3.17.2-x64'
if (Test-Path $patchDir) {
    Push-Location $EngineRoot
    try {
        foreach ($p in (Get-ChildItem $patchDir -Filter '*.patch' | Sort-Object Name)) {
            & cmd /c "git -c core.autocrlf=false apply --reverse --check -p1 `"$($p.FullName)`" 2>nul"
            if ($LASTEXITCODE -eq 0) { Write-Host "[skip] patch $($p.Name) (already applied)"; continue }
            & cmd /c "git -c core.autocrlf=false apply --check -p1 `"$($p.FullName)`""
            if ($LASTEXITCODE -ne 0) { throw "patch $($p.Name) does not apply cleanly (partially applied or engine modified?)" }
            & git -c core.autocrlf=false apply -p1 $p.FullName
            if ($LASTEXITCODE -ne 0) { throw "git apply failed: $($p.Name)" }
            Write-Host "[done] patch $($p.Name)"
        }
    } finally { Pop-Location }
}

# 4) junction
$game = Join-Path $RepoRoot 'DodgeGame'
$link = Join-Path $game 'cocos2d'
if (-not (Test-Path $game)) {
    Write-Host '[skip] junction: DodgeGame not created yet'
} elseif (Test-Path $link) {
    $item = Get-Item $link -Force
    if ($item.LinkType -ne 'Junction') { throw "$link exists and is not a junction. Remove it manually." }
    Write-Host "[skip] junction: $link -> $($item.Target)"
} else {
    New-Item -ItemType Junction -Path $link -Target $EngineRoot | Out-Null
    Write-Host "[done] junction: $link -> $EngineRoot"
}
