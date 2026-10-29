# 03. 미니게임 구현 (Phase 3)

## 게임 개요
"떨어지는 물체 피하기". 파란 사각형(플레이어)을 좌우로 움직여 위에서 떨어지는 붉은 사각형을 피한다. 닿으면 게임오버.
이미지 리소스 없이 `DrawNode` 도형과 `fonts/arial.ttf`만 쓴다.

## 조작
| 입력 | 동작 |
|---|---|
| ← / → 또는 A / D | 누르고 있는 동안 좌우 이동 (340 px/s) |
| 마우스 왼쪽 버튼 누름 | 누른 동안 커서 x 위치를 향해 이동 (같은 속도 제한) |
| R / Space / 클릭 | 게임오버 후 0.5초가 지나면 재시작 (Phase 4에서 추가) |
| Esc | 종료 |

## 규칙
| 항목 | 값 |
|---|---|
| 화면 | 480 x 640 (세로), `ResolutionPolicy::NO_BORDER` |
| 점수 | 장애물이 화면 아래로 빠져나갈 때마다 +10 |
| 최고 점수 | `UserDefault`의 `best_score`에 저장. 파일: `%LOCALAPPDATA%\DodgeGame\UserDefault.xml` |
| 생성 간격 | `max(0.25, 0.9 - 0.015 * t)` 초 (t: 경과 시간) |
| 낙하 속도 | `min(520, 180 + 6 * t)` px/s, 개체마다 x0.85~1.2 편차 |
| 장애물 크기 | 가로·세로 각각 24~56 px 무작위 |
| 충돌 | AABB. 플레이어 판정 영역은 한 변당 6 px 줄여(32x32) 억울한 충돌을 줄임 |

## 구조
`Classes/GameScene.h/.cpp` 한 클래스(`cocos2d::Layer`)로 구성한다. 한 판이 한 씬이고 재시작은 씬 교체로 처리한다(상태 초기화 누락 방지).

| 함수 | 역할 |
|---|---|
| `init` | 최고 점수 로드, 배경·플레이어·HUD·입력 구성, `scheduleUpdate` |
| `update(dt)` | `Playing` 상태에서만 플레이어, 스폰, 장애물 순서로 갱신 |
| `updatePlayer` | 키보드 또는 터치(마우스) 입력으로 이동, 화면 안으로 clamp |
| `updateSpawn` / `spawnObstacle` | 간격 누적 타이머로 생성 (프레임이 길어도 `while`로 밀린 만큼 생성) |
| `updateObstacles` | 낙하, 충돌 검사, 화면 이탈 시 제거와 득점 |
| `gameOver` | 상태 전환, 최고 점수 저장, 오버레이 표시 |
| `restart` | `replaceScene`. `_restarting` 플래그로 연타 중복 호출 방지 |

상태는 `enum class State { Playing, GameOver }`. 장애물은 `std::vector<Obstacle>`로 관리하고, 득점으로 제거할 때는 인덱스 순회 중 `erase` 후 `continue`로 건너뛰는 방식을 쓴다.

## 변경한 파일
- 추가: `Classes/GameScene.h`, `Classes/GameScene.cpp`
- 삭제: `Classes/HelloWorldScene.h/.cpp` (템플릿 씬)
- 수정: `Classes/AppDelegate.cpp` (시작 씬을 `GameScene`으로, 해상도 480x640 및 스케일 기준 갱신)
- 빌드 등록: `CMakeLists.txt`, `proj.win32/DodgeGame.vcxproj`, `proj.win32/DodgeGame.vcxproj.filters`
- `proj.android/jni/Android.mk`와 `proj.ios_mac`의 프로젝트 파일은 사용하지 않아 갱신하지 않았다. 해당 플랫폼 빌드는 `HelloWorldScene` 참조 때문에 깨진다.

## 인코딩 주의
MSVC는 BOM 없는 UTF-8을 cp949로 읽어 한글 주석에서 C4819 경고를 낸다(주석 끝 글자가 줄바꿈을 삼켜 다음 줄을 주석으로 만들 위험도 있다).
`GameScene.h/.cpp`는 UTF-8 BOM으로 저장한다.

## 검증
Debug 빌드로 실행해 창 메시지(`WM_KEYDOWN`, `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`)를 보내고 화면 캡처의 파란 사각형 위치를 읽어 확인했다.

| 항목 | 결과 |
|---|---|
| 낙하와 점수 | 장애물이 떨어지고 빠져나갈 때 `Score: 10` 증가 |
| ← / → | 시작 x=247 → ← 0.8초 후 30(벽에서 멈춤) → → 0.8초 후 301 (30 + 340x0.8 = 302와 일치) |
| A / D | 0.3초 입력에 각각 약 -109 / +103 (예상 +-102) |
| 마우스 | x=400 누르고 있기 → 407(창 테두리 약 8 px 포함, 실제 약 399). 100 쪽으로 이동 시 속도 제한대로 0.7초에 약 238 px 이동, 버튼을 떼면 정지 |
| 충돌과 게임오버 | 오버레이(`GAME OVER`, 점수, 안내) 표시 |
| 재시작 | R, Space, 클릭 모두 새 판 시작(점수 0 초기화). 최고 점수 유지 |
| 최고 점수 저장 | 재시작 후 이전 판의 `Best` 유지 |
| 빌드 | Debug, Release 모두 성공. `GameScene`, `AppDelegate` 경고 없음. Release 4초 실행 확인 |

시험 중 생긴 `UserDefault.xml`(Best 100)은 삭제해 초기 상태로 돌려 놓았다.

### 시험 중 만난 함정
- `keybd_event`는 창에 포커스가 있어야 하므로 포커스를 못 받으면 입력이 안 들어간다. 창 메시지 직접 전송이 안정적이다.
- cocos의 Windows 마우스 처리는 마지막 `WM_MOUSEMOVE` 좌표를 버튼 이벤트에 재사용한다. 이동 메시지 없이 클릭만 보내면 x=0으로 인식된다.
- PowerShell에서 함수 이름 `Move`는 `Move-Item` 별칭이 우선한다.

## 알려진 한계
- 자동 시험 스크립트는 저장소에 넣지 않았다(스크래치 용도).
- 충돌 판정은 프레임 끝 위치만 검사하는 AABB다. 프레임이 크게 길어지면 통과할 수 있다는 문제는 Phase 4에서 `dt` 상한(1/30초)으로 해결했다(`docs/04-review-and-debugging.md`).
