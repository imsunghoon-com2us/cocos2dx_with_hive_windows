# 05. 엔진·게임 x64 전환 (Phase 5)

브랜치: `phase5/x64-migration`

## 왜 x64인가
Hive SDK v4 Windows(UE용 패키지)의 네이티브 DLL은 **전부 x64**이고 x86 프로세스에 로드할 수 없다.
cocos2d-x 3.17.2는 Win32(x86)로만 빌드되어 있었으므로 엔진과 게임을 x64로 옮긴다. Phase 5 시점에는 Win32 빌드를 그대로 유지했고 회귀 시험도 했지만, Phase 6 마무리에서 **x86 지원을 제거**했다(`docs/06` "x86 제거"). 아래 Win32 관련 기록은 그 시점의 사실이다.

## 결과
| 항목 | 결과 |
|---|---|
| Win32 Debug 빌드 (패치된 엔진) | 성공 (회귀 없음) |
| x64 Debug / Release 빌드 | 성공, `DodgeGame.exe` machine=8664 |
| 실행 프로세스 | 경로 `build-x64\...`, WOW64 아님(64비트), x64 `zlib1.dll`/`glew32.dll` 로드 |
| 회귀 시험(이동, 마우스, 게임오버, Space·클릭 재시작) | x64에서 통과 (A 139, D 241, 마우스 407) |
| 재시작 지연(cdb, x64) | `_gameOverElapsed` = 0.550 (>= 0.5) |
| `CC_LITTLE_ENDIAN` 수정 효과 | x64 원본 0(불일치) -> 패치 후 1(일치), 실측 |
| Win32 Release 빌드 (패치된 엔진) | 성공, exe machine=14C(x86), 네트워크 DLL 포함, 실행 정상 |
| 새 clone 재현 | 성공 (아래 "재현 시험") |

## 구성
### 1. 엔진은 패치로 관리 (`tools/patches/engine-3.17.2-x64/`)
엔진 트리는 저장소 밖(`C:\Project\cocos2d-x-3.17.2`)에 있으므로 수정은 패치 파일이 기준이다. `tools/setup_engine.ps1`이 적용한다.
- 이미 적용됐는지 `git apply --reverse --check`로 감지해 건너뛴다(재실행 안전). 부분 적용 상태면 오류로 멈춘다.
- **`core.autocrlf=false`로 적용한다.** 이 PC의 전역 `core.autocrlf=true`로는 `git apply`가 엔진 파일 전체를 CRLF로 바꿔 놓는다.
  패치 파일 자체도 LF여야 하므로 `.gitattributes`에 `*.patch -text`를 두었다.

| 패치 | 내용 |
|---|---|
| `0001-x64-prebuilt-paths` | `CocosExternalConfig.cmake`가 `win64` 경로(`win_arch_dir`, `win_prebuilt_subdir`)를 쓰게 하고(Phase 5에는 `CMAKE_SIZEOF_VOID_P EQUAL 8` 분기였으나 Phase 7에서 x64 전용으로 단순화), 경로가 `win32`로 고정돼 있던 `zlib`, `sqlite3`, `win32-specific/{gles,icon,MP3Decoder,OggDecoder,OpenalSoft}`가 이 변수를 쓰게 함 |
| `0002-optional-network-and-bullet-box2d` | 옵션 `COCOS_USE_NETWORK`, `COCOS_USE_BULLET_BOX2D` 추가(Phase 5에는 기본 ON = 기존 동작이었으나 Phase 7에서 **기본 OFF**로 변경). OFF면 network 소스, assets-manager 소스, curl/openssl/uv/websockets/Box2D/Bullet 타깃을 빼고 `CC_USE_3D_PHYSICS=0` 정의 |
| `0003-x64-source-fixes` | `UIEditBoxImpl-win32.cpp`: `GWL_WNDPROC`/`GWL_USERDATA` -> `GWLP_*` (64비트에서는 정의되지 않아 컴파일 실패). `ccMacros.h`: `CC_LITTLE_ENDIAN`을 `_M_IX86 || _M_X64`일 때 1로 |

**`CC_LITTLE_ENDIAN` 버그는 컴파일은 되지만 동작이 틀리는 종류라** 실측으로 확인했다. 실제 엔진 헤더를 x64로 컴파일해 값을 출력:
- 원본: `CC_LITTLE_ENDIAN=0`, 실제 CPU는 리틀엔디언 -> 불일치. `Node`의 `_localZOrder$Arrival` union이 잘못된 레이아웃이 되어 z-order 정렬이 깨진다.
- 패치 후: `1`, 일치.
(x86 패치 후 시험 exe는 보안 솔루션이 실행을 막아 값을 못 읽었다. 조건이 `_M_IX86 || _M_X64`라 x86 값은 변하지 않고 Win32 게임 회귀가 통과했다.)

