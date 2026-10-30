# CLAUDE.md

cocos2d-x 3.17.2 기반 Windows 미니게임(`DodgeGame`) 프로젝트. 전체 계획은 `docs/00-overview.md`를 본다.

## 툴체인 (버전 고정)
- 컴파일러: VS 2022 Build Tools의 MSVC **v141** 툴셋 (`-T v141`)
- 플랫폼: **x64 전용**. Hive SDK 네이티브 DLL이 x64뿐이라 Phase 6부터 x86(Win32) 빌드를 지원하지 않는다
  (`DodgeGame/CMakeLists.txt`가 Win32 configure를 오류로 막고, `build.ps1`은 `-Arch`를 받지 않는다).
  - 3.17.2에 데스크톱 x64 프리빌트가 없어 `tools/build_deps_x64.ps1`로 직접 빌드한 라이브러리를 쓴다(`docs/05-x64-migration.md`).
  - 네트워크 모듈(`HttpClient`, `WebSocket`, `SocketIO`, `Downloader`, assets-manager)과 Bullet/Box2D는 **제외**된다(엔진 패치에서 `COCOS_USE_NETWORK`, `COCOS_USE_BULLET_BOX2D`의 기본값이 OFF).
- CMake 3.31.x (4.x 사용 금지), Python 2.7.18 (cocos 콘솔용)
- 환경 점검: `tools/check_env.ps1`

## 명령
- 엔진 복원: `tools/setup_engine.ps1` (엔진 다운로드, **x64 패치 적용**, `DodgeGame\cocos2d` junction 생성)
- x64 라이브러리 빌드: `tools/build_deps_x64.ps1` (약 25분, 이미 있으면 건너뜀. `-Only`, `-Force`)
- 빌드: `tools/build.ps1 -Config Debug|Release [-Clean]` (산출물 `DodgeGame\build-x64`)
- Hive SDK 런타임 배치: `tools/setup_hive.ps1 -Config Debug|Release [-AppId <id>] [-Zone real|test|sandbox]` (변경분만 복사, 구성당 약 740MB).
  AppId는 `-AppId` > 환경 변수 `HIVE_APP_ID` > 저장소 루트 `hive.local.json`(`{"appId":"...","zone":"..."}`, git 제외) 순으로,
  Zone은 `-Zone` > `hive.local.json`의 `zone` 순으로 적용된다(`HIVE_ZONE`은 게임 런타임용이라 읽지 않는다). 없으면 SDK 샘플 값
  (`steam`, `test`)이다. 매 실행이 샘플 설정을 다시 복사한 뒤 값을 덮어쓴다. 관찰상 **zone을 `test`에서 `real`로 바꾸자 다른 약관 화면이 다시 떴다.**
  게임 키: `L` Hive 로그인 UI, `O` 로그아웃(로그인 UI가 떠 있는 동안에는 게임 키 입력을 무시한다).
  SDK의 이용약관 동의는 사람이 직접 해야 한다(동의 전에는 `setup` 응답이 오지 않고 HUD가 `setup...`에 머문다).
  관찰상 **구성(exe 폴더)마다 처음 한 번** 필요했다: Release에서 동의한 뒤 같은 Release 폴더의 새 인스턴스는 3초 만에 `ready`가 됐지만,
  Debug 폴더에서는 약관이 다시 떴다(저장 위치는 확인하지 못했다).
- Hive 어댑터 시험(`HiveCocos/`): `cmake --build DodgeGame\build-x64 --config Debug --target HiveCocosTests` 후 실행(SDK 불필요).
  실제 SDK 통합은 `--target HiveSdkProbe`(SDK 폴더를 작업 디렉터리로, **빌드 폴더의 원본 exe**를 실행. 복사본은 막힐 수 있다)
- 디버깅: `C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe` (x86 cdb는 x64 프로세스를 디버깅할 수 없다).
  `-G`로 실행. `-c "..."` 스크립트를 쓸 때는 `-g`를 쓰지 않는다(`-c`가 초기 중단에서 실행되기 때문. 대화형 디버깅은 `-g -G`도 된다).
  출력을 파일로 받으려면 리다이렉트 대신 `-logo <file>`을 쓴다(리다이렉트는 블록 버퍼링). x64에서 `this`는 `@rcx`.

## 디렉터리 규칙
- `DodgeGame/cocos2d/`는 엔진 junction이며 git에서 제외된다. **직접 수정하지 않는다.**
  엔진을 고쳐야 하면 `tools/patches/engine-3.17.2-x64/*.patch`에 패치로 추가한다(`setup_engine.ps1`이 멱등하게 적용).
  패치 파일은 LF 줄바꿈을 유지한다(`.gitattributes`). 패치 수정 절차: 엔진 파일을 직접 고친 뒤 원본(`%TEMP%\engine_orig`처럼 따로 보관한 사본)과
  `diff -u --label a/<경로> --label b/<경로>`로 패치를 다시 만들고, 원본 사본에 적용해 편집본과 바이트 일치하는지 확인한다.
  패치는 x64 전용이다(x86 분기는 Phase 7에서 제거했다).
- Hive SDK 연동 어댑터는 `HiveCocos/`(cocos 무의존 핵심 + `CocosDispatcher.cpp` 한 파일만 cocos 의존)에 둔다. 코드는 ASCII/영문 주석으로 쓴다.
- `tools/*.ps1`은 **PowerShell로 다시 쓰지 않는다**(BOM 변환 직후 파일이 사라진 사례가 반복됨). BOM이 필요하면 내용 맨 앞에 BOM 문자(U+FEFF)를 넣어 Write 도구로 한 번에 만든다.
- 게임 코드는 `DodgeGame/Classes/`에 둔다. 새 파일은 `DodgeGame/CMakeLists.txt`에 등록한다
  (레거시 `proj.win32` VS 프로젝트 파일은 x86 제거와 함께 삭제했다. `proj.win32/`의 `main.cpp`, `game.rc` 등은 CMake가 쓴다).
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

## 인계 (새 세션은 먼저 읽을 것)
- 현재 상태, 이 PC의 함정, 다음 작업(**다운로드 소스 SHA256 고정**, 브랜치 `phase9/sha256-pinning`)은 `docs/09-handoff.md`에 정리돼 있다.
- 미확인 항목(로그아웃, 자동 로그인, 한글 입력 IME, `<market>` 조합)은 확인된 것처럼 말하지 말고 미확인으로 다룬다.
