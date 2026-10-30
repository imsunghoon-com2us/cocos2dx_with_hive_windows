# 06. Hive SDK 연동 - 초기화 + 설정 (Phase 6)

브랜치: `phase6/hive-sdk-init` (`phase5/x64-migration`에서 분기)

UE4/5용으로 배포되는 Hive SDK v4 **Windows 네이티브**를 cocos2d-x에서 쓰기 위한 어댑터 `HiveCocos/`를 만들고, DodgeGame에서 SDK 초기화와 `AuthV4.setup`까지 호출한다. 로그인, 결제는 범위 밖이다.

## 결과 요약
| 항목 | 결과 |
|---|---|
| 핵심 단위 시험(`HiveCocosTests`) | 87개 검사 통과 (x64 Debug, Release). 변이 시험으로 실패를 잡아냄 |
| 실제 SDK 통합 시험(`HiveSdkProbe`) | DLL 로드, 콜백 등록, 동기 호출(`getHiveSDKVersion` = `4.26.7.0`), **비동기 응답이 SDK 콜백으로 도착해 dispatcher 스레드에서 실행됨**, `resultAPI` errorCode=0 |
| 게임 + 실제 SDK (x64 Debug, Release) | 버전 조회 후 `setup` 요청 전송, **Hive 이용약관 동의 화면이 게임 창 위에 표시됨**(CEF 오버레이) |
| cdb로 실제 요청 확인 | 아래 "와이어 형식" |
| DLL 없음 / x86 + x64 DLL / 시작 실패 | 크래시 없이 명확한 오류, 게임은 계속 실행 |
| 종료 | SDK 활성 상태에서 0.5초 안에 종료, 남는 프로세스 없음, WER 크래시 기록 없음 (종료 코드 값은 PowerShell에서 읽지 못함) |
| Win32 빌드/실행 | 성공 (HiveCocos 포함 빌드, SDK 없이 `Hive: SDK unavailable`) |
| **`setup` 응답(HUD `ready`/`failed`)** | **미확인 - 약관 동의(사람의 클릭)가 필요하다. "남은 검증" 참고** |

## 구조
```
HiveCocos/
  include/HiveCocos/   HiveTypes, HiveTransport, HiveBridge, HiveConfiguration, HiveAuthV4,
                       DllTransport, FakeTransport, CocosDispatcher
  src/                 구현
  tests/               HiveCocosTests.cpp (단위), HiveSdkProbe.cpp (실제 SDK 통합 probe)
  CMakeLists.txt       HiveCocos(핵심, cocos 무의존), HiveCocosAdapter(cocos 연결), 시험 타깃 2개(on demand)
DodgeGame/Classes/HiveService.h/.cpp   게임 쪽 얇은 래퍼 (시작, 상태 한 줄, 종료)
tools/setup_hive.ps1                    SDK 런타임을 exe 옆에 배치
```
- **cocos 의존은 `CocosDispatcher.cpp` 한 파일**이다. 나머지는 cocos 없이 빌드되고 단독 시험이 가능하다(나중에 SDK 저장소로 옮기기 쉽게).
- JSON은 엔진에 포함된 rapidjson(`external/json`)을 쓴다. 새 의존성 없음.
- 모든 소스는 ASCII/영문 주석이다(BOM 문제 회피).

### 계층
| 클래스 | 역할 |
|---|---|
| `IHiveTransport` | 네이티브 C 함수 3개를 감싼 인터페이스: `start(콜백)`, `call(json)`, `stop()` |
| `DllTransport` | `HIVE_PLUGIN_CPP.dll`을 절대 경로로 `LoadLibraryExW(LOAD_WITH_ALTERED_SEARCH_PATH)` 로드(의존 DLL은 같은 폴더에서 탐색), `HiveCPP_CallNative` / `HiveCPP_destroyCallNativeReturn` / `SetHiveSendMessageCallback` 해결. 반환 문자열은 DLL이 `new[]`로 만든 것이라 반드시 `destroy`로 해제 |
| `FakeTransport` | 시험용. 요청 기록, 응답 함수 지정, SDK 스레드에서 오는 메시지 흉내 |
| `HiveBridge` | 요청 JSON 생성, `handlerId` 매핑(1부터, 원자적 증가), 응답/이벤트 라우팅, **SDK 스레드 -> dispatcher 전달**. 동기(`callSync`)와 비동기(`callAsync`) |
| `HiveConfiguration` | `setAppId`, `setZone`, `setServerId`, `setGameLanguage`, `setUseLog`, `setHiveCertificationKey`, `getHiveSDKVersion`, `getAppId`, `getZone`, `getGameLanguage` |
| `HiveAuthV4` | `setup`(비동기, `resultAPI`/`isAutoSignIn`/`did`/`providerTypeList` 파싱), `terminateProcess` |
| `makeCocosDispatcher` | `Scheduler::performFunctionInCocosThread`로 다음 프레임에 cocos 스레드에서 실행 |

