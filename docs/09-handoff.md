# 09. 인계 문서 (2026-10-30) - 다운로드 소스 SHA256 고정 (구현 완료, 아래는 작성 당시 계획)

> **갱신(2026-10-30)**: 아래 5절의 계획은 `phase9/sha256-pinning` 브랜치에서 구현을 마쳤다. 실제 결과와 시험은 `docs/10-sha256-pinning.md`를 본다.
> 계획과 달라진 점: ① **Hive SDK도 포함**(사용자 요청): 항상 최신이 아니라 `tools/hive_sdk.json`의 고정 버전·해시로 `tools/fetch_hive.ps1`이 받고, 최신 확인은 `-CheckLatest`, 갱신은 `-Update`.
> ② 외부 번들은 `download-deps.py`를 고치지 않고, 스크립트가 받아 검증한 zip을 엔진 루트에 두는 방식으로 검증한다. 남은 일: `main` 병합(사용자 확인 후), 6절의 나머지 작업.

새 세션에서는 이 문서와 `CLAUDE.md`를 먼저 읽는다. (이전 안내: "SHA256 고정 작업부터 이어서 진행해줘")
(같은 내용의 작업 메모가 `C:\Users\COM2US\.claude\plans\bubbly-frolicking-seal.md`에도 있지만 저장소 밖이라 자동으로 읽히지 않는다.)

## 1. 한 줄 요약
cocos2d-x 3.17.2 Windows 미니게임 `DodgeGame`에 **Hive SDK v4(UE5용 Windows 패키지, x64 전용)**를 연동하는 프로젝트.
GitHub private 저장소 `imsunghoon-com2us/cocos2dx_with_hive_windows`. 문서와 대화는 한국어.

## 2. 현재 상태 (확인된 사실)
- **`main` = `33d5651`**(Phase 5~8 병합). 단계 브랜치 `phase5`~`phase8`은 삭제하지 않고 남겼다. 이 문서는 `phase9/sha256-pinning`에서 시작한다.
- 단계: 0~4(Win32 시절) -> 5 x64 전환 -> 6 Hive 초기화+설정 -> 7 엔진 패치 x86 분기 정리 -> 8 로그인+AppId+Zone. 문서는 `docs/01~06`, `docs/08`(Phase 7은 `docs/05`의 "Phase 7" 절).
- **x64 전용**이다(x86 제거: `build.ps1`은 `-Arch`를 거부하고, `DodgeGame/CMakeLists.txt`가 Win32 configure를 막는다).
- 병합된 `main`에서 재검증: `check_env` 모두 OK, x64 Debug/Release 빌드 오류 0, 단위 시험 **119개** 통과, `real` 존 실제 SDK `ready (providers 7)`와 `L` 키 로그인 UI 표시.
- **사용자가 `real` 존 약관에 동의하고 로그인 동작을 확인**했다(provider와 HUD 문구 세부는 전달받지 못함).
  **로그아웃, 자동 로그인, 한글 입력(IME)은 확인 결과를 못 받아 미확인**이다.
- PC 설정: AppId = SDK 공식 패키지 샘플의 Microsoft Store용 AppId(공개 값, 노출 무방 - 사용자 확인), `zone=real`, `<market>GO</market>`(그대로, Microsoft Store와의 조합은 미확인).
  값은 커밋되지 않는 `hive.local.json`(`appId`, `zone`)에서 `tools\setup_hive.ps1`이 읽어 `Resources\hive_config.xml`에 적용한다.

