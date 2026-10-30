# 08. Hive 로그인과 AppId (Phase 8)

브랜치: `phase8/hive-login` (`phase7/engine-patch-cleanup`에서 분기). x64 전용.

## 결과 요약
| 항목 | 결과 |
|---|---|
| AppId 변경 | 사용자가 지정한 Microsoft Store용 AppId로 적용(Debug, Release). 값은 커밋되지 않는 `hive.local.json`에만 있다. SDK 패키지 원본(`steam`)은 그대로 |
| 새 AppId로 `setup` | `ready (providers 8)` (이전 `steam` AppId는 `providers 9`). 약관 동의는 유지되어 클릭 없이 3초 만에 `ready` |
| 단위 시험 | **119개 검사 통과**(이전 87 + 로그인 32). 토큰 노출 변이 시험으로 실패를 잡아냄 |
| `L` 키 -> Hive 로그인 UI | **확인됨.** `showSignIn` 요청 후 HUD `Login: Hive sign-in UI...`, 화면에 계정 선택 UI(Google, Hive, Apple, Facebook, X, Telegram, LINE, 게스트, "로그인 상태 유지")가 뜸. 제 시험에서는 UI에서 아무것도 선택하지 않았다 |
| 실제 로그인 완료, 로그아웃, 자동 로그인 | **미확인 - 사용자 확인 대상**(계정 생성/로그인은 대신하지 않았다) |
| 빌드 | x64 Debug, Release 오류 0, 우리 코드 경고 0 |

## AppId 적용 방식
SDK는 exe 옆 `Resources\hive_config.xml`을 읽는다. 샘플 설정은 `steam`이 활성이고 `microsoftstore` 등은 주석 후보로 적혀 있다.
`tools/setup_hive.ps1`이 복사 직후 **활성 `<appId>` 한 줄만** 바꾼다(정확히 1개가 아니면 실패). 값의 우선순위:
1. `-AppId <id>` 2. 환경 변수 `HIVE_APP_ID` 3. 저장소 루트의 `hive.local.json`(`{"appId": "..."}`, **`.gitignore`로 커밋 제외**)

- 현재 PC에는 `hive.local.json`에 위 AppId를 넣어 두었으므로 `tools\setup_hive.ps1 -Config Debug|Release`만 실행해도 적용된다.
- 매 실행이 샘플 설정을 다시 복사한 뒤 AppId를 덮어쓴다. AppId를 지정하지 않으면 SDK 샘플 값(`steam`)으로 돌아간다.
- 게임 쪽 `HiveService`는 `HIVE_APP_ID`가 있으면 `setup` 전에 `Configuration.setAppId`로도 지정한다(런타임 덮어쓰기).
- 이 AppId는 SDK 공식 패키지의 샘플 `hive_config.xml`에 후보로 이미 적혀 있는 공개 값이라 노출돼도 문제없다(사용자 확인). `hive.local.json`은 보안 때문이 아니라 PC별 설정을 저장소 밖에 두기 위한 편의다. clone만으로 같은 AppId가 적용되게 하려면 `setup_hive.ps1`의 기본값으로 커밋하면 된다(아직 하지 않음). 실제 게임용 AppId와 `appsflyer` 같은 키는 별개이며 계속 커밋하지 않는다.
- **`<market>GO</market>`는 그대로다.** AppId만 Microsoft Store용으로 바뀌었고 마켓 값은 유효한 값을 몰라 건드리지 않았다. 로그인 동작에 영향이 있는지는 확인하지 못했다.

## 구조
```
HiveCocos/include/HiveCocos/HiveAuthV4.h, src/HiveAuthV4.cpp   signIn(providerType), showSignIn(), signOut() + 응답 파싱
HiveCocos/include/HiveCocos/HiveTypes.h                         HivePlayerInfo (toString()은 토큰 제외)
DodgeGame/Classes/HiveService.h/.cpp                             로그인 흐름과 상태 문구
DodgeGame/Classes/GameScene.cpp                                  L/O 키, HUD 로그인 줄
```

### 와이어 형식 (SDK 소스 `HiveAuthV4Impl.cpp`, `HiveAuthV4Types.cpp`)
| 호출 | 요청 | 응답 |
|---|---|---|
| `signIn` | `{"class":"AuthV4","method":"signIn","handlerId":N,"providerType":"GUEST",...}` | `resultAPI`, `playerInfo` |
| `showSignIn` | `{"class":"AuthV4","method":"showSignIn","handlerId":N,...}` (파라미터 없음) | `resultAPI`, `playerInfo` |
| `signOut` | `{"class":"AuthV4","method":"signOut","handlerId":N,...}` | `resultAPI` |