### 와이어 형식 (SDK 소스와 cdb로 확인)
근거: `HiveNativeBridgeTypes.cpp`, `HiveConfigurationImpl.cpp`, `HiveAuthV4Impl.cpp`, `HiveNativeBridgeManager.cpp`.
- 요청은 평면 JSON: `{"class":..., "method":..., "handlerId":N, ["engine":...,] "platform":..., ...파라미터}`.
- `Configuration`은 **전부 동기**이고 파라미터 키와 응답 키가 메서드 이름과 같다(`setAppId` -> `{"setAppId":"..."}`, `getHiveSDKVersion` -> `{"getHiveSDKVersion":"..."}`). 예외: `setHiveCertificationKey`의 파라미터 키는 `appKey`.
- 동기 호출은 `handlerId` -1, 응답이 반환값. 비동기(`AuthV4.setup` 등)는 `handlerId` >= 1, 응답은 SDK가 **자기 스레드**에서 콜백으로 보내며 같은 `handlerId`를 달고 온다. 같은 응답은 한 번만 전달한다.
- 정수값은 `5`로 직렬화한다(`5.0` 아님. Unreal 래퍼와 동일).
- `engine`은 비어 있으면 생략, `platform` 기본값은 `"cpp"`. Unreal은 `"Unreal"`/`"unreal"`을 보내고 레거시 `hive::` C++ 어댑터(`HIVE_CppPlugin.cpp`)는 `"platform":"cpp"`만 보낸다. **`"cpp"`는 SDK가 받아들임을 확인했다**(요청 처리, 약관 UI 표시). 다른 값("Cocos2dx" 등)은 시험하지 않았다. 환경 변수로 바꿔 시험할 수 있다.

cdb로 실제 전달된 요청(`bu HIVE_PLUGIN_CPP!HiveCPP_CallNative ".printf \"REQ: %ma\\n\", @rcx; gc"`):
```
{"class":"Configuration","method":"getHiveSDKVersion","handlerId":-1,"platform":"cpp"}
{"class":"AuthV4","method":"setup","handlerId":1,"platform":"cpp"}
```

