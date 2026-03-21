# CMake + Visual Studio + Vcpkg

> [!CAUTION]
>
> CMake 빌드는 진행 중입니다.

## 전제 조건

- `cmake` >= 3.24.0
- [vcpkg.io](https://vcpkg.io/en/getting-started.html)의 `vcpkg`

Visual Studio 2022 버전 17.6부터 `vcpkg`가 배포에 포함되어 VS 개발자 명령 프롬프트에서 사용할 수 있으므로 별도로 설치할 필요가 없습니다.

## 구성

`CMakePresets.json`의 사전 설정 중 하나를 사용하여 구성할 수 있습니다. 모두 `out/build/<preset>/` 디렉토리에 코드를 빌드합니다.

### 터미널

`cmake`가 `vcpkg`를 찾을 수 있는지 확인합니다. 찾지 못하면 누락된 패키지에 대해 불평합니다. 다음 방법 중 하나로 수행할 수 있습니다:

- 사전 설치된 `vcpkg`가 있는 VS2022 사용자의 경우, 일반 터미널이 아닌 VS 개발자 명령 프롬프트를 실행하면 `vcpkg`를 이미 사용할 수 있어야 합니다.
- 모든 cmake 구성 명령에 `-DVCPKG_ROOT=C:\dev\vcpkg` (또는 경로가 무엇이든)를 추가합니다.
- 환경 변수 `VCPKG_ROOT`를 vcpkg 체크아웃 경로로 설정합니다.
- `CMakePresets.json`에 적절한 경로로 `VCPKG_ROOT` 캐시 변수를 추가합니다 (나중에 코드와 함께 작업할 계획이라면 권장하지 않음, git이 이 파일을 추적함).

명령 실행:

```sh
cmake --list-presets
```

사용 가능한 사전 설정을 표시합니다. 목록은 사용 중인 환경에 따라 변경됩니다. 비어 있으면 환경이 지원되지 않습니다.

명령 실행:

```sh
cmake --preset <preset>
```

`vcpkg` 설치를 사용할 수 있는 한 모든 의존성을 다운로드하고 빌드 파일을 생성합니다.

VS2022를 사용하는 경우 이름에 `2022`가 있는 사전 설정을 선택해야 합니다. 접미사가 없는 사전 설정은 VS2019를 대상으로 합니다.

명령에 `-Doption=value`를 추가하여 옵션을 재정의할 수 있습니다. [빌드 옵션](./cmake.md/#build-options)을 참조하세요. 예를 들어 테스트가 필요 없으면 `-DTESTS=OFF`로 테스트 빌드를 비활성화할 수 있습니다.

### Visual Studio

Visual Studio에서 게임 소스 폴더를 엽니다.

Visual Studio는 폴더를 CMake 프로젝트로 인식하고 구성을 시작하려고 시도할 수 있지만 적절한 사전 설정을 사용하지 않았기 때문에 실패할 가능성이 높습니다.

표준 도구 모음의 `Configuration` 드롭다운 상자에 사전 설정이 표시됩니다. 적절한 것 (`windows`와 `msvc`를 포함해야 함)을 선택한 다음 기본 메뉴에서 `Project` -> `Configure Cache`를 선택합니다.

VS2022를 사용하는 경우 이름에 `2022`가 있는 사전 설정을 선택해야 합니다. 접미사가 없는 사전 설정은 VS2019를 대상으로 합니다.

## 빌드

### 터미널

명령 실행:

- `cmake --build --preset <preset> --config Release`

`Release`를 `Debug`로 바꾸면 디버그 빌드를, `RelWithDebInfo`로 바꾸면 최적화는 적지만 디버그 정보는 더 많은 릴리스 빌드를 얻을 수 있습니다.

### Visual Studio

표준 도구 모음의 `Build Preset` 드롭다운 메뉴에서 빌드 사전 설정을 선택합니다. 기본 메뉴에서 `Build` -> `Build All`을 선택합니다.

UI 레이아웃에 따라 이 드롭다운 메뉴가 오버플로 버튼 뒤에 숨겨져 있을 수 있지만 `Release`, `Debug`, `RelWithDebInfo` 빌드 중에서도 선택할 수 있습니다.

## 번역

번역은 선택 사항이며 `gettext` 패키지의 `msgfmt` 바이너리가 필요합니다. `vcpkg`가 자동으로 설치해야 합니다.

### 터미널

명령 실행:

- `cmake --build --preset <preset> --target translations_compile`

### Visual Studio

Visual Studio는 이전 단계에서 번역을 빌드했어야 합니다. 그렇지 않은 경우 솔루션 탐색기를 열고 CMake Targets 모드로 전환한 다음 (마우스 오른쪽 버튼 클릭으로 가능) `translations_compile` 대상을 마우스 오른쪽 버튼으로 클릭 -> `Build translations_compile`.

## 설치

> [!CAUTION]
>
> 설치는 아직 진행 중이며 테스트가 거의 없었습니다.

### Visual Studio

기본 메뉴에서 `Build` -> `Install CataclysmBN`을 선택합니다.

### 터미널

명령 실행:

- `cmake --install out/build/<preset>/ --config Release`

선택한 빌드 유형으로 `Release`를 바꿉니다.

## 실행

게임 및 테스트 실행 파일은 모두 `.\Release\` 폴더에서 사용할 수 있습니다 (폴더 이름은 빌드 유형과 일치하므로 다른 빌드 유형의 경우 다른 폴더 이름을 얻음).

터미널에서 수동으로 실행할 수 있습니다. 프로젝트의 최상위 디렉토리에서 실행해야 합니다. 기본적으로 게임은 현재 경로에 데이터 파일이 있을 것으로 예상합니다.

Visual Studio에서 실행하고 디버깅하려면 `out\build\<preset>\CataclysmBN.sln`에 있는 생성된 VS 솔루션을 열고 (이전 단계를 IDE 또는 터미널에서 완료했는지 여부에 관계없이 존재함) 대신 그것으로 추가 작업을 수행하는 것이 좋습니다.

또는 "Open Folder" 모드에서 유지할 수 있지만 게임 실행 파일 (및 테스트)에 대한 시작 구성을 사용자 지정해야 하며 아직 발견되지 않은 다른 부작용이 있을 수 있습니다.

### 터미널

게임을 시작하려면 다음을 실행합니다:

- `.\Release\cataclysm-bn-tiles.exe`

테스트를 실행하려면 다음을 실행합니다:

- `.\Release\cata_test-tiles.exe`

### Visual Studio (옵션 1, 권장)

Visual Studio를 닫은 다음 `out\build\<preset>\`로 이동하여 `CataclysmBN.sln`을 엽니다. `cataclysm-bn-tiles`를 시작 프로젝트로 설정하면 (솔루션 탐색기에서 마우스 오른쪽 버튼 클릭으로 가능) 추가 문제 없이 게임 실행 파일을 실행하고 디버그할 수 있습니다. 최상위 프로젝트 디렉토리에서 데이터 파일을 찾도록 이미 미리 구성되어 있습니다.

테스트를 실행하려면 시작 프로젝트를 `cata_test-tiles`로 전환합니다.

### Visual Studio (옵션 2)

Visual Studio가 CMake 프로젝트를 처리하는 방식으로 인해 VS가 "Open Folder" 모드에 있는 동안 실행 파일의 작업 디렉토리를 지정할 수 없습니다. StackOverflow 답변에서 잘 설명합니다: https://stackoverflow.com/a/62309569 다행히 VS는 개별적으로 실행 파일 시작 옵션을 사용자 지정할 수 있습니다.

솔루션 탐색기를 열고 아직 CMake Targets 모드로 전환하지 않았다면 전환합니다 (마우스 오른쪽 버튼 클릭으로 가능). `cataclysm-bn-tiles` 대상을 마우스 오른쪽 버튼으로 클릭 -> `Add Debug Configuration`. Visual Studio는 `cataclysm-bn-tiles` 대상에 대한 새 구성과 함께 이 프로젝트의 시작 구성 파일을 엽니다. 다음 줄을 추가합니다:

```
"currentDir": "${workspaceRoot}",
```

구성에 추가하고 파일을 저장합니다.

최종 결과는 다음과 같아야 합니다:

```json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "currentDir": "${workspaceRoot}",
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)",
      "name": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)"
    }
  ]
}
```

이제 Visual Studio 내에서 게임 실행 파일을 실행하고 디버그할 수 있어야 합니다.

테스트를 실행하려면 `cata_test-tiles` 대상에 대해 이 프로세스를 반복합니다.
