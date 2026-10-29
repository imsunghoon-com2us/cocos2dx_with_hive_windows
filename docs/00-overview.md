# 00. 프로젝트 개요와 진행 계획

## 목표
cocos2d-x **3.17.2**로 간단한 Windows 미니게임("떨어지는 물체 피하기", 프로젝트명 `DodgeGame`)을 만든다.
개발, 리뷰, 디버깅을 모두 Claude Code 터미널 안에서 진행하고, 단계마다 문서를 쓰고 GitHub(public)에 커밋·푸시한다.

## 원칙
- 무료 도구만 사용한다.
- 3.17.2와 호환되는 버전을 쓴다.
- 한 단계를 끝내면 결과를 보고하고 멈춘다. 지시를 받으면 다음 단계로 간다.
- 커밋 메시지는 `[Phase N] ...` 형식으로 쓴다.
- 저장소가 public이므로 토큰과 비밀정보는 커밋하지 않는다.

## 툴체인 (확정)
| 구성요소 | 버전 | 선정 근거 |
|---|---|---|
| VS 2022 Build Tools + MSVC v141 | v141 (VS2017, cl 19.16.x) | 3.17.2의 공식 지원 컴파일러는 VS2015/VS2017. 무료이고 IDE 없이 CLI로 빌드 가능 |
| 대상 플랫폼 | Win32 (x86) | 3.17.2의 Windows 프리빌트 라이브러리가 x86 전용 |
| CMake | 3.31.x | 4.x는 오래된 `cmake_minimum_required` 호환이 제거되어 3.x로 고정 |
| Python | 2.7.18 | cocos 콘솔(`setup.py`, `cocos new`)이 Python 2 기반 |
| GitHub CLI | 최신 | 저장소 생성, 인증 |
| 디버거 | cdb (Windows SDK Debugging Tools) | 터미널에서 콜스택과 브레이크포인트 확인 |

## 저장소 구조
```
cocos2dx_with_hive_windows/
├─ .gitignore
├─ README.md
├─ CLAUDE.md               # Claude Code 프로젝트 지침
├─ .claude/settings.json   # 모델: opusplan
├─ docs/                   # 단계별 개발 문서
├─ tools/                  # check_env.ps1, setup_engine.ps1, build.ps1
└─ DodgeGame/              # cocos new 결과물 (cocos2d/는 junction, git 제외)
```
엔진(`DodgeGame/cocos2d/`)은 용량이 커서 저장소에서 제외하고 `tools/setup_engine.ps1`로 복원한다.

## 단계 로드맵
| Phase | 내용 | 산출물 | 검증 기준 |
|---|---|---|---|
| 0 | 전체 계획 문서화, Claude Code 초기화, git 초기화, GitHub public 저장소 생성·푸시 | 이 문서, CLAUDE.md, README.md, .gitignore | `gh repo view`, 원격에 커밋 반영 |
| 1 | 개발 환경 구축 | `docs/01-environment-setup.md`, `tools/check_env.ps1` | 모든 도구 버전 OK |
| 2 | 엔진 준비, 프로젝트 생성, 템플릿 빌드 | `docs/02-project-setup-and-build.md`, `tools/setup_engine.ps1`, `tools/build.ps1`, `DodgeGame/` | HelloWorld Debug/Release 빌드·실행, clone 후 재현 |
| 3 | 미니게임 구현 | `docs/03-game-design.md`, `GameScene.h/.cpp` | 이동, 낙하, 충돌, 게임오버, 재시작, 점수 |
| 4 | 리뷰와 디버깅 | `docs/04-review-and-debugging.md` | 리뷰 지적 반영, cdb 브레이크포인트 적중 |

각 단계의 마무리 순서: 작업, 검증, 문서 작성, `git commit`, `git push`, 요약 보고.

## 진행 상태
- [x] Phase 0: 계획 문서화, Claude Code·git 초기화, GitHub 저장소
- [ ] Phase 1: 개발 환경 구축
- [ ] Phase 2: 엔진 준비, 프로젝트 생성, 템플릿 빌드
- [ ] Phase 3: 미니게임 구현
- [ ] Phase 4: 리뷰와 디버깅