## 사용법
```powershell
tools\build.ps1 -Config Debug -Arch x64        # 게임 빌드 (Win32도 빌드되지만 SDK는 x64 전용)
tools\setup_hive.ps1 -Config Debug             # SDK 런타임을 exe 옆에 배치 (약 18초, 변경분만)
DodgeGame\build-x64\bin\DodgeGame\Debug\DodgeGame.exe
```
- `setup_hive.ps1`은 `HiveSDKv4_unreal\...\HiveSDKWindows\assets`를 찾아(`-HiveRoot`로 지정 가능) `Build.cs`의 규칙대로 배치한다: `additional\**` -> exe 폴더(하위 폴더 유지, `hercules` 제외), `hercules\*` -> exe 폴더(평탄화), `config\*` -> `Resources\`. 끝에 `HIVE_PLUGIN_CPP.dll`이 x64인지 검사한다. 구성(Debug/Release)마다 약 740MB(CEF 포함).
- 설정값은 코드에 넣지 않는다. 기본은 `Resources\hive_config.xml`(SDK 샘플)을 SDK가 읽고, 필요하면 환경 변수로 덮어쓴다:
  `HIVE_APP_ID`, `HIVE_ZONE`(SANDBOX|REAL), `HIVE_SERVER_ID`, `HIVE_GAME_LANGUAGE`, `HIVE_BRIDGE_ENGINE`, `HIVE_BRIDGE_PLATFORM`.
- 상태는 HUD 두 번째 줄에 표시된다: `Hive: SDK unavailable` / `Hive 4.26.7.0: setup...` / `Hive 4.26.7.0: ready (providers N)` / `Hive 4.26.7.0: setup failed (code N)`.
- 시험:
  ```powershell
  cmake --build DodgeGame\build-x64 --config Debug --target HiveCocosTests   # 단위 시험 (SDK 불필요)
  DodgeGame\build-x64\HiveCocos\Debug\HiveCocosTests.exe
  cmake --build DodgeGame\build-x64 --config Debug --target HiveSdkProbe     # 실제 SDK 통합 probe
  # SDK 폴더를 작업 디렉터리로 하고 빌드 폴더의 exe를 실행:
  cd DodgeGame\build-x64\bin\DodgeGame\Debug; ..\..\..\..\HiveCocos\Debug\HiveSdkProbe.exe "<SDK가 있는 폴더 절대 경로>"
  ```

## 검증 상세
- **단위 시험 87개**: 요청 형식(필드, `engine` 생략, 정수 직렬화, UTF-8/이스케이프 왕복), `Configuration` 동기 호출과 파라미터 키, 동기 호출 실패(빈 응답, 잘못된 JSON, 잘못된 형태), 비동기 매칭과 **SDK 스레드 -> dispatcher 스레드 전달**, 순서가 뒤바뀐 응답, 중복/미지 `handlerId` 무시, 이벤트 라우팅(`uniqueKey` 포함), 시작 전/시작 실패/종료 후 동작, dispatcher 없을 때 인라인 실행, `resultAPI` 의미(`errorCode >= 0 || code >= 0`, 64비트 `latencyMs`), DLL 없음.
- **변이 시험**: "응답 후 핸들러 삭제" 줄을 지우고 재빌드하면 시험이 1건 실패(종료 코드 1)하고, 되돌리면 다시 통과한다.
- **실제 SDK probe**: 위 결과 참고. `getMetaData`(키 `hivecocos_probe`)는 UI가 필요 없는 비동기 호출이라, 약관 동의 없이 콜백 경로를 검증하는 용도로 골랐다. SDK가 서버와 통신하는 호출이다(SDK의 정상 동작).
- **오류 경로**: DLL 없음 -> 찾은 경로를 로그에 남기고 `Hive: SDK unavailable`. x86 게임 + x64 DLL -> `LoadLibrary failed (193) ... - the Hive SDK DLLs are x64 only; this process must be a 64-bit build`.

## 시험 중 만난 함정
- **cdb 출력을 파일로 리다이렉트하면 블록 버퍼링**되어 강제 종료 시 사라진다. `-logo <file>`로 cdb 자체 로그를 쓴다(`q`로 정상 종료하던 Phase 4 시험은 영향 없었음).
- cocos `log()`는 Windows 콘솔용으로 **ANSI(cp949)로 변환**해 출력하므로 UTF-8로 읽으면 한글 시스템 메시지가 깨져 보인다(cp949로 읽으면 정상).
- **SDK 폴더로 새로 복사한 exe는 실행이 막히거나 잠겼다**(`Device or resource busy`, 실행 대기). 빌드 폴더의 원본 exe는 같은 작업 디렉터리에서 바로 실행됐다. 보안 솔루션의 새 실행 파일 검사 때문으로 추정하나 확인하지는 못했다. 그래서 probe는 복사하지 않고 원본을 실행한다.
- **`tools/setup_hive.ps1`이 PowerShell로 BOM 변환(`WriteAllText`)한 직후 사라졌다**(밀리초 단위. `docs/05`에 적은 소실 현상의 재발). 그 뒤 **BOM 문자를 내용 맨 앞에 넣어 Write 도구로 한 번에 만들고 PowerShell로는 다시 쓰지 않았더니** 사라지지 않았다. 우연일 수 있어 인과는 확정하지 못했다. `tools/*.ps1`을 PowerShell로 재작성하지 않는 것을 규칙으로 삼는다.
- `Start-Sleep` 뒤 `Get-Content`, `Remove-Item` + `cmd /c` 같은 조합은 이 환경의 명령 검사가 오탐해 막는다(`Remove-Item on system path '/c'`). 명령을 분리하면 된다.
- 첫 시도에서 "SDK 없을 때 오류 경로"를 보려던 `setup_hive.ps1` 실행이 Release exe가 이미 있어서 실제 복사 경로로 들어갔다(가정이 틀렸음). 빌드 산출물 폴더라 피해는 없었다.

## 알려진 한계 / 남은 검증
- **`setup` 응답은 사람이 약관에 동의해야 도착한다.** 첫 실행에서 SDK가 `이용약관` 화면(`전체 선택`, `시작`)을 띄우고 동의 전까지 응답하지 않는다. 약관 동의는 대신하지 않았다. HUD의 최종 문구(`ready` 또는 `setup failed (code N)`)는 직접 확인이 필요하다.
- `HiveService::start()`의 설정 호출과 버전 조회는 **동기**라 SDK 초기화 동안 cocos 스레드를 막는다(체감 지연은 측정하지 않았다).
- `terminateProcess`를 종료 시 호출한다. 종료는 빠르고 크래시 기록이 없었지만, `setup` 완료 전에 종료하는 경우만 시험했다.
- `AppId`는 SDK 샘플의 `hive_config.xml`을 그대로 썼다. 실제 게임용 AppId/Zone은 환경 변수 또는 설정 파일로 지정해야 한다(저장소에 넣지 않는다).
- Steam/Google Play/DMM 플러그인 DLL은 함께 배치되지만 사용하지 않았고, 로드 여부는 확인하지 않았다.
- 레거시 `proj.win32` vcxproj에는 `HiveService`와 `HiveCocos` 소스, include 경로를 등록했으나 **이 vcxproj는 빌드해 보지 않았다**(`.filters`는 갱신하지 않음). 지원되는 빌드는 CMake(`tools/build.ps1`)다.
- 요청의 `engine`/`platform` 값은 `"cpp"`만 시험했다. SDK 서버 쪽 분석에서 엔진 구분이 필요하면 값 협의나 SDK 소스(GitHub) 확인이 필요하다.
- 로그인 UI와 CEF 오버레이(`HIVEGraphicsHook`)는 `setup`의 약관 화면에서 이미 동작하는 것을 봤지만(OpenGL 오버레이), 입력 포커스, 한글 입력(IME) 등은 검증하지 않았다.
