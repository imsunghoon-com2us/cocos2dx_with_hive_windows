# 01. 개발 환경 구축 (Phase 1)

> 참고: 이 문서는 Win32(x86) 시절의 기록이다. Phase 6부터 빌드는 x64 전용이며, 현재 명령과 규칙은 `CLAUDE.md`를 따른다.

## 결과
`tools/check_env.ps1`이 모든 항목 OK(exit 0)를 반환한다.

| 구성요소 | 설치된 버전 | 위치 |
|---|---|---|
| git | 2.55.0 | PATH |
| GitHub CLI | 2.103.3 | `C:\Program Files\GitHub CLI\gh.exe` (PATH에 없음) |
| CMake | 3.31.8 | `C:\Program Files\CMake\bin` |
| Python | 2.7.18 | `C:\Python27\python.exe` |
| MSVC v141 | 14.16.27023 (VS2017, cl 19.16) | VS 2022 Build Tools (`...\2022\BuildTools`) |
| Windows SDK | 10.0.19041.0 | `C:\Program Files (x86)\Windows Kits\10` |
| cdb | Windows SDK Debugging Tools | `...\Windows Kits\10\Debuggers\x86\cdb.exe` |

Build Tools에는 v143(14.44)도 함께 설치되어 있지만 이 프로젝트는 `-T v141`만 사용한다.

## 설치 방법
```powershell
# 일반 권한
winget install Kitware.CMake --version 3.31.8 --silent
winget install Python.Python.2 --version 2.7.18150 --silent

# 관리자 권한 (UAC 승인 필요)
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Component.VC.v141.x86.x64 --add Microsoft.VisualStudio.Component.Windows10SDK.19041"
```
- v141만 지정해 설치하면 CMake의 `Visual Studio 17 2022` 생성기가 요구하는 v170 MSBuild 파일이 빠진다. 아래 명령으로 `VC.Tools.x86.x64`를 추가한다(Phase 2에서 확인).
  `--quiet`/`--passive`는 **관리자 권한으로 승격된 셸**에서만 동작한다(그렇지 않으면 종료 코드 5007). 일반 셸이면 `Start-Process -Verb RunAs`로 UAC 승인 창을 띄운다.
  ```powershell
  $bt = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
  Start-Process "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" -Verb RunAs -Wait -PassThru -ArgumentList "modify --installPath `"$bt`" --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --passive --norestart"
  ```
- `--scope user` 설치 관리자는 두 패키지 모두 없어서 스코프 없이 설치했다. MSI가 UAC 승인을 기다린다.
- winget의 CMake는 4.x가 최신이므로 버전을 반드시 고정한다.
- Python 2.7.18은 MSI가 x64이지만 cocos 콘솔 실행용일 뿐이라 빌드 대상(Win32)과 무관하다.

## 점검 스크립트
`tools/check_env.ps1`
- PATH에 없어도 기본 설치 경로(gh, cmake, cdb, `C:\Python27`)를 찾는다.
- `python`은 Microsoft Store 스텁(`WindowsApps`)을 제외한다.
- MSVC는 `vswhere`로 `VC.v141.x86.x64` 컴포넌트 유무를 확인한다.
- Python 2는 버전을 stderr로 출력하므로 `cmd /c`로 감싸 PowerShell 5.1의 NativeCommandError를 피한다.

## 알려진 사항
- GitHub 저장소는 private으로 생성했다: `imsunghoon-com2us/cocos2dx_with_hive_windows`. CLAUDE.md의 "public" 방침과 다르므로 필요할 때 public으로 전환한다. 전환 전에 커밋 작성자 이메일(회사 주소) 노출을 확인한다.
- Phase 0 시점에는 원격 저장소가 만들어지지 않은 상태였고, Phase 1에서 `gh auth login` 후 생성·푸시했다.
