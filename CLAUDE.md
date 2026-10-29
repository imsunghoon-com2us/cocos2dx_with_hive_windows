# CLAUDE.md

cocos2d-x 3.17.2 기반 Windows 미니게임(`DodgeGame`) 프로젝트. 전체 계획은 `docs/00-overview.md`를 본다.

## 툴체인 (버전 고정)
- 컴파일러: VS 2022 Build Tools의 MSVC **v141** 툴셋 (`-T v141`)
- 플랫폼: **Win32 (x86)** 만 사용. x64는 3.17.2 프리빌트 라이브러리가 없어 빌드되지 않는다.
- CMake 3.31.x (4.x 사용 금지), Python 2.7.18 (cocos 콘솔용)
- 환경 점검: `tools/check_env.ps1`

## 명령
- 엔진 복원: `tools/setup_engine.ps1` (엔진 다운로드 후 `DodgeGame\cocos2d` junction 생성)
- 빌드: `tools/build.ps1 -Config Debug|Release`
- 디버깅: `cdb -g -G <exe>` (콜스택 `k`, 브레이크포인트 `bp`)

위 스크립트는 Phase 1~2에서 추가된다. 아직 없으면 `docs/00-overview.md`의 진행 상태를 확인한다.

## 디렉터리 규칙
- `DodgeGame/cocos2d/`는 엔진 junction이며 git에서 제외된다. **수정하지 않는다.**
- 게임 코드는 `DodgeGame/Classes/`에 둔다. 새 파일은 `CMakeLists.txt`와 `proj.win32` vcxproj 양쪽에 등록한다.
- 빌드 산출물(`build/`, `Debug.win32/`, `Release.win32/`)은 커밋하지 않는다.

## 작업 규칙
- 작업은 `docs/00-overview.md`의 Phase 순서로 진행하고, 한 단계가 끝나면 보고한 뒤 지시를 기다린다.
- 단계마다 `docs/0N-*.md`를 작성하고, `docs/00-overview.md`의 진행 상태 체크리스트를 갱신한다.
- 커밋 메시지는 `[Phase N] 요약` 형식으로 쓰고, 단계가 끝나면 `git push` 한다.
- 저장소는 public이다. 토큰, 자격 증명, 개인정보를 커밋하지 않는다. 커밋 전에 `git diff --cached`로 확인한다.
- 문서와 대화는 한국어로 쓴다.
