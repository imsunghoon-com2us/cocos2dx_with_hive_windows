# 11. 타이틀 화면, 자동/명시적 로그인, 설정 메뉴 (Phase 10)

게임을 켜면 타이틀 화면이 나오고, 여기서 Hive SDK를 초기화하고 로그인한 뒤 키를 눌러 시작한다.
게임 화면 우측 상단의 설정 버튼에서 로그아웃과 계정 삭제를 할 수 있다. 게임 안 문구는 영어다(`arial.ttf`에 한글 글리프가 없다).

## 흐름
```
앱 시작 -> TitleScene(autoSignIn=true) -> HiveService::start()
  -> setup 성공 -> isAutoSignIn() ? signIn("AUTO") : showSignIn()
       (AUTO가 실패하면 같은 시도 안에서 showSignIn으로 넘어간다: SDK 문서의 권장 흐름)
  -> 로그인 성공 -> "Press any key to start" -> 아무 키(ESC 제외)나 클릭 -> GameScene
GameScene 우측 상단 [Settings]
  -> Log out(연동 계정만) -> signOut -> TitleScene(autoSignIn=false): "Press any key to sign in", 키를 누르면 로그인 시작
  -> Delete account -> 확인 창(Delete/Cancel) -> playerDelete -> TitleScene(autoSignIn=false)
```
- `isAutoSignIn`은 동기 호출(`AuthV4.isAutoSignIn`)로 읽는다. setup 응답의 값과 같고, SDK 문서가 로그아웃 뒤 로그인 화면으로 돌아왔을 때 이 API를 쓰라고 한다.
- 이 PC의 설정(real 존)에서 로그인 창은 Google/Facebook/Hive/Apple/X/Telegram/LINE을 보여 주고 **게스트 버튼은 없었다**. "로그인 상태 유지"를 체크해야 다음 실행에서 자동 로그인이 가능하다(해석이며 이 상태를 직접 시험하지는 않았다).

## 실패 정책 (사용자 결정)
- 초기화(브리지 시작, setup)나 로그인이 실패하면 실패를 알리는 팝업을 띄우고 **OK를 누르면 재시도**한다. **재시도는 2회까지**(처음 1회 + 재시도 2회 = 최대 3회).
  3번째 실패 뒤에는 "The game will exit." 팝업이 뜨고 **OK를 누르면 게임이 종료**된다.
- 로그인 창을 닫는 것(취소)도 실패로 센다(코드 -1200032를 받았다). 초기화와 로그인은 재시도 횟수를 따로 세고, 성공하면 0으로 돌아간다.
- 팝업 문구 예: `Login failed (code -1200032). Retrying (1/2)...`, `Hive initialization failed (SDK unavailable). The game will exit.`
- **SDK가 없는 PC에서는 이제 게임이 3번 실패 팝업 뒤 종료한다**(개발용 우회 옵션 없음).

## 게스트 계정
- SDK 문서: 연동된 Provider가 없는 게스트는 `signOut` 뒤 다시 로그인할 수 없다. 그래서 게스트에게는 **Delete account만** 보인다.
- 판별: 로그인 응답 `playerInfo`의 `providerInfoData`와 `customProviderInfoData`(Provider 이름 -> 정보 객체)가 모두 비어 있으면 게스트다. 어댑터는 **키(Provider 이름)만** 읽는다(userId, e-mail은 읽지 않는다).
- 응답에 두 객체가 없으면 판별할 수 없으므로 **게스트처럼 취급**(로그아웃 숨김)하고 로그에 남긴다. 타이틀의 로그인 줄에 `linked: GOOGLE` / `guest` / `link info unknown`으로 표시한다.
- **미확인**: 실제 응답에서 `providerInfoData`가 이 모양으로 오는지는 이 PC에서 로그인해 보지 못했다(단위 시험은 UE 플러그인의 파서가 가정하는 모양으로 했다). 처음 로그인했을 때 이 줄을 확인해야 한다.

