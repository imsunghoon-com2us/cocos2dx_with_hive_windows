# 12. Hive SDK v4 cocos2d-x Windows 배포판 (Phase 11)

## 목적과 범위
회사가 Android/iOS용으로 배포해 온 Hive SDK v4 cocos2d-x 샘플(`HIVE_SDK_Cocos_Sample`, 원본 zip `Hive_SDK_v4_Cocos2d-x_Android_iOS_26.7.0.zip`)과
**같은 구조와 소스 형식**으로 Windows(x64) 배포판을 만든다. DodgeGame과 `HiveCocos`(우리 어댑터)와는 별개의 산출물이다.

사용자 결정:
- **git에는 올리지 않는다.** 샘플 폴더에 Hive 독점 소스(`PluginSource`)와 실제 키(`Classes/ConfigurationAPIKey.cpp`, `hive_config.xml`, `google-services.json` 등)가 들어 있고 저장소는 public이다.
  `.gitignore`에 `HIVE_SDK_Cocos_Sample/`과 `Hive_SDK_v4_Cocos2d-x_*.zip`을 넣었고, 저장소에는 이 문서, `tools/package_cocos_windows.ps1`만 있다.
- 범위는 `Classes/` 전체(v4 시험 화면 모두). `v1/`, `c2s/`는 JNI/Objective-C 전용 레거시라 Windows에서 제외.
- 구조는 샘플 폴더에 `proj.win32/`와 `PluginSource/Windows/`를 추가(Android의 `proj.android`, `PluginSource/Android`에 대응).
- 샘플 키(`ConfigurationAPIKey.cpp`)는 Android/iOS와 같은 값 그대로(사내 배포 전제).

## 원본 배포판 구조 (조사 결과)
- `PluginSource/Android/{include,src}`는 사실상 공용 C++ 플러그인이다. 모든 API가 picojson `{"class","method","platform":"cpp",...}`를 만들어 `HiveCPP_CallNative` 하나로 보내고,
  응답은 `HiveCppPlugin::callEngine`이 `class`로 `<Class>::executeEngine`을 골라 `method`별 전역 리스너를 호출한다(handlerId 없음). iOS의 `.mm`은 이 JSON 브리지의 네이티브 쪽이다.
- Android는 이 소스를 미리 빌드한 `jni/hivesdk/lib/c++/<abi>/libHIVE_SDK.a` + `include/HIVE_SDK_Plugin`으로 배포한다(소스는 참고용).
- `HIVE_CppPlugin.cpp`에 Win32 분기(`HIVE_PLUGIN_PLATFORM_WIN32`)가 이미 있고, 네이티브 DLL이 `HiveCPP_CallNative`, `HiveCPP_destroyCallNativeReturn`, `SetHiveSendMessageCallback`을 내보내면 된다.
  Windows SDK의 `HIVE_PLUGIN_CPP.dll`이 정확히 이 셋을 내보낸다(`HiveCocos`의 `DllTransport`도 같은 셋을 쓴다).
- Windows SDK 패키지(`HiveSDKWindows/Private/CPP/HIVE_*.cpp`, UE용)에도 레거시 `hive::` API의 Windows 구현 11개가 있지만 **Android 26.7.0 소스보다 오래됐다**
  (`ageRangeSource`, `isNewUser`, GameStats가 없음). 그래서 Android 소스를 기준으로 하고 Windows 구현의 차이만 이식했다.
  UE 쪽에서는 `SetHiveSendMessageCallback(callEngine)`이 주석 처리돼 레거시 비동기 콜백이 실제로 돈 적이 없었다(코드 조사). 아래 0단계로 직접 확인했다.

