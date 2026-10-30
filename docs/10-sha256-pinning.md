# 10. 다운로드 소스 SHA256 고정 (Phase 9)

`tools/build_deps_x64.ps1`과 `tools/setup_engine.ps1`이 외부에서 받는 파일을 SHA256으로 고정하고, 불일치하면 지우고 중단한다.
(계획과 배경은 `docs/09-handoff.md` 5절. 이 문서는 실제로 한 일과 확인한 사실만 적는다.)

## 위협 모델
- **막는 것**: 업스트림 침해로 인한 악성 소스 혼입(특히 `mpg123`은 미리 빌드된 DLL을 그대로 복사한다), 같은 URL의 조용한 내용 변경, 재빌드 시 바이트 변화.
- **못 막는 것**: 처음 받은 파일이 이미 위변조됐다면(최초 신뢰 가정). 일부 항목만 업스트림 공개 서명/크기와 교차 확인했다(아래 표).

## 기준 해시 (기준점: 이 PC의 다운로드 캐시)
15개 라이브러리는 `%TEMP%\cocos2dx_deps_x64\dl\*.zip`, 엔진과 서브모듈은 `%TEMP%\cocos2dx_setup`의 **스크립트가 받은 그대로의 바이트**이며,
이 바이트로 x64 Debug/Release 빌드와 시험이 통과했다(2026-10-30에 계산).

| 이름 | 버전/출처 | SHA256 | 교차 확인 |
|---|---|---|---|
| zlib | v1.3.1 (GitHub tag) | `50B24B47BF19E1F35D2A21FF36D2A366638CDF958219A66F30CE0861201760E6` | 못 함(공개 체크섬 없음) |
| libpng | v1.6.16 (GitHub tag) | `631DAB2825E60139A0367448AD8AB8AD43A2855F7F46E651E8788AC84F53276F` | 못 함 |
| jpeg | IJG 9 (ijg.org) | `3320BB7D5BCD0C9ABCF89F2176BDB04334344B649FDAB1D85339BDA1E39B88B1` | 못 함 |
| tiff | 4.0.3 (osgeo) | `C77F0D23B4C6FEF96ED18DDC811990B35687848CCD2BACD7EDC327B5795B6481` | 못 함 |
| webp | v0.4.4 (GitHub tag) | `952E58039C197F22E6C9DA59A19AD18274BCE195FDB1DF9BD2763A6E262C660E` | 못 함 |
| freetype | VER-2-5-5 (GitHub tag) | `68D86CB1C2883513D33BAAA114ECBC04DEE12D788624C97C7C9415897ED62136` | 못 함 |
| glfw3 | 3.2.1 (GitHub tag) | `0C623F65A129C424D0FA45591694FDE3719AD4A0955D4835182FDA71B255446F` | 못 함 |
| glew | 2.2.0 (GitHub 릴리스 자산) | `A9046A913774395A095EDCC0B0AC2D81C3AACCA61787B39839B941E9BE14E0D4` | **크기만 일치**(릴리스 자산 1,334,840 B). 자산에 digest 필드가 비어 있어 해시는 대조 못 함 |
| sqlite3 | 3.45.3 amalgamation | `EA170E73E447703E8359308CA2E4366A3AE0C4304A8665896F068C736781C651` | 못 함(공식 다운로드 페이지에는 현재 3.54.1만 있고 3.45.3의 SHA3 값이 없다) |
| chipmunk | Chipmunk-7.0.1 (GitHub tag) | `37E250EF7CCD319C475DD3CB153D0E89BBEF3C25F0214602E165D6E8FCA5F490` | 못 함 |
| openal | openal-soft-1.19.1 (GitHub tag) | `059A1BDB65074FDE5FCE10CCAC073B9FC12A663069418A54A46804768D33244E` | 못 함 |
| ogg | v1.3.5 (GitHub tag) | `4FD31A41AB178BAF2F2204C48D67EC21292C69CCC03DF126451B4701862C13F8` | 못 함 |
| vorbis | v1.3.7 (GitHub tag) | `0580E02B3A3E566FB2178C5536D7E47E04EA2E90AC47C742D152AFC32324BEF4` | 못 함 |
| **mpg123** | 1.32.10 win64 **미리 빌드된 DLL** (mpg123.de) | `3299EDC2A29BB0B4A2015D1C2E2492C33575B21A66A021F58B9D2B79FDF6BA81` | **확인함**: 업스트림 PGP 분리 서명(`.sig`)이 유효하다(아래) |
| iconv | win-iconv v0.0.8 (GitHub tag) | `2C29910A86470B4695885E78390F960E586F524A89FE2FC5F2F5E257E690C328` | 못 함 |
| 엔진 | cocos2d-x-3.17.2 (GitHub tag) | `2C5029040877578AAF25D4F622F767003BCAD9AA39F181E08D136EE596F6854D` | 못 함 |
| cocos2d-console | 커밋 `2eb4073` | `82DA0E7544FEE768C62A4D30CBF74E6E0C753832B5E6C7A55B495BE5C77ECDA4` | 커밋 해시가 URL에 있다(내용 고정은 이미 되어 있었고, zip 바이트까지 고정) |
| bindings-generator | 커밋 `f85b28b` | `EAC244CC98EEC2E9ED3B9F5B29A10298B7835106E7DCA8DA46D1998DB5355A44` | 위와 같음 |
| 외부 번들 | `v3-deps-158` (cocos2d-x-3rd-party-libs-bin) | `D591197016FF6F16FB8FFF07B13BC7CB381EFBE9A177EA9A0C4089C303139757` | 아래 "외부 번들" |