## 구현 파일
- `HiveCocos`: `HiveAuthV4::playerDelete`, `HiveAuthV4::isAutoSignIn`, `HivePlayerInfo::connectedProviders`/`isGuest()`. 시험 119개 -> 146개.
- `DodgeGame/Classes`: `HiveService`(상태, 실패 정책, 로그아웃/삭제), `TitleScene`, `ModalDialog`(팝업 1~2버튼), `SettingsMenu`(설정 버튼과 드롭다운), `GameScene`(Best 라벨을 좌측으로, 설정 버튼 추가, 메뉴/팝업이 열리면 일시정지), `AppDelegate`(첫 씬이 `TitleScene`).
  Hive 상태 라벨 2줄과 `L`/`O` 키는 게임 화면에서 없앴다(타이틀 화면이 보여 준다). 새 파일은 `DodgeGame/CMakeLists.txt`의 WINDOWS 블록에 등록했다.

## 시험 결과 (2026-10-30)
자동으로 확인한 것(창 메시지로 키/클릭을 보내고 화면 캡처, 보조 스크립트는 저장소에 없다):
| 항목 | 결과 |
|---|---|
| `HiveCocosTests` | 146개 통과(playerDelete 요청/응답, isAutoSignIn true/false/미시작, 게스트 판별 4가지, userId/e-mail이 로그 문자열에 없음) |
| Debug/Release 빌드 | 오류 0, 새 코드 경고 0 |
| SDK DLL이 없는 환경(Debug) | 실패 팝업 `(1/2)` -> OK -> `(2/2)` -> OK -> "The game will exit." -> OK -> 프로세스 종료 |
| 실제 SDK(Release, real 존) | 타이틀에서 초기화 성공(`ready (providers 7)`), `isAutoSignIn`이 false라 로그인 창이 **자동으로** 뜸 |
| 실제 SDK, 로그인 창을 3번 닫기(취소) | 매번 실패 팝업 `(1/2)`, `(2/2)`, 마지막은 종료 안내 -> Enter -> 종료 코드 0. 재시도 때마다 로그인 창이 다시 뜸 |
| 게임 화면(로그인 없이 임시로 띄움) | 우측 상단 Settings, 드롭다운(`Delete account`만 보임), 확인 창, Esc = Cancel(게임이 종료되지 않음), 일시정지 |
| 확인 창에서 Delete를 눌렀을 때(로그인 안 된 상태) | 요청이 거절되어 `Account deletion failed (not available).` 팝업(서버 요청 없음) |

**사람이 확인해야 하는 것(미확인)**:
- 로그인 성공 뒤 "Press any key to start" -> 게임 시작 -> Settings -> Log out -> 타이틀에서 키로 다시 로그인하는 전체 흐름.
- 자동 로그인 경로(`isAutoSignIn == true`, "로그인 상태 유지" 체크 후 재실행)와 AUTO 실패 시 로그인 창으로의 전환.
- 연동 계정과 게스트에서 메뉴 버튼 차이, `providerInfoData`의 실제 모양.
- **계정 삭제 자체**(`playerDelete`의 서버 동작과 결과 코드). 되돌릴 수 없으므로 실제 계정으로 시험하지 않았다. 1회용 계정(가능하면 test/sandbox zone)으로 확인해야 한다.
- 초기화 재시도에서 `setup()`을 다시 부르는 것이 SDK에서 문제없는지(SDK 미탑재 경로는 setup에 도달하지 못해 확인하지 못했다).

## 시험 중 발견한 문제: 같은 키의 다음 입력이 무시됨
- cocos2d-x 3.17은 GLFW의 `REPEAT` 키 이벤트를 무시한다. Hive 로그인 창은 뜨자마자 키보드를 가져가므로, 그 직후 도착하는 **키 뗌(WM_KEYUP)을 가로채면** GLFW는 그 키를 계속 눌린 상태로 알고
  **같은 키의 다음 입력을 REPEAT로 처리**해 게임이 무시할 수 있다(팝업에서 Enter로 재시도한 뒤 다음 팝업의 Enter가 먹히지 않은 현상과 일치한다. 클릭으로 닫으면 문제가 없었다).
- 원인을 코드로 끝까지 확인한 것은 아니고, 조치 뒤 같은 시나리오(Enter만으로 3번)가 통과했다. 조치: 키로 팝업을 닫거나 타이틀에서 키로 로그인을 시작하면 **0.25초 뒤에** 동작을 실행해 키 뗌이 먼저 게임에 도착하게 한다(`ModalDialog::choose`, `TitleScene::onAction`).