## 0단계 검증: 레거시 방식(handlerId 없음)이 Windows DLL에서 동작하는가
`HIVE_PLUGIN_CPP.dll`에 `platform:"cpp"`, handlerId 없는 요청을 직접 보냈다(스크래치 프로브, 사용자 상태는 `propFolder` 비교로 변화 없음 확인).
- 동기 호출(`Configuration.getHiveSDKVersion`)은 반환 문자열로 `4.26.7.0`을 돌려준다.
- 비동기(`Configuration.getMetaData`)는 `class`, `method`, `platform`, `resultAPI`를 담은 응답이 콜백으로 온다. **handlerId 없이도 class/method로 라우팅할 수 있다.**
- 콜백은 **SDK 내부 스레드**에서 호출된다(메인 스레드가 아님). 그래서 플러그인에 디스패처 훅을 넣었다.
- DLL은 `resultAPI.latencyMS`(대문자 S)를 보낸다. 플러그인의 `ResultAPI`는 `latencyMs`를 읽으므로 이 값은 비어 있다(iOS와 같은 현상, 기능 영향 없음).

## 구조
```
HIVE_SDK_Cocos_Sample/
  Classes/, Resources/                 Android/iOS와 공유 (Windows 분기만 추가)
  PluginSource/
    Android/, iOS/                     원본 그대로 (Windows 배포판 zip에는 넣지 않음)
    Windows/
      include/                         Android 헤더 + HiveCppPlugin::setCallbackDispatcher
      src/                             Android 소스 + Windows 변경 (아래)
  proj.win32/
    CMakeLists.txt                     앱 (x64, MSVC v141, v1/c2s 제외)
    build.ps1, build_hivesdk_lib.ps1, setup_hive_runtime.ps1, README.md
    hivesdk/
      CMakeLists.txt                   타깃 HIVE_SDK (프리빌트 lib 또는 소스 빌드)
      include/HIVE_SDK_Plugin/         헤더
      lib/v141/x64/{Debug,Release}/HIVE_SDK.lib
      lib_build/CMakeLists.txt         lib만 빌드 (build_hivesdk_lib.ps1)
    main.cpp, main.h, resource.h, game.rc, res/   (DodgeGame의 cocos 템플릿 파일)
```

## 플러그인 변경 (`PluginSource/Windows`)
Android 소스 23개 중 `HIVE_JniHelper.cpp`, `HIVE_OtherPlatformEmptyImpl.cpp`(없는 `HIVE_IAP.h`를 include해 그대로는 컴파일되지 않고, Windows 구현과 심볼이 겹침)를 뺀 21개를 복사했다. 줄바꿈은 원본 파일의 것을 유지했다.
- **`HIVE_CppPlugin.cpp`**(Windows 전용으로 새로 작성): `HIVE_PLUGIN_CPP.dll`을 **첫 호출 때** exe 폴더에서 `LoadLibraryEx`로 로드해 세 export를 부른다(import 라이브러리를 링크하지 않는다. 처음 버전은 `.lib`로 암시적 링크해 프로세스 시작 시 로드했고 콜백을 매 호출마다 등록했는데,
  샘플에서 `showSignIn` 때 멈춤이 나와 `HiveCocos`와 같은 방식으로 바꿨다. "Windows 샘플 멈춤 조사" 절). 콜백은 한 번만 등록하고, 반환 문자열은 복사한 뒤 `HiveCPP_destroyCallNativeReturn`으로 해제한다(원본 Win32 분기는 초기화되지 않은 포인터를 해제할 수 있었다).
  - `HiveCppPlugin::setCallbackDispatcher(std::function<void(std::function<void()>)>)` 추가(Windows 전용). 지정하면 모든 응답이 그 함수를 통해 전달된다. 지정하지 않으면 Android/iOS처럼 받은 스레드에서 바로 호출한다. 플러그인은 cocos에 의존하지 않는다.
  - Windows 네이티브에 구현이 없는 클래스(`Push`, `ProviderGoogle/Apple/Funtap`, `SocialHive/Facebook/Google`)는 `callNative`에서 가로채 `NOT_SUPPORTED`(`code -2`, `errorCode -13` CommonNotSupported) 응답을 만들어 같은 경로(디스패처)로 전달한다. 리스너가 영영 안 불리는 일을 막는다.