### 2. x64 제외 모듈
x64용 데스크톱 프리빌트가 없는 **curl, OpenSSL, libwebsockets, libuv, Box2D, Bullet**은 제외했다(엔진 `external`의 x64 바이너리는 UWP용뿐이라 데스크톱 exe에 쓸 수 없다).
- 이유 1: 가장 어려운 묶음(OpenSSL 1.1.0c는 Perl+NASM, curl/websockets는 그 위에 의존)이 통째로 빠진다.
- 이유 2: Hive SDK가 자체 `libcurl.dll`, `zlib1.dll`, OpenSSL 3를 exe 옆에 두므로 엔진용 x64 빌드와 DLL 이름이 충돌한다.
- 영향: x64 빌드에서 `HttpClient`, `WebSocket`, `SocketIO`, `Downloader`, assets-manager를 쓸 수 없다(현재 게임은 쓰지 않음). Win32 빌드는 그대로 포함한다.
- `cocosbuilder` 등이 `extensions`의 일부 헤더를 쓰므로 extensions 전체가 아니라 `assets-manager` 소스만 조건부로 뺐다.

### 3. x64 라이브러리: `tools/build_deps_x64.ps1`
엔진 `external/*/include`의 **같은 버전** 소스를 받아 CMake(`-A x64 -T v141`, Release, `/MD`)로 빌드하고 엔진의 `...\prebuilt\win64\`(sqlite3는 `libraries\win64`)에 설치한다.
`-Only`, `-Force`, `-EngineRoot`, `-WorkRoot`(기본 `C:\Project\cocos2d-deps-x64`) 지원, 결과가 있으면 건너뛰고, 설치한 파일마다 `dumpbin /headers`로 x64를 확인하며 실패 시 종료 코드 1.

| 라이브러리 | 빌드 버전 | 비고 |
|---|---|---|
| zlib | 1.3.1 | 출력명을 `zlib1.dll`/`libzlib.lib`로 맞춤 |
| libpng | 1.6.16 | 엔진 헤더와 동일 |
| jpeg | IJG 9 | 자체 CMake(`tools/deps-x64/jpeg`) |
| libtiff | 4.0.3 | 자체 CMake. 엔진 `tiffconf.h`에 코덱이 없어 JPEG/ZIP 등 미포함. `tif_config.vc.h`의 `snprintf` 재정의 제거 |
| libwebp | 0.4.4 | decoder+encoder 합본 |
| freetype | 2.5.5 | 자체 CMake. **엔진의 `ftoption.h`가 먼저** 포함되도록 include 순서 지정 |
| glfw3 | 3.2.1 | |
| GLEW | 2.2.0 | |
| sqlite3 | 3.45.3 | amalgamation |
| chipmunk | 7.0.1 | 자체 C 프로젝트(업스트림 CMake는 MSVC에서 C++로 강제되어 컴파일 실패), Debug+Release. `cpHastySpace.c` 제외 |
| OpenAL Soft | 1.19.1 | |
| ogg / vorbis / vorbisfile | 1.3.5 / 1.3.7 | `libogg.dll` 등 엔진이 기대하는 이름. `ogg.def`의 `LIBRARY` 줄도 교정 |
| mpg123 | 1.32.10 | 공식 x64 바이너리를 `libmpg123.dll`로 복사하고 import lib 재생성 |
| iconv | win-iconv 0.0.8 | `iconv_*`와 `libiconv_*` 두 이름 모두 export(엔진 헤더가 후자로 매핑) |

zlib, sqlite3, GLEW, OpenAL, mpg123, ogg/vorbis, iconv는 API 호환되는 더 새 버전을 썼고, 구조체가 헤더에 노출되는 png/jpeg/tiff/freetype/glfw/webp/chipmunk는 엔진 헤더와 같은 버전으로 맞췄다.
산출물 26개(`.dll`/`.lib`)를 직접 `dumpbin`으로 검사해 **전부 machine=8664**임을 확인했다. DLL 의존성은 시스템 DLL과 VC/UCRT 런타임뿐이다.

### 4. 빌드/점검 스크립트
- `tools/build.ps1 -Arch Win32|x64`(Phase 5 시점. 현재는 `-Arch`가 없고 항상 x64): 빌드 폴더 분리(`build`/`build-x64`), x64는 위 두 옵션을 자동 지정. `UCRTContentRoot` 우회는 그대로.
- `tools/check_env.ps1`: `MSVC v141 x64`(`HostX64\x64\cl.exe`), `UCRT x64` 점검 추가.
- `.gitignore`: `HiveSDKv4_unreal/`, `DodgeGame/build-x64/` 추가(이 브랜치의 첫 커밋).

## 재현 시험 (새 clone)
`git clone -b phase5/x64-migration`한 빈 폴더에서 저장소 스크립트만으로 처음부터 빌드했다.

| 단계 | 결과 | 소요 |
|---|---|---|
| `setup_engine.ps1` | 엔진 다운로드, 서브모듈 2개, 외부 라이브러리, **패치 3개 적용**, junction | 101초 |
| `build_deps_x64.ps1` 1차(작업 폴더 캐시 있음) | 15개 OK | 32초 |
| `build.ps1 -Config Debug -Arch x64` | exit 0, exe 생성 | 63초 |
| **`build_deps_x64.ps1 -Force -WorkRoot <새 폴더>` (콜드)** | **15개 OK** | **473초(약 8분)** |
| 콜드 라이브러리로 x64 Debug 재링크 | exit 0, exe 실행(창 `DodgeGame`) | 10초 |

- **1차의 32초는 재현이 아니다.** 고정 경로의 작업 폴더(`C:\Project\cocos2d-deps-x64`)에 에이전트가 남긴 소스·빌드 캐시를 재사용한 것이라, 캐시 없는 빌드는 콜드 시험으로 따로 검증했다.
  이 때문에 `-WorkRoot` 기본값을 저장소 옆 폴더로 바꿨다(체크아웃마다 캐시가 섞이지 않게).
- 콜드 결과 검증: 26개 파일 전부 x64, 25개가 콜드 시작 이후 새로 생성됨. 예외 `libmpg123.dll`은 컴파일 대신 공식 바이너리를 복사하는 항목이라 `-Force`에도 다시 쓰이지 않는다.
- 첫 빌드와 비교해 DLL은 전부 같은 크기, 정적 라이브러리 8개는 수십~수백 바이트(<0.3%) 차이가 있다. 작업 폴더 경로가 달라 `.lib`에 들어가는 경로 문자열이 달라진 것으로 보이나 확인하지는 않았다.
- 콜드 라이브러리로 만든 exe는 실행 확인만 했고 이동·충돌·재시작 회귀 시험은 첫 빌드 라이브러리 기준으로 수행했다.
- 서브에이전트의 첫 빌드 보고와 콜드 결과가 일치하므로 스크립트는 처음부터 재현 가능하다고 판단한다.

## `tools/` 스크립트 소실 현상 (원인 불명, 3회)
Phase 2에서 1회, Phase 5에서 서브에이전트 작업 중 `build_deps_x64.ps1`이 1회 사라졌다(복구). 보안 솔루션이 격리했을 가능성을 읽기 전용으로 확인했다.
- Windows Defender: 우리 파일에 대한 탐지 없음(이 PC의 관련 기록은 Symantec 내부 파일뿐).
- Symantec 이벤트(최근 3일 394건)에 우리 파일명(`setup_engine`, `build_deps`, `.ps1`, `cocos2dx_with_hive`, `HiveSDK`, `DodgeGame`)을 언급한 항목은 **0건**. 탐지 이름은 `ACM.Ps-Base64`, `ACM.Ps-Reg`, `ACM.Icacls-Lnch` 등 일반 행위 규칙이다.
- 격리 목록과 Defender 제외 목록은 관리자 권한이 필요해 확인하지 못했다. **원인은 확정하지 못했다.**
- 대응: 커밋 전에 `tools/` 파일 존재를 직접 확인한다(이번 단계에서도 그렇게 했다).

## x64 디버깅 (cdb)
- x86 cdb는 x64 프로세스를 디버깅할 수 없다. `C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe`를 쓴다.
- x64 호출 규약: `this`는 `@rcx`, 첫 float 인자는 `xmm1`(멤버 함수). Phase 4의 `@ecx`/`esp+4` 방식은 x86 전용이다.
- 확인: `bp DodgeGame!GameScene::restart` 적중 시 `?? ((DodgeGame!GameScene*)@rcx)->_gameOverElapsed` = 0.550.

## 시험 중 만난 함정
- **`sed`로 백슬래시 경로를 치환하면 조용히 실패한다.** 시험 스크립트의 경로를 `sed`로 바꿨는데 치환이 안 돼, "x64 시험"이 실제로는 Win32 exe를 돌렸다(결과 숫자가 Win32와 똑같아 발견). PowerShell `String.Replace`로 바꾸고, 프로세스의 실행 경로와 WOW64 여부를 출력해 증명하도록 고쳤다.
- 재현 시험 스크립트가 `$using:`(원격 실행 전용)을 써서 clone이 실패했고, 이후 단계가 **원본 저장소에서** 돌았다. clone 실패 시 즉시 중단하는 검사를 넣었다. 원본에는 부작용이 없었다(`git status` 깨끗).
- 서브에이전트가 만든 `build_deps_x64.ps1`이 작업 중 한 번 사라졌다가 복구된 적이 있다(Phase 2의 `tools/` 파일 소실과 같은 현상, 원인 불명). 커밋 전에 파일 존재를 직접 확인했다.

## 알려진 한계
- ~~`build_deps_x64.ps1`이 내려받는 소스 15개는 체크섬 검증이 없다~~ -> **Phase 9에서 해결**: 15개 소스와 `setup_engine.ps1`의 다운로드(엔진, 서브모듈 2개, 외부 번들)를 SHA256으로 고정하고 불일치 시 중단한다(`docs/10-sha256-pinning.md`).
  남은 한계는 최초 신뢰 가정(처음 받은 파일이 이미 위변조됐다면 못 막는다)과 일부 URL(osgeo `old/` 경로 등)이 사라질 수 있다는 점이다.
- x64에는 네트워크 모듈이 없다. Hive SDK는 자체 네트워크 스택(libcurl 등)을 쓰므로 영향이 없지만, 게임 쪽에서 HTTP/WebSocket이 필요해지면 OpenSSL/curl/websockets를 따로 x64로 빌드해야 한다(Hive의 DLL 이름과 충돌하지 않게 주의).
- x64 라이브러리는 Release 위주다(chipmunk만 Debug 포함). Debug x64 exe(`/MDd`)는 `/MD`로 빌드한 정적 라이브러리를 링크하므로 링커 경고 `LNK4098`(`MSVCRT` 기본 라이브러리 충돌)이 1건 나온다. 빌드·실행·시험은 문제없었다(Win32 프리빌트도 같은 구조). 힙 할당/해제가 라이브러리와 exe 경계를 넘는 코드에서 Debug CRT 단언이 날 수 있으니 Debug 시험에서 이상이 보이면 먼저 의심한다.
- x64 Debug 빌드에는 엔진 코드의 컴파일러 경고가 약 880건 있다(`C4244` 766, `C4305` 68, `C4267` 33 등. 64비트 정수/포인터 축소 변환). `Classes\`(우리 코드)에서는 0건이며 엔진 소스는 수정하지 않았다.
- 서브에이전트 보고 중 일부(버전, 출력명 교정)는 직접 재검증하지 않았다. 산출물의 비트 수와 엔진 빌드·실행으로 간접 확인했다.

## Phase 7: 엔진 패치의 x86 분기 정리
x86 지원을 제거했으므로(`docs/06`) 패치에 남아 있던 x86용 경로를 걷어냈다(브랜치 `phase7/engine-patch-cleanup`).
- `0001`: `CocosExternalConfig.cmake`의 `CMAKE_SIZEOF_VOID_P` 분기를 없애고 `win64` 경로를 무조건 쓴다.
- `0002`: `COCOS_USE_NETWORK`, `COCOS_USE_BULLET_BOX2D` 기본값을 ON에서 OFF로 바꿨다. 그래서 `tools/build.ps1`이 더 이상 `-DCOCOS_USE_*=OFF`를 넘기지 않는다(플래그 없는 새 configure로 같은 구성이 되는지 확인).
- `0003`은 그대로(`GWLP_*`, `CC_LITTLE_ENDIAN`은 x64용 수정이라 유지).
- 패치는 원본 13개 파일 사본에 적용해 편집된 엔진과 바이트 일치함을 확인했고, 이미 적용된 엔진에서는 `--reverse --check`로 감지된다.
- 엔진 소스의 x86 코드(예: `ccMacros.h`의 `_M_IX86` 조건)는 원본 코드라 건드리지 않았다.