## 3. 구조와 명령 (자세한 규칙은 `CLAUDE.md`)
- `tools\`: `build.ps1`, `check_env.ps1`, `setup_engine.ps1`, `build_deps_x64.ps1`, `setup_hive.ps1`, `deps-x64\`, `patches\engine-3.17.2-x64\*.patch`
- `HiveCocos\`: Hive 어댑터(핵심은 cocos 무의존, `CocosDispatcher.cpp`만 cocos 의존), `tests\`(`HiveCocosTests`, `HiveSdkProbe`)
- `DodgeGame\Classes\`: `HiveService.*`(시작, setup, 로그인, HUD 상태), `GameScene.*`(`L` 로그인 UI, `O` 로그아웃)
- 엔진 본체는 저장소 밖 `C:\Project\cocos2d-x-3.17.2`, x64 라이브러리 작업 캐시는 `C:\Project\cocos2d-deps-x64`.
- SDK 패키지 `HiveSDKv4_unreal\`은 git 제외(독점 + 키/토큰 포함, 991개 파일 약 623MB).
- 빌드 `tools\build.ps1 -Config Debug|Release`, SDK 배치 `tools\setup_hive.ps1 -Config Debug|Release`,
  시험 `cmake --build DodgeGame\build-x64 --config Debug --target HiveCocosTests`(SDK 불필요) 후 실행,
  실제 SDK 통합 `--target HiveSdkProbe`(SDK 폴더를 작업 디렉터리로 하고 **빌드 폴더의 원본 exe**를 실행).
- 커밋 `[Phase N] 요약` + 끝에 `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. 단계별 브랜치에서 작업하고, 검증 후 사용자 확인을 받아 `main`에 `--no-ff` 병합.
  **`git add -A` 금지(경로 지정 스테이징)**, 커밋 전 비밀 스캔과 `tools/` 파일 존재 확인.

## 4. 이 PC/환경의 함정 (같은 시행착오 방지)
- **`tools/*.ps1`을 PowerShell로 다시 쓰면 파일이 사라진 사례가 반복됐다**(원인 불명, 보안 솔루션 추정이나 미확인). 수정은 Edit 도구로만 한다.
  새 `.ps1`은 내용 맨 앞에 BOM 문자(U+FEFF)를 넣어 Write 도구로 한 번에 만든다. `.ps1`과 한글 주석이 든 `.cpp/.h`는 UTF-8 BOM이 필요하다.
- **`build-x64`에 `-Clean`을 쓰지 말 것**: SDK 런타임(구성당 약 740MB)과 약관 동의 상태가 들어 있다.
- 사용자가 게임 창을 열어 둘 수 있다. 빌드와 `setup_hive` 전에 `Get-Process DodgeGame`을 확인하고, 실행 중이면 닫아 달라고 요청한다(임의 종료 금지).
- `sed`로 백슬래시 경로를 치환하면 조용히 실패한다 -> PowerShell `String.Replace`. bash의 `grep -c $'\r'`은 모든 줄을 센다 -> `tr -cd '\r' | wc -c`.
- PowerShell `*>`/`Tee-Object` 로그는 **UTF-16**이라 `grep`이 못 읽는다 -> `Get-Content`. cocos `log()` 콘솔 출력은 cp949.
- 명령 안전 검사가 `Remove-Item`+`cmd /c`, `Start-Sleep` 뒤 `Get-Content` 같은 조합을 오탐해 막는다 -> 명령을 분리하고, 대기는 `run_in_background`의 `until` 루프로 한다.
- SDK 폴더로 새로 복사한 exe는 보안 솔루션이 잠글 수 있다(`Device or resource busy`). 빌드 폴더의 원본을 실행한다.
- `hive_config.xml`에는 키(AppsFlyer 등)가 있다. **통째로 출력하지 말고** 필요한 줄만 값을 가려서 본다.
- 약관 동의 화면은 사람이 눌러야 하며 대신 동의하지 않는다. 동의 상태는 구성(Debug/Release)과 zone마다 따로 필요한 것으로 관찰됐다(저장 위치 미확인).
  현재 Release는 `real` 동의 완료, **Debug는 `real` 미동의**.
- 시험용 보조 스크립트(창 메시지로 키 보내기, 스크린샷, cdb 실행 등)는 세션 임시 폴더에만 있고 저장소에 없다. 필요하면 다시 만든다.
- 시험은 **프로세스 경로와 비트 수를 출력해 증명**한다(`sed` 실패로 "x64 시험"이 사실 Win32를 돌린 적이 있다).