- **Windows SDK 구현에서 이식한 차이**: `selectedPlayerId`/`playerId`/`playerIdList`를 문자열이 아닌 숫자로 전송, `AuthV4::terminateProcess` 실제 호출, `IAPV4::purchase`의 `quantity`를 숫자로,
  `Configuration::setUseHercules`/`setUsePrivateBrowsingForAuth`/`get·setEnableGameController`.
- **MSVC 호환**: MSVC는 `static_cast<enum>(double)`을 거부한다(clang은 허용). `HIVE_CppPlugin.h`에 `hive::jsonNumberCast<T>`(부동소수점은 그대로, 정수와 enum은 `long long`을 거침)를 추가해
  `HIVE_SETVALUEFROMJSONNUMBER` 매크로와 `AuthV4.cpp`의 enum 변환 5곳에 썼다.

## 샘플 앱 변경 (`Classes/`, 모두 `CC_TARGET_PLATFORM == CC_PLATFORM_WIN32` 분기 또는 Windows 전용 파일)
| 파일 | 변경 |
|---|---|
| `AppDelegate.cpp` | 시작 시 `setCallbackDispatcher`로 응답을 `performFunctionInCocosThread`에 연결 |
| `BaseView_win32.cpp`(새) | `StartActivityIndicator`/`EndActivityIndicator`(로그만) |
| `HttpClient_win32.cpp`(새) | 엔진의 network 모듈이 없어 `network::HttpClient` 구현을 제공: 모든 요청이 즉시 실패 응답(cocos 스레드로 전달) |
| `Configuration.cpp` | v1/C2S 헤더, 버튼, 시작 함수를 Windows에서 제외(빈 함수) |
| `Postbox.cpp`, `ChatParamScene.cpp` | v0(C2S)/v1 데이터 매니저 include와 로그인 분기를 Windows에서 제외 |
| `IAPV4Test.cpp` | Windows에도 `marketPidList` 입력칸을 만들어 없는 위젯을 읽는 문제 방지 |
| `BaseView.cpp`, `ChatDataManager.hpp` | MSVC에서 빠진 `<ctime>`, `<cctype>` include 추가 |
Android 파일 `BaseView_android.cpp`와 같은 방식으로 새 파일은 전체를 플랫폼 가드로 감싸 Android의 `Classes/*.cpp` glob에 들어가도 영향이 없게 했다.

## 빌드와 배포
README(`proj.win32/README.md`)에 사용자용 절차가 있다. 요약:
1. `proj.win32\build_hivesdk_lib.ps1`: Debug/Release `HIVE_SDK.lib`를 `hivesdk\lib\v141\x64`에 만든다(Debug lib는 약 130MB, zip에서는 압축됨).
2. `proj.win32\build.ps1 -CocosRoot <엔진> -Config Debug|Release` (Hive SDK 패키지 경로는 빌드에 필요 없고 3번의 DLL 복사에만 쓴다)
3. `proj.win32\setup_hive_runtime.ps1 -HiveSdkRoot <HiveSDKWindows> -Config ...`: SDK DLL과 config를 exe 옆에 복사(`tools/setup_hive.ps1`과 같은 배치, 선택적으로 `-AppId`, `-Zone`).
4. 배포 zip: `tools\package_cocos_windows.ps1` -> `Hive_SDK_v4_Cocos2d-x_Windows_26.7.0.zip`(최상위 `HIVE_SDK_Cocos_Sample/`, `Classes`, `Resources`, `PluginSource/Windows`, `proj.win32`. `v1`, `c2s`, Android/iOS 프로젝트, SDK DLL, 엔진은 없음).
엔진은 이 저장소의 패치된 3.17.2 x64 엔진(`DodgeGame/cocos2d`)을 `-CocosRoot`로 썼다. 공식 3.17에는 x64 데스크톱 프리빌트가 없다(`docs/05`).

