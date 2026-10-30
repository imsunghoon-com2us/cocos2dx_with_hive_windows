# 04. 리뷰와 디버깅 (Phase 4)

> 참고: 이 문서는 Win32(x86) 시절의 기록이다. 문서의 cdb 사용법은 x86 기준(`@ecx`, `esp+4`)이며, 현재 x64에서는 `@rcx`와 x64 cdb를 쓴다(`CLAUDE.md`, `docs/05`).

## 1. 코드 리뷰
`/code-review medium`으로 `GameScene.h/.cpp`, `AppDelegate.cpp`를 리뷰했고 8건이 나왔다.
리뷰가 적은 줄 번호(`AppDelegate.cpp` 465, `GameScene.cpp` 370)는 실제 파일 길이(151줄, 308줄)를 넘어 신뢰할 수 없어서, 모든 지적을 내용 기준으로 직접 대조했다.

| # | 지적 | 판단 | 근거 |
|---|---|---|---|
| 1 | 프레임 `dt` 미제한: 큰 `dt`에서 장애물이 플레이어를 통과(터널링) | **수정** | cdb로 재현(아래 2-2) |
| 2 | 지연 후 스폰 `while`이 한꺼번에 여러 개를 같은 높이에 생성 | **수정** (1번으로 해결) | `dt <= 1/30` < 최소 스폰 간격 0.25초이므로 프레임당 최대 1개 |
| 3 | 죽는 순간 진행 중이던 클릭/키로 즉시 재시작 | **수정** | 마우스로 조종하다 죽으면 실제로 발생. cdb로 검증(2-3) |
| 4 | `Label::createWithTTF` null 검사 없음 | 수정 안 함 | 폰트는 빌드가 복사하는 필수 리소스. 누락은 배포 오류라 즉시 드러나는 편이 낫다 |
| 5 | 포커스 상실 시 `_leftDown/_rightDown`이 남아 계속 이동 | **재현 안 됨** | ←를 누른 채 `WM_KILLFOCUS` 전송: 144 -> 0.6초 후에도 144. GLFW가 포커스 상실 시 눌린 키를 해제 |
| 6 | Release에서도 FPS/드로우콜 통계 표시 | **수정** | 화면 왼쪽 아래 플레이 영역을 가림. `#if COCOS2D_DEBUG`로 한정 |
| 7 | 입력 콜백 안에서 `replaceScene`/`Director::end()` 호출 | 수정 안 함 | 리뷰도 현재 동작은 인정. `_restarting` 가드가 있고 cocos의 일반적 패턴 |
| 8 | `<vector>`, `<algorithm>` 미포함 | **수정** | `cocos2d.h`의 간접 포함에 의존 중이었음 |

### 수정 내용
- `GameScene::update`: `dt = min(dt, 1/30)`(`kMaxFrameDt`). 게임오버 상태에서는 `_gameOverElapsed`만 누적.
- `canRestart()`: 게임오버 후 `kRestartDelay`(0.5초)가 지나야 R/Space/클릭 재시작을 받는다.
- `AppDelegate.cpp`: `setDisplayStats(true)`를 `#if COCOS2D_DEBUG`로 감쌌다. `COCOS2D_DEBUG=1`은 Debug 구성에만 정의된다(`CocosConfigDefine.cmake`).
  이 파일은 템플릿 원본이라 BOM이 없는 ASCII이므로 주석은 영문으로 썼다(한글을 넣으면 C4819 경고와 줄 먹힘 위험).
- `GameScene.h`: `<algorithm>`, `<vector>` 추가.

## 2. cdb 디버깅
### 2-1. 환경
- `cdb` 10.0.17763 (`C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe`). Win32 exe이므로 x86 디버거를 쓴다.
- Debug exe 옆에 `DodgeGame.pdb`(약 68MB)가 있어 함수명과 소스 줄 번호가 나온다.
- 스크립트로 돌릴 때는 `-g`(초기 중단 건너뛰기)를 **쓰지 않는다**. `-c` 명령이 초기 중단 시점에 실행되기 때문이다. 종료 시 멈춤을 건너뛰려면 `-G`.

### 2-2. 브레이크포인트 적중과 콜스택
```
cdb -G -c ".lines -e; bp DodgeGame!GameScene::gameOver; g; k 10; q" DodgeGame.exe
```
플레이어를 가만히 두면 충돌로 `gameOver`에서 멈춘다. 콜스택(소스 줄 포함):
```
DodgeGame!GameScene::gameOver [gamescene.cpp @ 263]
DodgeGame!GameScene::updateObstacles+0x18b [gamescene.cpp @ 223]
DodgeGame!GameScene::update+0x70 [gamescene.cpp @ 150]
DodgeGame!cocos2d::Scheduler::update+0x115 [ccscheduler.cpp @ 855]
DodgeGame!cocos2d::Director::drawScene+0x81 [ccdirector.cpp @ 267]
DodgeGame!cocos2d::Director::mainLoop+0x5c [ccdirector.cpp @ 1478]
DodgeGame!cocos2d::Application::run+0x187 [ccapplication-win32.cpp @ 113]
```
(줄 번호는 이 리뷰 수정 전 기준.)