## 5. 다음 작업: 다운로드 소스 SHA256 고정 (Phase 9, 브랜치 `phase9/sha256-pinning`)
### 이유
`build_deps_x64.ps1`과 `setup_engine.ps1`은 외부에서 받은 소스를 **무결성 검증 없이** 컴파일해 게임에 넣는다. HTTPS는 서버 신원만 보장하고 내용은 보장하지 않는다.
- 막는 것: 업스트림 침해로 인한 악성 소스 혼입(특히 `mpg123`은 미리 빌드된 DLL을 그대로 복사한다), 같은 URL의 조용한 내용 변경, 재빌드 시 바이트 변화.
- 못 막는 것: 처음 받은 파일이 이미 위변조됐다면(최초 신뢰 가정). 일부는 업스트림 공개 체크섬과 교차 확인할 수 있다.
- 필수 시점: **public 전환, CI/타인 빌드, 게임 배포** 중 하나가 올 때. ("반드시"라고 한 이전 표현은 과장이었다.)

### 범위: 19개 다운로드, Hive SDK는 제외
- `build_deps_x64.ps1` 15개: zlib, libpng, jpeg, libtiff, libwebp, freetype, glfw, GLEW, sqlite3, chipmunk, OpenAL Soft, ogg, vorbis, **mpg123(미리 빌드된 DLL, 최우선)**, iconv.
  (라이브러리 표 `$script:Libs`의 `Url`, 다운로드 함수 `Get-Source`)
- `setup_engine.ps1` 2개: cocos2d-x 3.17.2 태그 zip(`$EngineUrl`), 외부 번들 `v3-deps-158`(`download-deps.py`가 받으며 파일 크기를 진행률용으로만 쓰고 해시 검증 없음. 번들의 헤더와 소스는 게임에 컴파일된다).
- `cocos2d-console`, `bindings-generator`: URL에 전체 커밋 해시가 있어 내용은 이미 고정이다.
- CMake/Python/VS Build Tools는 winget이 자체 해시로 검증한다.
- (계획 당시 판단, **이후 변경됨**: 위 갱신 참고) **Hive SDK는 대상이 아니다**: 스크립트가 내려받지 않고, 업데이트가 정상 사용이라 고정하면 갱신 때마다 막힌다. 선택 사항으로 **버전 추적**만 한다:
  `setup_hive.ps1`이 패키지 `version.json`(현재 `26.7.0`)을 출력하고 시험한 버전과 다르면 경고(실패 아님). 런타임 `getHiveSDKVersion`은 `4.26.7.0`으로 표기가 다르다.

### 해시의 기준점(캐시)
- 15개: `%TEMP%\cocos2dx_deps_x64\dl\*.zip`(스크립트가 받은 그대로, 빌드와 시험이 통과한 바이트). 임시 폴더 정리로 사라질 수 있으니 **가장 먼저 해시를 계산해 기록**한다.
- 엔진 zip, 서브모듈 zip: `%TEMP%\cocos2dx_setup`. 외부 번들 zip은 `download-deps.py -r yes`가 설치 후 삭제하므로 캐시에 없다 -> 다시 받아 해시를 정해야 하며, 이때는 최초 신뢰 가정과 업스트림 대조가 필요하다.
- **GitHub `archive/refs/tags/*.zip`은 압축 방식이 바뀌면 같은 태그라도 바이트가 달라질 수 있다**(과거에 실제로 있었다). 해시 불일치가 침해가 아니라 압축 변경일 수 있음을 오류 메시지와 문서에 적고, 대안(릴리스 자산 URL, 커밋 해시 병기)을 검토한다.

### 구현 순서(제안)
1. 캐시 파일의 SHA256을 계산해 표로 이 폴더의 새 문서(`docs/10-sha256-pinning.md` 등)에 기록한다(동작 변경 없음).
2. 가능한 항목은 업스트림 공개 체크섬(sqlite, zlib, GLEW 등)과 교차 확인하고 "확인함/못 함"을 구분해 적는다. mpg123 DLL이 최우선.
3. `$script:Libs`에 `Sha256` 필드를 추가하고 `Get-Source`에서 **다운로드 직후와 캐시 재사용 시 모두** `Get-FileHash -Algorithm SHA256`로 비교한다.
   불일치하면 파일을 지우고 라이브러리 이름, 기대값, 실제값, 의심 원인을 담은 오류로 중단한다. 해시를 갱신하는 우회 옵션은 만들지 않는다(버전 갱신 = URL, 버전, 해시를 함께 수정하는 절차를 문서화).
