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
| 대상 플랫폼 | **x64** (Phase 6부터 x86 지원 제거) | Phase 0~4는 Win32(3.17.2 프리빌트가 x86 전용)였으나, Hive SDK Windows 네이티브가 x64뿐이라 x64 전용으로 전환(`docs/05`, `docs/06`) |
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
| 5 | 엔진·게임 x64 전환 (Hive SDK Windows 네이티브가 x64 전용). 브랜치 `phase5/x64-migration` | `docs/05-x64-migration.md`, `tools/build_deps_x64.ps1`, `tools/patches/engine-3.17.2-x64/`, `build.ps1`(x64) | x64 Debug/Release 빌드·실행, 새 clone·콜드 재현 (당시 Win32 회귀도 확인했으나 Phase 6에서 x86 제거) |
| 6 | Hive SDK 연동 (초기화 + 설정). 브랜치 `phase6/hive-sdk-init` | `docs/06-hive-sdk-integration.md`, `HiveCocos/`, `tools/setup_hive.ps1` | `getHiveSDKVersion` 응답, `setup` 응답, cdb로 요청 JSON 확인, DLL 없을 때 오류 처리 |
| 7 | 엔진 패치의 x86 분기 정리(x64 전용, `COCOS_USE_*` 기본 OFF). 브랜치 `phase7/engine-patch-cleanup` | `tools/patches/engine-3.17.2-x64/`, `docs/05-x64-migration.md`의 "Phase 7" 절 | 새 패치가 원본에 적용돼 편집본과 바이트 일치, 플래그 없는 새 configure로 같은 구성, x64 빌드·시험·SDK 실행 |
| 8 | Hive 로그인과 AppId. 브랜치 `phase8/hive-login` | `docs/08-hive-login.md`, `HiveCocos`의 `signIn`/`showSignIn`/`signOut`, `setup_hive.ps1 -AppId`, `hive.local.json` | 단위 시험, `L` 키로 로그인 UI 표시, **사용자 확인: 실제 로그인/로그아웃/자동 로그인** |
| 9 | 다운로드 소스 SHA256 고정(19개) + Hive SDK 고정 버전 다운로드. 브랜치 `phase9/sha256-pinning` | `docs/10-sha256-pinning.md`, `tools/hive_sdk.json`, `tools/fetch_hive.ps1`, `build_deps_x64.ps1`/`setup_engine.ps1`/`setup_hive.ps1` 수정 | 정상 통과, 변조 시 명확한 오류로 중단(부정·변이 시험), 단위 시험 회귀 없음 |
| 10 | 타이틀 화면, 초기화 후 자동/명시적 로그인, 설정 메뉴(로그아웃, 계정 삭제), 실패 팝업과 재시도. 브랜치 `phase10/title-login-settings` | `docs/11-title-login-settings.md`, `TitleScene`, `SettingsMenu`, `ModalDialog`, `HiveService` 개편, `HiveAuthV4::playerDelete`/`isAutoSignIn` | 단위 시험 146개, SDK 없는 환경과 로그인 취소로 재시도 정책 확인, **사용자 확인: 로그인 후 시작/로그아웃/계정 삭제, IME** |

Phase 5~6은 단계별 브랜치에서 작업하고, 모두 끝나 정상 동작이 확인된 뒤에만 `main`에 `--no-ff`로 병합한다.

각 단계의 마무리 순서: 작업, 검증, 문서 작성, `git commit`, `git push`, 요약 보고.

## 진행 상태
- [x] Phase 0: 계획 문서화, Claude Code·git 초기화, GitHub 저장소
- [x] Phase 1: 개발 환경 구축
- [x] Phase 2: 엔진 준비, 프로젝트 생성, 템플릿 빌드
- [x] Phase 3: 미니게임 구현
- [x] Phase 4: 리뷰와 디버깅
- [x] Phase 5: 엔진·게임 x64 전환 (브랜치 `phase5/x64-migration`, Phase 6과 함께 `main` 병합 완료 `e8422c5`)
- [x] Phase 6: Hive SDK 연동 — 초기화 + 설정 (브랜치 `phase6/hive-sdk-init`). 실제 SDK로 `setup` 응답 확인(HUD `Hive 4.26.7.0: ready (providers 9)`). x86 지원 제거(x64 전용). `main` 병합 완료
- [x] Phase 7: 엔진 패치 x86 분기 정리 (브랜치 `phase7/engine-patch-cleanup`, `main` 병합 완료)
- [x] Phase 8: Hive 로그인과 AppId (브랜치 `phase8/hive-login`, `main` 병합 완료). 구현·단위 시험(119개)·로그인 UI 표시 확인, `real` 존 약관 동의 후 **사용자가 로그인 동작 확인**. 로그아웃/자동 로그인/한글 입력(IME)은 확인 결과 미전달(`docs/08` 참고)
- [x] Phase 9: 다운로드 소스 SHA256 고정 + Hive SDK 고정 다운로드 (브랜치 `phase9/sha256-pinning`, `main` 병합 완료 `5dee067`). 라이브러리 15개, 엔진, 서브모듈 2개, 외부 번들을 SHA256으로 고정하고,
  Hive SDK는 `tools/hive_sdk.json` 고정 버전을 `tools/fetch_hive.ps1`로 받는다(최신은 확인만, 갱신은 `-Update`). 자세한 내용과 시험은 `docs/10-sha256-pinning.md`
- [x] Phase 10: 타이틀 화면 + 자동/명시적 로그인 + 설정 메뉴(로그아웃/계정 삭제) (브랜치 `phase10/title-login-settings`, `main` 병합 완료). 구현, 단위 시험 146개, SDK 없는 환경과 로그인 취소로 재시도 정책, 종료 시 CEF 프로세스 정리 확인까지 완료. 약관은 정상 노출 확인(`propFolder\REAL_HiveProperty.dat` 삭제로 재시험 가능).
  **미확인(사람 확인이 필요했고 결과를 전달받지 못함)**: 로그인 후 시작/로그아웃/자동 로그인, 계정 삭제(1회용 계정), 게스트 판별(`providerInfoData` 실제 모양), 한글 입력(IME) (`docs/11-title-login-settings.md`)

Phase 5부터 빌드는 x64 전용이다. `docs/01~04`는 Win32 시절의 기록이므로 해당 명령(`-Arch` 없이 Win32 빌드 등)은 더 이상 유효하지 않다. 현재 규칙은 `CLAUDE.md`를 따른다.