## 검증 결과
- 플러그인 계층 시험(스크래치 `plugintest.exe`, 실제 `HIVE_PLUGIN_CPP.dll`, 약관이 필요 없는 호출만): 10개 모두 통과.
  동기 호출(`getHiveSDKVersion`, `setAppId/getAppId`), 비동기 응답 도착, 디스패처 없을 때 SDK 스레드/있을 때 호출 스레드에서 실행, `Push` NOT_SUPPORTED(`code -2`, `errorCode -13`)와 그 응답의 스레드.
- 샘플 앱 Debug/Release 빌드 성공(오류 없음). Debug exe를 실행해 창과 Configuration(키 입력) 화면이 뜨는 것을 스크린샷으로 확인했다.
- zip(176개 파일, 약 19MB)을 새 짧은 경로에 풀어 `build.ps1 -Config Release`로 빌드: 성공(엔진 포함 전체 빌드, 프리빌트 `HIVE_SDK.lib` 사용). zip만으로 재현된다.
- `Classes/`는 원본 zip과 비교해 수정 8개(`AppDelegate.cpp`, `BaseView.cpp`, `ChatDataManager.hpp`, `ChatParamScene.cpp`, `Configuration.cpp`, `IAPV4Test.cpp`, `Postbox.cpp`)와 새 파일 2개뿐이다(수정은 위 표의 가드 또는 표준 헤더 include 추가). `Resources/`는 동일하다.

## Windows 샘플 멈춤 조사 (사용자 시험, 2026-11-02)
사용자가 Debug 샘플을 실행해 `AuthV4.setup`(약관 동의 후 `isAutoSignIn`, `did`, 결과값 정상 출력)까지 확인했고, 이어서 `showSignIn`을 누르자 **창이 멈췄다**(아무 동작 없음, 로그인 창 안 보임).
- 처음에는 약관이 안 떴고 같은 인스턴스 안에서 나중에 떴다. 이전 실행의 `cef.subprocess.exe` 고아 4개가 있었고(부모 프로세스 없음) 종료한 뒤 재실행했을 때는 떴다. 잔류 CEF의 영향인지 타이밍인지는 구분하지 못했다.
- 멈춘 인스턴스: Windows가 `Responding=False`로 표시, 메인 스레드 스택 1회 샘플은 `Application::run`의 프레임 대기(`Sleep`)였고(플러그인 호출 안에서 영구 대기는 아님), 화면은 정상으로 그려졌지만 로그인 창은 없었고, CEF는 떠 있었지만 활성 네트워크 연결은 없었다. 원인은 확정하지 못했다.
- **DodgeGame(HiveCocos)과의 적용 차이**를 비교해 아래를 고쳤다(어느 것이 원인인지는 사용자 재시험 전까지 미확인):
  | 항목 | DodgeGame / HiveCocos | 샘플 첫 버전 | 지금 |
  |---|---|---|---|
  | DLL 로드 | 게임 창 생성 뒤 `LoadLibraryEx`로 동적 로드 | `.lib` 암시적 링크로 프로세스 시작 시 로드 | 첫 호출 때 동적 로드 |
  | SDK 콜백 등록 | 시작 시 한 번 | 호출할 때마다 | 한 번 |
  | 응답 라우팅 | 요청마다 handlerId | 메서드별 전역 리스너(원본 방식) | 같음(원본 방식) |
  | 입력 차단 | 로그인/팝업 중 게임 키 입력 무시 | 없음(샘플 UI가 계속 입력을 받음) | 같음 |
  | 종료 시 CEF 정리 | `terminateCefChildren` | 없음 | 같음(고아 CEF가 남을 수 있음) |
  | 구성 | AppId/zone만 설정 | 샘플 UI의 많은 설정(`startv4`, HiveTheme, 권한 화면 등) | 같음 |