4. `setup_engine.ps1`의 엔진 zip과 외부 번들도 검증한다. 번들은 `download-deps.py`가 받으므로 스크립트가 zip을 직접 받아 검증 후 설치하게 하거나 설치 후 파일 목록 해시로 대체하는 방안 중 택일한다(구현 전 `download-deps.py` 흐름을 다시 읽을 것).
5. `docs/05`의 "알려진 한계"(해시 미고정)를 갱신하고, `CLAUDE.md`에 갱신 절차를 추가하고, `docs/00-overview.md`의 Phase 9 항목을 완료로 바꾼다.

### 검증(적용 후)
- 정상: 캐시 파일로 `build_deps_x64.ps1`이 15개 모두 통과(가능하면 `-Force -WorkRoot <새 폴더>` 콜드 재현 1회), `setup_engine.ps1` 통과.
- 부정 시험: 캐시 zip 한 개의 바이트를 바꾸거나 해시를 틀리게 하면 그 라이브러리에서 명확한 오류로 중단되고 다른 항목은 영향이 없다(시험 후 원복).
  변이 확인: 검증 줄을 빼면 부정 시험이 통과해 버리는지 확인한다.
- 회귀: x64 Debug/Release 빌드, `HiveCocosTests` 119개, 실제 SDK Release 실행 `ready`.
- 커밋 전: `git status`, 경로 지정 스테이징, 비밀 스캔, `tools/` 파일 존재 확인. 병합은 사용자 확인 후 `--no-ff`.

## 6. 그 밖의 남은 작업 (우선순위는 사용자와 협의)
1. **public 전환 체크리스트**: SHA256 고정, 커밋 작성자 이메일(`imsunghoon@com2us.com`)이 공개 이력에 남는 점의 확인과 결정, README 정리, 저장소 공개 설정.
2. 로그인 미확인 항목: 로그아웃(`O`), 자동 로그인(`isAutoSignIn`; 최근 자동 로그인이 안 된 것은 오류가 아니라 "로그인 상태 유지" 선택 여부일 가능성이 있으나 미확인),
   **한글/키보드 입력(IME)**: UE는 `showSignIn` 전후로 CEF IME 함수(`HIVESDKCEFImeEnable/Disable`, HIVEUIFramework의 `setImeProcess` 등)를 호출하는데 이 어댑터는 호출하지 않는다. 입력창이 안 되면 연동이 필요하다.
3. 결제(IAP) 연동: `IAPV4` 클래스(SDK 소스 `HiveIAPV4Impl.cpp` 참고). Microsoft Store AppId와 `<market>` 값의 조합 확인이 필요하다.
4. `<market>GO</market>`와 Microsoft Store AppId 조합 확인. 요청의 `engine`/`platform` 값은 `"cpp"`만 시험했다(서버 쪽 분석에서 구분이 필요하면 SDK 소스(GitHub)가 필요).
5. 정리: `proj.android/ios_mac/linux`는 미사용(깨져 있음), Debug 폴더 `real` 약관 미동의, Hive SDK 버전 추적(선택).
6. 원인 불명 현상 기록: `tools/*.ps1` 소실. Symantec 이벤트에 우리 파일 언급은 0건이었지만 격리 목록은 관리자 권한이 없어 확인하지 못했다.

## 7. 이 프로젝트에서 반복한 실수 (재발 방지)
- 설정 파일 확인 중 `grep` 패턴이 넓어 SDK 샘플의 AppsFlyer 키 값이 출력에 한 번 노출됐다(저장소에는 들어가지 않았다). 항상 값을 가려서 본다.
- UTF-16 로그를 `grep`으로 읽어 "끝나지 않은 것"으로 오판한 적이 여러 번 있다. 대기용 감시 명령은 인코딩부터 확인한다.
- 가정을 확인하지 않고 명령을 실행해(예: 이미 있는 exe 때문에 실제 복사 경로로 진입) 의도와 다른 동작을 한 적이 있다.
