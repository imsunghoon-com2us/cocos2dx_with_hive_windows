# cocos2dx_with_hive_windows

cocos2d-x 3.17.2로 만드는 Windows 미니게임 **DodgeGame**("떨어지는 물체 피하기").
Claude Code로 개발, 리뷰, 디버깅을 진행하며 단계별 문서를 남긴다.

## 요구 환경
- Windows 10/11, Win32(x86) 빌드
- VS 2022 Build Tools (MSVC v141), CMake 3.31.x, Python 2.7.18

## 빠른 시작
엔진은 저장소에 포함되지 않는다. Phase 2 이후에는 다음 순서로 재현한다.
```powershell
tools\check_env.ps1      # 환경 점검
tools\setup_engine.ps1   # 엔진 복원
tools\build.ps1 -Config Release
```

## 문서
| 문서 | 내용 |
|---|---|
| [00-overview](docs/00-overview.md) | 목표, 툴체인, 로드맵, 진행 상태 |
| 01-environment-setup | 개발 환경 구축 (Phase 1) |
| 02-project-setup-and-build | 프로젝트 생성과 빌드 (Phase 2) |
| 03-game-design | 게임 설계 (Phase 3) |
| 04-review-and-debugging | 리뷰와 디버깅 (Phase 4) |