- 추적용으로 환경 변수 `HIVE_PLUGIN_TRACE=1`이면 exe 옆 `hive_plugin_trace.log`에 요청/반환/콜백(시각, 스레드, `class.method`, 결과 코드)을 기록한다. 내용(토큰, ID)은 쓰지 않는다.
- 위 변경 뒤 실제 DLL로 플러그인 시험 10개 재통과(동적 로드, 추적 로그 확인).
- **재시험에서 새 크래시(제가 추가한 코드의 버그)**: 첫 화면 하단 OK 버튼(`ConfigurationAPIKey::apply` -> `Configuration::setConfigurations`)에서 샘플이 접근 위반으로 종료(`std::map::_Lbound`, 스택은 `trace` -> `picojson::value::get`).
  원인: setter 호출은 DLL이 **빈 문자열**을 돌려주고(더미 값으로 직접 확인) 이를 파싱하면 null 값인데, `picojson::value::get(key)`는 객체에만 정의돼 있다(검사는 `PICOJSON_ASSERT`라 컴파일에서 빠져 Release에서는 우연히 넘어가고 Debug에서 죽는다).
  추적 로그 코드가 null 값에 `get()`을 불렀다. 수정: 객체인지 먼저 확인하는 `field()` 도우미를 쓰고, `callNative`는 객체가 아닌 반환을 **빈 객체**로 돌려줘 호출하는 쪽의 `get()`도 안전하게 했으며, `callEngine`은 파싱할 수 없는 응답을 무시한다(원본은 `//return;`으로 막혀 있었다).
  Debug CRT로 같은 호출(`setConfigurations`, `setAppId`, `getAppId`)을 하는 시험 exe가 크래시 없이 종료(종료 코드 0)하는 것을 확인했다. 이 크래시는 `showSignIn` 멈춤과는 별개다.
- **사용자 재시험 결과(Debug 샘플)**: `AuthV4.setup`(약관 동의 후 응답), `showSignIn`(명시적 로그인 팝업 표시), 로그인, `getPlayerInfo`, `signOut`까지 모두 정상 동작했다. 추적 로그상 순서는 `setup` 요청, 콜백(SDK 스레드, `code=0`), `showSignIn` 요청이었다.
  처음의 `showSignIn` 멈춤이 DLL 동적 로드와 콜백 1회 등록 중 무엇 때문에 해결됐는지는 한 번에 같이 바꿔서 가리지 못했다(일시적 현상이었을 가능성도 배제하지 못함). 현재 구조는 DodgeGame(`HiveCocos`)과 같은 방식이다.

## 종료 시 CEF 잔류 (사용자 시험, 2026-11-02)
정상 종료(창 닫기) 뒤에 `cef.subprocess.exe`가 남았고, 그다음 실행에서 약관/로그인 팝업이 뜨지 않았다. 확인한 것: 남은 4개(gpu-process, utility 2, renderer)는 부모 프로세스가 이미 없는 고아였다.
같은 날 앞선 실행(추적 로그가 있는 실행)은 `showSignIn`을 누른 뒤 로그인 창을 닫아 `-1200032`(사용자 취소) 콜백으로 끝났다. 팝업이 안 뜬 실행에는 추적 로그가 없다(그 실행은 `HIVE_PLUGIN_TRACE`를 켜지 않았다). 그 사이 빌드는 바뀌지 않았다.
DodgeGame(`HiveService::shutdown`)은 종료할 때 `AuthV4.terminateProcess`로 SDK를 끝낸 뒤 자기 자식 CEF를 종료하는데, 샘플에는 이 단계가 없었다.
- 추가: Windows 전용 `hive::HiveCppPlugin::shutdown()`. SDK를 한 번이라도 불렀다면 `AuthV4::terminateProcess()`를 호출하고 이 프로세스의 자식 `cef.subprocess.exe`를 종료한다(SDK를 안 썼으면 DLL을 로드하지 않고 아무것도 안 한다). 샘플 `AppDelegate` 소멸자에서 호출한다(DodgeGame과 같은 위치).
- 한계: 앱이 비정상 종료(크래시, 작업 관리자 종료)하면 호출되지 않아 CEF가 남을 수 있다. 이전 실행에서 남은 CEF가 다음 실행의 약관/로그인 팝업을 막는지는 DodgeGame에서 사용자가 확인한 현상과 같은 모양이지만, 이번 샘플 실행에서 직접 가려내지는 못했다.
- **확인(사용자 시험 + 프로세스 점검, 2026-11-02)**:
  - Release: `AuthV4.setup`을 눌렀지만 약관이 안 뜬 상태(CEF 5개 실행 중)에서 X로 닫았을 때 `cef.subprocess.exe`가 남지 않았다.
  - Release: 약관 동의 후 `showSignIn` 로그인 팝업이 떠 있는 상태에서 닫았을 때도 남지 않았다. 추적 로그: `AuthV4.terminateProcess` 뒤 `SHUTDOWN terminated 4 cef.subprocess child process(es)`. 이전(종료 정리 없음)에는 같은 조건에서 고아가 남았다.
  - Debug 빌드에도 같은 코드가 들어 있으나 종료 정리를 Debug에서 따로 시험하지는 않았다.
