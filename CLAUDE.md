# CLAUDE.md

cocos2d-x 3.17.2 기반 Windows 미니게임(`DodgeGame`) 프로젝트. 전체 계획은 `docs/00-overview.md`를 본다.

## 툴체인 (버전 고정)
- 컴파일러: VS 2022 Build Tools의 MSVC **v141** 툴셋 (`-T v141`)
- 플랫폼: **Win32 (x86)** 와 **x64** 를 모두 빌드한다(`-Arch Win32|x64`, 기본 Win32).
  - x64는 3.17.2에 데스크톱 x64 프리빌트가 없어 `tools/build_deps_x64.ps1`로 직접 빌드한 라이브러리를 쓴다(`docs/05-x64-migration.md`).
  - x64 빌드는 네트워크 모듈(`HttpClient`, `WebSocket`, `SocketIO`, `Downloader`, assets-manager)과 Bullet/Box2D가 **제외**된다(`COCOS_USE_NETWORK=OFF`, `COCOS_USE_BULLET_BOX2D=OFF`). Win32는 그대로 포함.
- CMake 3.31.x (4.x 사용 금지), Python 2.7.18 (cocos 콘솔용)
- 환경 점검: `tools/check_env.ps1`

## 명령
- 엔진 복원: `tools/setup_engine.ps1` (엔진 다운로드, **x64 패치 적용**, `DodgeGame\cocos2d` junction 생성)
- x64 라이브러리 빌드: `tools/build_deps_x64.ps1` (약 25분, 이미 있으면 건너뜀. `-Only`, `-Force`)
- 빌드: `tools/build.ps1 -Config Debug|Release [-Arch Win32|x64]` (산출물 `DodgeGame\build` / `DodgeGame\build-x64`)
- 디버깅: Win32는 `...\Debuggers\x86\cdb.exe`, x64는 `C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe`
  (`-G`로 실행, `-g`는 `-c` 명령이 초기 중단에서 실행되므로 쓰지 않는다). x64에서 `this`는 `@rcx`(x86은 `@ecx`).

## 디렉터리 규칙
- `DodgeGame/cocos2d/`는 엔진 junction이며 git에서 제외된다. **직접 수정하지 않는다.**
  엔진을 고쳐야 하면 `tools/patches/engine-3.17.2-x64/*.patch`에 패치로 추가한다(`setup_engine.ps1`이 멱등하게 적용).
  패치는 Win32 동작을 바꾸지 않게 분기를 **추가**하는 방식으로만 쓴다. 패치 파일은 LF 줄바꿈을 유지한다(`.gitattributes`).
- 게임 코드는 `DodgeGame/Classes/`에 둔다. 새 파일은 `CMakeLists.txt`와 `proj.win32` vcxproj 양쪽에 등록한다.
- 빌드 산출물(`build/`, `build-x64/`, `Debug.win32/`, `Release.win32/`)은 커밋하지 않는다.
- **`HiveSDKv4_unreal/`(Hive SDK 패키지)는 절대 커밋하지 않는다.** 독점 바이너리(CEF 약 440MB)와 키/토큰이 들어 있어 `.gitignore`로 제외되어 있다.
  `git add -A`를 쓰지 말고 경로를 지정해 스테이징한다.
- `.ps1`, 한글 주석이 든 `.cpp/.h`는 **UTF-8 BOM**으로 저장한다(Windows PowerShell 5.1, MSVC가 BOM 없는 UTF-8을 ANSI/cp949로 읽는다).

## 작업 규칙
- 작업은 `docs/00-overview.md`의 Phase 순서로 진행하고, 한 단계가 끝나면 보고한 뒤 지시를 기다린다.
- 단계마다 `docs/0N-*.md`를 작성하고, `docs/00-overview.md`의 진행 상태 체크리스트를 갱신한다.
- 커밋 메시지는 `[Phase N] 요약` 형식으로 쓰고, 단계가 끝나면 `git push` 한다.
- **Phase 5부터는 단계별 브랜치**(`phase5/x64-migration`, `phase6/hive-sdk-init`)에 커밋·푸시한다. `main`은 모든 단계가 끝나고
  정상 동작이 확인된 뒤에만 `--no-ff`로 병합한다. 병합 전에 충돌, `git status`, 민감 파일 미포함을 확인한다.
- 저장소는 public으로 전환할 예정이다(현재 private). 토큰, 자격 증명, 개인정보를 커밋하지 않는다. 커밋 전에 `git diff --cached`로 확인한다.
- 문서와 대화는 한국어로 쓴다.