### 2-3. 리뷰 지적을 디버거로 검증
x86 `thiscall`에서 `this`는 `ecx`, 첫 인자(float `dt`)는 `esp+4`에 있다(함수 진입 직후).

**1번(`dt` 미제한)** — `update` 진입 시 `dt`를 1.0f(`3f800000`)로 덮어쓰고, 하위 함수 `updatePlayer`가 받는 값을 읽는다.
```
bp DodgeGame!GameScene::update; g
?? *(float*)(@esp+4)          # 실제 dt
ed @esp+4 3f800000            # dt = 1.0f 로 강제
bd 0; bp DodgeGame!GameScene::updatePlayer; g
?? *(float*)(@esp+4)          # 하위 함수가 받은 dt
```
| | `update`의 dt | `updatePlayer`가 받은 dt |
|---|---|---|
| 수정 전 | 0.01667 | **1** (상한 없이 그대로 전달) |
| 수정 후 | 0.01667 | **0.03333** (= 1/30) |

수정 전에는 한 프레임에 `속도(180~620px/s) x 1초`만큼 장애물이 점프하고 도착 위치만 검사하므로 플레이어(32px 판정)를 통과할 수 있다.

**3번(재시작 지연)** — `restart()`에 브레이크포인트를 걸고 R 키를 100ms마다 보낸 뒤, 처음 적중했을 때 값을 읽는다.
```
bp DodgeGame!GameScene::restart; g
?? ((DodgeGame!GameScene*)@ecx)->_gameOverElapsed    # 0.5833  (>= 0.5)
?? (int)((DodgeGame!GameScene*)@ecx)->_state         # 1 (GameOver)
```
수정 전이면 R을 보내는 주기(0.1초) 이내에 재시작됐을 것이다.

### 2-4. 자주 쓰는 명령
| 명령 | 의미 |
|---|---|
| `bp 모듈!클래스::함수` | 브레이크포인트 (`bu`는 지연 해석, `bd/be/bc`는 비활성/활성/삭제) |
| `g` | 계속 실행 |
| `k` / `kb` / `k 10` | 콜스택 (`.lines -e`로 소스 줄 표시) |
| `?? 식` | C++ 식 평가. 예: `?? ((DodgeGame!GameScene*)@ecx)->_score` |
| `dd @esp+4 L1` / `ed 주소 값` | 메모리 읽기 / 쓰기 |
| `p` / `t` / `gu` | 한 줄 실행 / 함수 안으로 / 함수 끝까지 |
| `q` | 종료 (디버기도 종료) |

### 2-5. 한계
- `?? ...->_obstacles.size()`는 `Type does not have given member`로 실패한다(MSVC STL 멤버 함수는 cdb 식 평가기가 호출하지 못함). 멤버 필드(`_obstacles._Myfirst` 등)로 접근하거나 `dx`를 써야 한다.
- 브레이크포인트에서 멈추면 게임 루프가 멈춘다. 화면 갱신이 필요한 시험에서는 조건부 브레이크포인트(`bp ... ".if (...) {...} .else {gc}"`)를 쓴다.
- 이 PC에서는 Symantec 에이전트 DLL이 함께 로드되어 `ModLoad`/`int 3` 로그가 섞여 나온다(무관).

## 3. 검증
| 항목 | 결과 |
|---|---|
| Debug/Release 빌드 | 성공. `GameScene`, `AppDelegate` 경고 없음 |
| `dt` 상한 | cdb로 확인 (1 -> 0.03333) |
| 재시작 지연 | cdb로 확인 (`_gameOverElapsed` = 0.583) |
| 회귀(이동, 마우스, Space/클릭 재시작) | A 144, D 247(시작 247 복귀), 마우스 407, 게임오버 후 Space·클릭으로 재시작 |
| Release 통계 표시 | 화면 캡처로 FPS/GL 정보 사라짐 확인 |
| 포커스 상실 시 키 상태 | 재현 안 됨(144 -> 144) |

시험으로 생긴 `%LOCALAPPDATA%\DodgeGame\UserDefault.xml`은 삭제해 초기 상태로 돌려 놓았다.
시험 스크립트(창 메시지 입력, 화면 캡처, cdb 구동)는 저장소에 넣지 않았다(스크래치 용도).

## 4. 남은 사항
- 충돌 판정은 여전히 프레임 끝 위치만 검사한다(`dt <= 1/30`에서 최대 이동은 프레임당 약 21px(624px/s ÷ 30)로 장애물 최대 높이 56px + 플레이어 판정 32px = 88px보다 작아 안전하지만, 속도 상한(520 x 1.2)을 크게 올리면 스윕 검사가 필요하다).
- Android/iOS 프로젝트 파일은 Phase 3에서 삭제한 `HelloWorldScene`을 참조하므로 해당 플랫폼 빌드는 깨져 있다(Win32 전용이라 방치).
