# 02. 엔진 준비, 프로젝트 생성, 템플릿 빌드 (Phase 2)

## 결과
- cocos2d-x 3.17.2 엔진을 복원하고 `DodgeGame`(cpp 템플릿, HelloWorld)을 생성했다.
- `tools\build.ps1 -Config Debug|Release`로 Win32 빌드에 성공했고, 두 exe 모두 창(`DodgeGame`)이 뜨고 5초간 정상 실행됨을 확인했다.
- 산출물: `DodgeGame\build\bin\DodgeGame\{Debug,Release}\DodgeGame.exe` (git 제외)

## 엔진 복원: `tools/setup_engine.ps1`
| 단계 | 내용 |
|---|---|
| 1 | GitHub 태그 `cocos2d-x-3.17.2` 압축본을 받아 `C:\Project\cocos2d-x-3.17.2`(저장소 옆 폴더)에 푼다 |
| 2 | 비어 있는 서브모듈 2개를 고정 커밋으로 받는다: `cocos2d-console` `2eb4073`, `bindings-generator` `f85b28b` |
| 3 | `download-deps.py -r yes`로 외부 프리빌트 라이브러리(`v3-deps-158`, 약 140MB) 설치 |
| 4 | `DodgeGame\cocos2d` → 엔진 폴더 junction 생성 |

- 각 단계는 이미 끝났으면 건너뛰므로 재실행해도 안전하다. 엔진 위치는 `-EngineRoot`로 바꿀 수 있다.
- 공식 배포 서버(`digitalocean.cocos2d-x.org`)는 TLS 인증서 신뢰 오류가 나서 쓰지 않았다. 인증서 검증을 끄는 우회는 하지 않았다.
- Git Bash의 GNU `tar`는 `C:\` 경로를 원격 호스트로 해석하므로 `System32\tar.exe`(bsdtar)를 쓴다. `Expand-Archive`(PS 5.1)는 긴 경로에서 실패해 쓰지 않는다.

## 프로젝트 생성
```powershell
python tools\cocos2d-console\bin\cocos.py --agreement n new DodgeGame -l cpp -p com.example.dodgegame -d <repo> -e <engine>
```
- `--agreement n`: 콘솔의 익명 사용 통계 전송 동의 프롬프트를 거부한다(프롬프트가 없으면 비대화형 환경에서 `EOFError`).
- `cocos new`는 엔진 전체(약 300MB)를 `DodgeGame\cocos2d`에 복사한다. 복사본을 지우고 junction으로 교체했다.
- 템플릿이 만든 `proj.android`, `proj.ios_mac`, `proj.linux`는 이 프로젝트에서 쓰지 않지만 템플릿 원본 그대로 남겨 두었다.

## 빌드: `tools/build.ps1`
```powershell
tools\build.ps1 -Config Debug      # 또는 Release, -Clean 으로 재구성
```
내부적으로 `cmake -S DodgeGame -B DodgeGame\build -G "Visual Studio 17 2022" -A Win32 -T v141` 후 `cmake --build`를 호출한다.

### 겪은 문제와 해결
1. **`Microsoft.Cpp.Default.props`(v170) 없음** — v141 컴포넌트만 설치하면 VS 2022 생성기가 요구하는 v170 MSBuild 파일이 없다.
   `VC.Tools.x86.x64` 컴포넌트를 추가 설치했다(`docs/01-environment-setup.md` 참고).
2. **`LNK1104: 'ucrtd.lib'`, "No CMAKE_CXX_COMPILER could be found"** — CMake는 64비트 `MSBuild\Current\Bin\amd64\MSBuild.exe`를 호출한다.
   이 PC는 64비트 레지스트리 뷰의 `KitsRoot10`이 `C:\Program Files\Windows Kits\10\`(x64 디버거 도구만 있고 `Lib` 없음)를 가리켜, 64비트 MSBuild가 UCRT 라이브러리를 못 찾는다.
   32비트 MSBuild로 직접 빌드하면 성공하므로 원인을 구분하기 어렵다. 로그(`CMakeFiles\CMakeConfigureLog.yaml`)의 LNK1104와 `/v:diag`의 `UCRTContentRoot` 값으로 확인했다.
   - 시험했지만 효과 없던 방법: `-T v141,host=x86`, `UniversalCRTSdkDir_10`/`WindowsSdkDir_10` 환경 변수.
   - 해결: `build.ps1`이 32비트 뷰의 `KitsRoot10`을 읽어 `UCRTContentRoot` 환경 변수로 지정한다(시스템 레지스트리는 수정하지 않는다).
3. **configure가 중간에 실패한 뒤 재실행이 막힘** — `CMakeCache.txt`만 남아 configure를 건너뛰었다. 생성된 `ALL_BUILD.vcxproj`로 판단하도록 바꿨다.
4. **PowerShell 5.1과 한글 주석** — BOM 없는 UTF-8 `.ps1`은 ANSI로 읽혀 한글이 깨지고 다음 줄이 주석에 먹힌다. `tools\*.ps1`은 UTF-8 BOM으로 저장한다.

## 점검 항목 추가 (`tools/check_env.ps1`)
- `MSBuild v170`: `MSBuild\Microsoft\VC\v170\Microsoft.Cpp.Default.props` 존재
- `Win SDK 19041`: 32비트 뷰 `KitsRoot10` 아래 `Lib\10.0.19041.0\ucrt\x86\ucrtd.lib` 존재

## 저장소 위생
- `.vscode/`는 로컬 절대 경로가 들어 있어 `.gitignore`에 추가했다.
- 엔진, 빌드 산출물(`DodgeGame/build/`)은 커밋되지 않는다.