## 약관 시험용 Reset 메뉴 (Settings -> Reset terms)
설정 드롭다운의 `Reset terms`는 SDK의 `AuthV4.resetAgreement`("약관동의 관련 데이터를 초기화")를 부른다. 게스트/연동 계정 모두에게 보이고, 누르면 안내 팝업이 뜬다.
SDK가 시작되지 않았으면 `Reset failed (Hive SDK not available).`가 뜬다(임시 씬으로 화면 확인). 어댑터는 `HiveAuthV4::resetAgreement()`(동기, 응답에 결과가 없어 "요청을 보냈는가"만 반환)이고 단위 시험이 있다(153개).

**효과는 확인되지 않았다.** 로그인하지 않은 상태에서 이 호출을 setup 전, setup 후에 각각 넣어 보았지만 다음 실행에 약관이 다시 뜨지 않았다(`propFolder`의 파일도 그대로). 로그인한 상태(메뉴가 보이는 조건)에서의 효과는 시험하지 못했다.

약관이 **다시 뜨는 것이 확인된 방법**(2026-10-30, Release, real 존. 스냅샷으로 복원하며 시험):
- `<exe 폴더>\propFolder\REAL_HiveProperty.dat` 삭제 또는 `propFolder` 폴더 삭제 -> 다음 실행에서 약관 표시. 게임을 실행 중에 지운 뒤 정상/강제 종료해도 파일이 다시 생기지 않아 마찬가지였다.
- 이 파일은 동의 전 352 B, 동의 후 1824 B이고 실행마다 갱신된다. 파일 이름의 접두사는 zone(`REAL_`, Debug에는 `TEST_`)이다.
- AppId 폴더(`%LOCALAPPDATA%\<AppId>`)의 `hdata.info`, `hdata2.info`, `hudata.info`, exe의 `cache` 폴더를 지워도 약관은 다시 뜨지 않았다.
- 이전에 "AppId 파일이 약관을 좌우한다"고 한 설명은 판정 오류에서 나온 것이므로 이 문서 내용이 맞다.
- 사용자가 겪은 "파일을 지워도 약관이 안 뜬다"는 재현하지 못했다(다른 exe 폴더의 파일을 지웠거나 로그인된 상태였을 가능성, 미확인).

## 한글 입력(IME)이 필요한가
**코드로 판단한 결론: 필요 없을 가능성이 높다. 실제 입력으로는 확인하지 못했다(미확인).**
- `HIVEUIFramework.dll`(26.7.0)은 `setImeProcess`, `registerImeEventCallback`, `setUseExternalTsfIme`, `registerImeCompositionBoundsCallback`을 내보낸다(`dumpbin /exports`).
- UE 플러그인의 `HiveCefIme.cpp`는 UE WebBrowser의 Win11 한글 입력 수정을 포팅한 **외부 TSF 구현**이다. UE(Slate)가 자체 TSF 텍스트 입력 컨텍스트를 가져 CEF 웹뷰와 충돌하기 때문에 필요한 코드다.
- DLL의 `setUseExternalTsfIme` 기본값은 false로, DLL 내장 IMM32 경로를 쓰고 이 DLL은 게임 창을 서브클래싱해 `WM_IME_*`를 처리한다(`HiveCefIme.cpp` 주석). UE 플러그인 안에서도 `HIVESDKCEFImeEnable()`은 게임 코드가 부르는 선택 API이며 호출하는 곳이 없다.
- cocos2d-x 3.17.2의 Windows 창(GLFW 3.2.1)에는 자체 TSF 컨텍스트나 IME 처리가 없어 충돌 원인이 없다.
- **확인 방법(사람)**: 타이틀에서 뜨는 로그인 창의 `Hive로 로그인` 입력칸에서 한글 모드로 입력한다. 글자가 중복/누락되지 않는지, 같은 자음/모음을 반복해 입력해 보고(Win11 IME의 알려진 증상), 후보 창 위치, IME가 한글 모드일 때 게임의 방향키/A/D 이동이 되는지 본다.
  문제가 있으면 별도 단계로 외부 TSF 컨텍스트 포팅(작지 않은 작업)을 계획한다. 문제가 없으면 이 문서에 "불필요 확인"으로 바꾼다.

## 알려진 한계
- 최고 점수(`UserDefault`)는 계정과 무관한 로컬 값이라 로그아웃/계정 삭제 뒤에도 남는다.
- 계정 삭제 뒤 처리는 요청에 명시가 없어 로그아웃과 같게(타이틀 + 키로 로그인) 했다.
- 테스트용 임시 변경(시작 씬을 `GameScene`으로 바꿔 화면 확인)은 되돌렸고 커밋에 없다.
