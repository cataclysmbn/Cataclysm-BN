# Visual Studio 2022 및 CMake로 빌드하기

이 가이드는 Visual Studio 2022의 네이티브 CMake 통합을 사용해 Windows에서 Cataclysm: Bright Nights를 빌드하는 방법을 설명합니다. 한 번 설정한 뒤에는 구성, 빌드, 디버깅을 모두 외부 도구 없이 Visual Studio 안에서 수행합니다.

> **레거시 빌드:** `msvc-full-features/`의 `.sln` 기반 빌드는 여전히 작동하며 이 시스템의 영향을 받지 않습니다. 같은 체크아웃에서 두 방식을 함께 사용할 수 있습니다.

## 작동 방식

프로젝트에는 두 개의 CMake 구성 파일이 있습니다:

| 파일                 | 사용처               |
| -------------------- | -------------------- |
| `CMakeSettings.json` | Visual Studio IDE    |
| `CMakePresets.json`  | cmake CLI, CI, Linux |

폴더를 열면 VS가 `CMakeSettings.json`을 직접 읽습니다. VS를 열기 전에 수동으로 cmake를 구성할 필요가 없습니다.

> **VS 설정:** **Tools → Options → CMake**에서 _"When a CMakeSettings.json or CMakePresets.json file is detected"_를 **"Use CMakeSettings.json (Legacy)"** 또는 **"Never use CMake Presets"**로 설정하세요. 그러면 VS가 `CMakeSettings.json`을 사용하고 `CMakePresets.json`을 무시합니다.

> [!TIP]
>
> Visual Studio에서 시작하는 프롬프트 기반 워크플로를 선호한다면 [Visual Studio 외부 도구 자동화 (Windows + WSL)](./vs_external_tool_wsl.md)를 참조하세요.

## 전제 조건