- **새 exe 폴더의 첫 `setup`에서 약관이 안 뜬 일**: Release 폴더(새 폴더)에서 처음 실행해 `setup`을 눌렀을 때 약관이 1분 넘게 안 떴고 네트워크 연결도 없었다(`TEST_HiveProperty.dat` 352B 생성). 정상 종료한 뒤 다시 실행하자 약관과 로그인 팝업이 정상으로 떴다.
  Debug 첫 실행과 DodgeGame에서 `build-x64`를 지운 뒤의 첫 실행에서도 같은 모양이 있었다. 새 폴더의 첫 실행에서 생기는 현상일 가능성이 있지만 원인은 확정하지 못했다(첫 실행에는 추적 로그가 없었다).

## 미확인 / 한계
- **샘플 UI로 확인한 것은 AuthV4의 `setup`, `showSignIn`, 로그인, `getPlayerInfo`, `signOut`까지다.** Release 빌드는 `setup`, 약관, `showSignIn` 팝업과 종료 정리까지 확인했다(로그인 완료 이후 흐름은 Debug에서 확인). `isAutoSignIn`(자동 로그인), `playerDelete`(1회용 계정으로만), IAPV4, Promotion, Chat, 미지원 클래스(`NOT_SUPPORTED`)의 화면 표시는 사람 확인 전이다. 새 exe 폴더는 처음 `setup`에서 이용약관 동의(사람)가 필요하다.
- `HttpClient_win32.cpp`의 실패 응답, `BaseView_win32.cpp`는 컴파일만 했고 화면에서 실행하지 못했다.
- 플러그인의 비동기 요청은 메서드당 리스너 하나만 보관한다(원본 설계). 같은 API를 응답 전에 다시 부르면 앞의 리스너가 덮어써진다.
- Android/iOS 프로젝트가 변경 후에도 빌드되는지는 이 PC에서 확인할 수 없다(`Classes` 변경은 Windows 가드 안이거나 표준 include 추가뿐이고, 원본 zip과의 diff로 확인).
- Windows 네이티브에 있는데도 시험하지 않은 클래스(IAPV4, Promotion, Chat, MatchMaking, DataStore, PlatformHelper, SocialV4, Analytics)는 플러그인 코드가 Android와 같고 DLL 쪽 지원 여부는 문자열 조사로만 확인했다(`showExit`, `registerLocalPush` 등은 DLL에 이름이 없었다).
- 샘플 키가 `Classes/ConfigurationAPIKey.cpp`에 그대로 들어 있다(사용자 결정). 배포 범위를 정할 때 유의한다.