### mpg123 서명 확인
- 업스트림 `mpg123-1.32.10-x86-64.zip.sig`(2024-12-14 서명)를 캐시 zip에 대해 `gpg --verify`로 검증했고 `Good signature`였다.
  서명 키: JonY `CAF5 641F 74F7 DFBA 88AE 2056 93BD B53C D4EB C740`(keyserver.ubuntu.com에서 받음).
- **한계**: 키 자체의 신원은 독립적으로 확인하지 않았다(웹 오브 트러스트 없음). "mpg123.de가 제공하는 서명과 그 키로 만든 파일이 일치한다"까지만 확인한 것이다.
- 업스트림은 SHA256/MD5 체크섬 파일을 제공하지 않는다(404).

### 외부 번들 (`v3-deps-158`)
- 캐시가 없었다(`download-deps.py -r yes`가 설치 후 지운다). 같은 URL에서 다시 받아 해시를 정했다.
- 받은 zip을 풀어 **현재 설치본(`external\`, 지금까지의 빌드에 쓴 내용)과 파일 단위로 비교**했다: 2,856개 중 2,845개는 크기와 SHA256이 같다.
  크기가 다른 11개는 `CMakeLists.txt` 9개(이 저장소의 엔진 패치가 고치는 파일들)와 `linux-specific\fmod`의 2개(번들에서는 0 바이트, 설치본에서는 10~11 바이트.
  `download-deps.py`의 `fix_fmod_link`가 만든 것으로 보이나 이 점은 코드를 읽어 확인하지 않았다)이다.
- zip 크기는 141,265,589 B로 `external\config.json`의 `zip_file_size`(141,265,643, 진행률 표시용 값)와 54 B 다르다.
  GitHub가 압축을 다시 만들었을 가능성이 있으나 원인은 확인하지 못했다. 내용은 위 비교로 설치본과 같음을 확인했다.
- 업스트림 공개 체크섬은 없다. 최초 신뢰 가정이 남는다.

## 구현
- `build_deps_x64.ps1`: `$script:Libs`의 각 항목에 `Sha256`을 두고, `Get-Source`가 **받은 직후와 캐시 zip을 재사용할 때 모두** `Get-FileHash`로 비교한다.
  `Sha256`이 없거나 형식이 틀린 항목은 실행을 거부한다(새 라이브러리를 고정 없이 추가할 수 없다).
- `setup_engine.ps1`: `Get-Zip`이 `.part`로 받고(중단 시 깨진 파일이 캐시로 남지 않는다) 같은 방식으로 검증한다. 엔진, 서브모듈 2개, 외부 번들에 적용한다.
  외부 번들은 스크립트가 직접 받아 검증한 zip을 `<엔진 루트>\v3-deps-158.zip`에 두고 `download-deps.py -r yes`를 실행한다.
  `download-deps.py`는 이 파일이 이미 있으면 내려받지 않고 그대로 쓴다(`download_zip_file`의 `os.path.isfile`). `finally`에서 남은 zip을 지운다.
  `external\config.json`의 `version`이 스크립트가 고정한 `v3-deps-158`과 다르면 중단한다.
- 불일치 시: zip(과 `build_deps_x64.ps1`은 추출 폴더)을 지우고, 기대값, 실제값, URL, 의심 원인을 담은 오류로 중단한다. 해시를 갱신하는 **우회 옵션은 없다**.
- 해시 검증은 라이브러리의 **빌드가 필요할 때만** 실행된다(`build_deps_x64.ps1`은 산출물이 이미 있으면 `skip`하고 소스를 받지 않는다).

## 시험 (2026-10-30, 실제 엔진 폴더는 건드리지 않고 임시 엔진 루트 사용)
| 시험 | 결과 |
|---|---|
| 정상: 캐시로 `-Only iconv,ogg` | 둘 다 OK |
| 부정: 캐시 `ogg.zip` 한 바이트 변조 후 `-Only iconv,ogg -Force` | ogg만 `SHA256 mismatch`로 FAIL(zip과 추출 폴더 삭제), iconv는 OK |
| 변이: 검증 조건을 무력화한 복사본으로, **유효한 zip이지만 내용이 다른** 변조본(파일 1개 추가)을 실행 | 변조본이 통과해 빌드됨 -> 검증 줄이 실제로 효과가 있음. 같은 변조본을 실제 스크립트로 실행하면 mismatch로 FAIL |
| 정상: `setup_engine.ps1` 콜드 실행(임시 엔진 루트, 엔진/서브모듈/번들 모두 검증 후 설치, 패치 3개 적용) | 통과(75초). 엔진 루트에 임시 zip이 남지 않음 |
| 부정: 캐시의 번들 zip 한 바이트 변조 후 `setup_engine.ps1` | 압축 해제 전에 mismatch로 중단, 설치되지 않음(`external\version.json` 없음), zip 삭제 |

시험 중 알게 된 점: 비트 하나를 뒤집은 zip은 `tar`의 CRC 검사에서 먼저 실패하므로 그것만으로는 SHA256 검증의 효과를 가릴 수 없다.
그래서 변이 시험은 유효한 zip으로 했다. 콜드 15개 재빌드(약 25분)는 하지 않았다.

## 해시 갱신 절차 (버전을 올릴 때)
1. 새 URL과 버전으로 파일을 받는다(스크립트는 해시가 틀리면 받은 파일을 지우므로, 수동으로 받아 `Get-FileHash -Algorithm SHA256`로 계산한다).
2. 가능하면 업스트림 서명/체크섬과 교차 확인한다(위 표의 mpg123 방식). 못 하면 "최초 신뢰"임을 이 문서에 적는다.
3. URL, 버전, `Sha256`을 **한 커밋에서 함께** 고치고 이 문서의 표를 갱신한다. 불일치가 났다고 해시만 바꾸지 않는다.
   **GitHub `archive/*.zip`은 압축 방식이 바뀌면 같은 태그라도 바이트가 달라질 수 있다**(과거에 실제로 있었다). 내용이 같은지(압축을 풀어 파일 단위 비교) 확인한 뒤에만 새 해시를 채택한다.
4. 새 해시로 라이브러리를 빌드하고 x64 Debug/Release 빌드와 시험을 통과시킨다.