| 도구                                          | 최소 버전 | 구하는 곳                                                         |
| --------------------------------------------- | --------- | ----------------------------------------------------------------- |
| Visual Studio 2022                            | 17.6      | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| VS 워크로드: **Desktop development with C++** | —         | VS Installer                                                      |
| cmake                                         | 3.24      | 위 VS 워크로드에 포함                                             |
| ninja                                         | 모든 버전 | 위 VS 워크로드에 포함                                             |
| vcpkg                                         | 모든 버전 | VS 2022 17.6+에 포함 (아래 참조)                                  |
| git                                           | 모든 버전 | [git-scm.com](https://git-scm.com/)                               |

### vcpkg

Visual Studio 2022 17.6 이상에는 vcpkg가 포함됩니다. 권장 설치 옵션을 사용했다면 이미 설치되어 있습니다. VS 개발자 환경이 설정하는 `VCPKG_INSTALLATION_ROOT` 환경 변수를 통해 CMake가 자동으로 찾습니다.

별도의 vcpkg를 설치했다면 `VCPKG_ROOT`를 해당 경로로 설정하면 CMake가 사용합니다.

---

## Visual Studio에서의 일상적인 워크플로

### 1. 폴더 열기

Visual Studio 2022를 열고 **File → Open → Folder…**를 선택한 뒤 `CMakeLists.txt`가 있는 프로젝트 루트 디렉터리를 선택합니다.

`msvc-full-features/`의 `.sln` 파일은 열지 마세요. 이는 레거시 빌드 시스템이며 두 시스템은 서로 분리되어 있습니다.

### 2. 구성 선택

표준 도구 모음에서 **Configuration** 드롭다운을 열고 선택합니다:

| 구성             | 용도                                      |
| ---------------- | ----------------------------------------- |
| `Debug`          | 디버깅, 모든 심볼, 최적화 없음            |
| `RelWithDebInfo` | 일반 개발 — 최적화되지만 디버깅 가능      |
| `Release`        | 성능 테스트, 배포                         |
| `Tests`          | 테스트 모음 빌드 및 실행                  |
| `Tracy`          | Tracy 프로파일러를 사용한 성능 프로파일링 |

> **RelWithDebInfo**가 일상 개발의 기본값으로 가장 적합합니다. 게임은 정상 속도로 실행되면서 중단점과 스택 추적에 필요한 디버그 정보를 유지합니다.

`Tests` 구성은 테스트 모음을 활성화한 RelWithDebInfo 빌드입니다. 나머지 구성은 빌드 시간을 줄이기 위해 테스트를 비활성화합니다.

`Tracy` 구성은 Tracy 프로파일러 계측을 포함한 Release 빌드입니다. [Tracy 프로파일링](#tracy-프로파일링)을 참조하세요.

### 3. 빌드

**Build → Build All**(또는 `Ctrl+Shift+B`)을 선택합니다.

첫 빌드에서는 vcpkg 의존성을 다운로드하고 컴파일하므로 시간이 걸립니다. 이후 빌드는 증분 빌드입니다.

### 4. 실행 및 디버깅

도구 모음에서 시작 항목을 선택합니다:

| 구성                                     | 시작 항목                  |
| ---------------------------------------- | -------------------------- |
| Debug / RelWithDebInfo / Release / Tracy | **cataclysm-bn-tiles.exe** |
| Tests                                    | **cata_test-tiles.exe**    |

그 다음 **F5**를 누릅니다.

작업 디렉터리는 `launch.vs.json`을 통해 프로젝트 루트로 설정되므로 추가 설정 없이 게임이 데이터 파일을 찾습니다.

---

## 빌드 사용자 지정

로컬 빌드의 cmake 변수를 덮어쓰려면 `CMakeSettings.json`을 열고 사용할 구성의 `variables` 배열에 항목을 추가합니다. 이 파일은 git으로 추적되므로 개인 설정은 로컬 브랜치에서 편집하거나 구성을 복사해 새 이름을 사용하세요.

### 유용한 변수

| 변수          | 기본값                      | 효과                     |
| ------------- | --------------------------- | ------------------------ |
| `TESTS`       | `OFF` (Tests 구성에서는 ON) | 테스트 모음 빌드         |
| `JSON_FORMAT` | `ON`                        | JSON formatter 도구 빌드 |
| `LOCALIZE`    | `ON`                        | 번역 지원 빌드           |
| `SOUND`       | `ON`                        | 오디오 지원 빌드         |

---

## Tracy 프로파일링

[Tracy](https://github.com/wolfpld/tracy)는 실시간 프레임 프로파일러입니다. VS 도구 모음에서 **Tracy** 구성을 선택하고 평소처럼 빌드하세요. Tracy는 `TRACY_ON_DEMAND` 모드를 사용하므로 Tracy 뷰어가 연결되어 녹화를 시작할 때만 프로파일링하며, 뷰어 없이도 게임을 사용할 수 있습니다.

터미널 워크플로에서도 `windows-tiles-sounds-x64-msvc-tracy` cmake 사전 설정으로 Tracy를 사용할 수 있습니다. [터미널 워크플로](#터미널-워크플로)를 참조하세요.

---

## 터미널 워크플로

`setup.ps1`은 전제 조건을 확인하고 터미널 빌드에 사용할 cmake 사전 설정을 구성합니다. 일반 PowerShell 창에서 한 번 실행하세요:

```powershell
.\setup.ps1
```

스크립트는 전제 조건을 확인하고 번역 빌드에 필요한 gettext 바이너리를 다운로드한 다음 `cmake --preset windows-tiles-sounds-x64-msvc`를 실행합니다.

그 후 **VS 2022 Developer Command Prompt** 또는 **Developer PowerShell**에서 표준 cmake 명령을 사용할 수 있습니다:

```powershell
# 한 번 구성 (또는 CMakeLists.txt 변경 후)
cmake --preset windows-tiles-sounds-x64-msvc

# 빌드
cmake --build --preset windows-msvc-relwithdebinfo

# 프로젝트 루트에서 게임 실행
.\out\build\windows-tiles-sounds-x64-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe

# 테스트 실행
.\out\build\windows-tiles-sounds-x64-msvc\tests\RelWithDebInfo\cata_test-tiles.exe

# 번역만 빌드
cmake --build --preset windows-msvc-relwithdebinfo --target translations_compile

# 설치 (게임과 데이터를 독립 실행 디렉터리에 복사)
cmake --install out\build\windows-tiles-sounds-x64-msvc --config RelWithDebInfo
```

> **참고:** 일반 터미널(VS 개발자 터미널이 아님)에서 `cmake --build`를 실행하면 `CMakeUserPresets.json`에 저장된 VS 환경을 사용합니다. 이 파일이 없으면 `setup.ps1`로 다시 생성하거나 VS 개발자 명령 프롬프트를 사용하세요.

---

## 문제 해결

### CMake 구성이 즉시 실패함

**가장 흔한 원인:** vcpkg를 찾지 못함.

`VCPKG_ROOT`가 설정되어 있는지(또는 VS 번들 vcpkg를 사용할 수 있는지) 확인하세요. VS 개발자 명령 프롬프트를 열고 다음을 실행합니다:

```
echo %VCPKG_ROOT%
echo %VCPKG_INSTALLATION_ROOT%
```

둘 중 하나는 `vcpkg.exe`가 있는 디렉터리를 가리켜야 합니다. 둘 다 설정되지 않았다면 `setup.ps1`을 실행하세요. VS 번들 vcpkg를 자동으로 찾습니다.

### VS에 `x64-Debug` 구성이 표시되거나 ncurses 오류가 나타남

VS가 `CMakeSettings.json`을 사용하지 않는 것입니다. **Tools → Options → CMake → General**에서 preset 통합을 **"Use CMakeSettings.json (Legacy)"** 또는 **"Never use CMake Presets"**로 설정한 뒤 아래의 전체 초기화를 수행하세요.

### 구성은 성공하지만 헤더/라이브러리가 없어 빌드가 실패함

VS 환경(`INCLUDE`, `LIB`, `PATH`)이 올바르게 캡처되지 않았을 수 있습니다. 다음을 시도하세요:

1. 프로젝트 루트의 `CMakeUserPresets.json`을 삭제합니다.
2. 관련 `out\build\` 하위 디렉터리를 완전히 삭제합니다.
3. `setup.ps1`을 다시 실행해 둘 다 생성합니다.

### 게임이 즉시 충돌하거나 데이터를 찾지 못함

프로젝트 루트의 `launch.vs.json`은 F5 실행의 작업 디렉터리를 프로젝트 루트로 설정합니다. 파일이 없거나 VS가 읽지 않으면 `./data/`를 찾지 못할 수 있습니다.

파일 탐색기나 터미널에서 `.exe`를 직접 실행한다면 프로젝트 루트에서 실행하세요:

```powershell
# 올바름 — 프로젝트 루트에서 실행
.\out\build\win-rel-deb\src\cataclysm-bn-tiles.exe

# 잘못됨 — ./data/를 찾지 못함
cd out\build\win-rel-deb\src
.\cataclysm-bn-tiles.exe
```

### 구성 중 `VsDevCmd.bat not found` 오류

`VsDevCmd.bat`은 VS 설치 폴더에 있습니다. 이 오류가 나타나면 VS가 표준 위치가 아닌 곳에 설치되었을 수 있습니다. cmake 실행 전에 `DevEnvDir` 환경 변수를 VS `Common7\IDE` 디렉터리 경로로 설정하세요:

```powershell
$env:DevEnvDir = "D:\VisualStudio\Common7\IDE\"
cmake --preset windows-tiles-sounds-x64-msvc
```

### 빌드가 매우 느림

설치되어 있고 `PATH`에 있다면 ccache가 자동으로 감지되어 사용됩니다. [ccache.dev](https://ccache.dev/)에서 설치하면 `git clean` 또는 브랜치 전환 후 증분 빌드 속도가 크게 향상됩니다.

### 빌드 환경을 완전히 초기화하는 방법

문제가 생겨 깨끗한 상태가 필요하다면:

```powershell
# VS의 캐시된 프로젝트 상태 (오래된 구성, IntelliSense DB) 삭제
Remove-Item -Recurse -Force .vs

# 모든 빌드 출력 디렉터리 삭제
Remove-Item -Recurse -Force out\build

# 생성된 사용자 사전 설정 삭제 (다음 구성에서 다시 생성됨)
Remove-Item -Force CMakeUserPresets.json

# 터미널 빌드를 위해 setup 재실행 (다음 VS 열 때도 다시 구성됨)
.\setup.ps1
```

### 재구성할 때 CMakeUserPresets.json이 "already exists"라고 표시됨

의도된 동작입니다. 사용자 지정 설정을 덮어쓰지 않도록 이 파일은 최초 구성에서만 생성됩니다. 기본 생성 내용으로 초기화하려면 삭제한 뒤 `setup.ps1`을 실행하세요.