- `providerType`은 네이티브 이름 문자열이다: `GUEST`, `HIVE`, `GOOGLE`, `FACEBOOK`, `APPLE`, `X`, `TELEGRAM`, `LINE`, `STEAM`, ..., `AUTO`. 사용 가능한 목록은 `setup` 응답의 `providerTypeList`.
- `playerInfo`: `playerId`(64비트 정수), `playerName`, `playerImageUrl`, `playerToken`, `accessToken`, `did`, `isNewUser`, `providerInfoData`(미파싱).
- 응답 판정은 `setup`과 같다: `resultAPI.errorCode >= 0 || code >= 0`이면 성공.
- 세 호출 모두 비동기이고 응답은 SDK 스레드 -> dispatcher -> cocos 스레드 순으로 전달된다(Phase 6 구조 재사용). 요청을 보낼 수 없으면 콜백이 `delivered == false`로 즉시 호출된다.

## 게임 동작
- 시작: `setup` 성공(`ready`) 후 `isAutoSignIn`이 true이면 저장된 세션으로 `signIn("AUTO")`를 자동 호출한다. false이면 HUD에 `Login: press L to sign in`.
- **`L`**: `showSignIn`(Hive 로그인 UI). **`O`**: `signOut`. 로그인 진행 중이거나 `setup`이 끝나지 않았으면 무시된다.
- HUD 세 번째 줄(녹색)에 로그인 상태가 표시된다: `press L to sign in` -> `Hive sign-in UI...` -> `signed in (player N[, new user]) - O: sign out` 또는 `failed (code N) - press L to sign in`.
- **로그인 UI가 떠 있는 동안 게임 키 입력(`ESC` 포함)을 무시한다.** UI에 글자를 입력하거나 `ESC`로 대화상자를 닫을 때 게임이 반응하거나 종료되지 않게 하려는 것이다. 게임은 계속 진행되므로 UI를 열어 둔 채 오래 있으면 장애물에 맞을 수 있다. 창 닫기 버튼으로는 언제든 종료된다.
- **토큰은 로그와 HUD에 남기지 않는다.** 로그에는 `resultAPI`와 `playerId`, `playerName`, `isNewUser`만 찍는다(`HivePlayerInfo::toString()`). `playerToken`, `accessToken`은 구조체에 보관만 한다.

## 검증
- **단위 시험 32개 추가**: 요청 형식(`providerType`, 파라미터 없음, `handlerId` >= 1), 응답 파싱(64비트 `playerId`, 문자열 필드, `isNewUser`), 순서가 뒤바뀐 응답 3개를 SDK 스레드에서 전달, 취소/실패 응답이 오류 정보를 보존하고 `playerInfo`가 비어 있음, **`toString()`에 토큰이 없음**, 시작 전에는 콜백이 `delivered == false`로 즉시 호출되고 요청이 전송되지 않음.
- **변이 시험**: `toString()`에 `accessToken`을 일부러 넣으면 1건 실패(종료 코드 1), 되돌리면 통과.
- **실제 SDK**: 새 AppId로 `ready (providers 8)`, `L` 키(창 메시지 전송) -> `Login: Hive sign-in UI...` -> 로그인 UI 표시(스크린샷으로 확인).

## 사용자 확인 절차 (실제 로그인)
1. `tools\build.ps1 -Config Release` (이미 빌드됨), `tools\setup_hive.ps1 -Config Release` (AppId 적용 완료).
2. `DodgeGame\build-x64\bin\DodgeGame\Release\DodgeGame.exe` 실행. HUD에 `Hive 4.26.7.0: ready (providers 8)`와 `Login: press L to sign in`이 보이는지 확인.
3. `L`을 눌러 계정 선택 UI에서 로그인(예: 게스트). 성공하면 HUD가 `Login: signed in (player N...)`로 바뀐다.
4. `O`로 로그아웃 -> `Login: signed out - press L to sign in`.
5. 로그인한 뒤 게임을 껐다 켜서 `isAutoSignIn` 자동 로그인(`Login: auto sign-in...` -> `signed in`)이 되는지 확인.
6. 실패하면 HUD의 `code N`과 콘솔 로그(`[Hive] sign-in reply: ...`)를 알려 달라.

## 알려진 한계 / 미확인
- 실제 로그인 완료(`signIn`/`showSignIn` 성공 응답과 `playerInfo` 값), `signOut`, 자동 로그인은 확인하지 못했다.
- **한글/IME 입력**: UE 쪽은 `showSignIn` 전후로 `HIVESDKCEFImeEnable/Disable`(HIVEUIFramework의 IME 함수)을 호출한다. 이 어댑터는 호출하지 않으므로 Hive 로그인 UI의 입력창(이메일 등)에서 키보드/한글 입력이 되는지 확인되지 않았다. 안 되면 IME 연동이 다음 작업이다.
- Google, Facebook 등 웹 기반 로그인이 외부 브라우저나 팝업을 여는지, 복귀가 되는지 확인하지 않았다.
- `<market>GO</market>` 값과 Microsoft Store AppId의 조합이 맞는지 확인하지 않았다.
- `showSignIn` 중 UI를 닫는 방법(취소 결과 코드)과 `signOut` 후 상태 복원은 시험하지 않았다.
- 계정 삭제(`playerDelete`), 연동(`connect`), 프로필 등 다른 AuthV4 기능은 범위 밖이다.
