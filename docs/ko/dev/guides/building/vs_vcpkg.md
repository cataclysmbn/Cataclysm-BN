# Visual Studio + vcpkg

Visual Studio와 vcpkg를 사용하여 Windows에서 Cataclysm-BN을 컴파일하는 데 필요한 단계를 포함한 가이드입니다.

현재 가이드의 단계는 Windows 10 및 11 (64비트), Visual Studio 2019 및 2022 (64비트)에서 테스트되었지만 다른 버전의 Windows 및 Visual Studio에서도 약간의 수정으로 작동해야 합니다.

## 전제 조건:

- 최신 Windows 운영 체제가 설치된 컴퓨터 (Windows 10 또는 11), Windows 7 및 8.1은 작동을 보장하지 않음
- 약 15GB 여유 공간이 있는 NTFS 파티션 (Visual Studio에 약 10GB, vcpkg 설치에 약 1GB, 저장소에 약 3GB, 빌드 캐시에 약 1GB)
- Git for Windows ([Git 홈페이지](https://git-scm.com/)에서 다운로드 가능)
- Visual Studio 2019 또는 2022
  - **참고**: Visual Studio 2022를 사용하는 경우 vcpkg 버그를 해결하려면 Visual Studio 2019 컴파일러를 설치해야 합니다. Visual Studio Installer에서 'Individual components' 탭을 선택하고 'MSVC v142 - VS 2019 C++ x64/x86 Build Tools'와 같은 구성 요소를 검색/선택합니다. https://github.com/microsoft/vcpkg/issues/22287 참조.
- 최신 버전의 vcpkg ([vcpkg 홈페이지](https://github.com/Microsoft/vcpkg)의 지침 참조)
- Bright Nights에 변경 사항을 기여할 계획이라면 코드 포매터도 설치해야 합니다. 자세한 내용은 [코드 스타일](#code-style) 섹션 참조.

**참고:** Windows XP는 지원되지 않습니다!

## 설치 및 구성:

1. `Visual Studio`를 설치합니다 ([Visual Studio 홈페이지](https://visualstudio.microsoft.com/)에서 다운로드 가능).

- "Desktop development with C++"와 "Game development with C++" 워크로드를 선택합니다.

2. `Git for Windows`를 설치합니다 ([Git 홈페이지](https://git-scm.com/)에서 다운로드 가능).

3. 최신 `vcpkg`를 설치하고 구성합니다:

_**경고: 이 저장소를 클론할 위치에 공백이 포함되지 않는 것이 중요합니다. 즉, `C:/dev/vcpkg`는 허용되지만 `C:/dev test/vcpkg`는 허용되지 않습니다.**_

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat -disableMetrics
vcpkg integrate install
```

## 클론 및 컴파일:

1. 다음 명령줄로 Cataclysm-BN 저장소를 클론합니다:

**참고:** 전체 CBN 저장소, 3GB 이상의 데이터를 다운로드합니다. 테스트만 하는 경우 `--depth=1`을 추가하세요.

```cmd
git clone https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

2. 제공된 솔루션 (`msvc-full-features\Cataclysm-vcpkg-static.sln`)을 `Visual Studio`에서 열고 구성 (단순히 컴파일하려면 `Release`, 코드 편집을 계획하는 경우 `Debug`)과 플랫폼 (`x64` 또는 `x86`)을 선택하여 빌드합니다. 필요한 모든 의존성은 vcpkg에 의해 자동으로 빌드되고 캐시됩니다.

3. `Build > Configuration Manager` 메뉴를 열고 의도한 대상과 일치하도록 `Active solution configuration`과 `Active solution platform`을 조정합니다.

이렇게 하면 Sound, Tiles, 현지화 지원이 포함된 릴리스 버전을 컴파일하도록 Visual Studio가 구성됩니다 (단, 언어 파일 자체는 자동으로 컴파일되지 않으며 나중에 수행됩니다).

4. `Build > Build Solution` 또는 `Build > Build > Cataclysm-vcpkg-static`을 선택하여 빌드 프로세스를 시작합니다. 프로세스가 오래 걸릴 수 있으므로 커피와 책을 준비하세요 :) 각 아키텍처의 첫 번째 빌드는 vcpkg를 통해 의존성을 다운로드하고 설치하므로 특히 오래 걸릴 수 있습니다.

5. Visual Studio에서 직접 게임을 실행하려면 아래 설명된 대로 올바른 작업 디렉토리를 지정했는지 확인하세요. 그렇지 않으면 파일 탐색기에서만 게임을 실행할 수 있습니다.

6. 현지화 지원이 필요한 경우 UNIX 유사 시스템처럼 Git Bash GUI에서 bash 스크립트 `lang/compile_mo.sh`를 실행합니다. 위 2단계에서 자동으로 컴파일되지 않은 언어 파일이 컴파일됩니다.

### Visual Studio에서 실행 및 디버깅

1. Cataclysm 게임 바이너리 프로젝트 (`Cataclysm-vcpkg-static`)가 선택된 시작 프로젝트인지 확인합니다 (솔루션 탐색기에서 마우스 오른쪽 버튼 클릭 -> `Set as Startup Project`)

2. 프로젝트 속성에서 작업 디렉토리를 `$(ProjectDir)..`로 구성합니다 (솔루션 탐색기에서 마우스 오른쪽 버튼 클릭 -> `Properties` -> 상단에서 `All Configurations`와 `All Platforms` 선택 -> `Debugging` -> `Working Directory`)

3. 디버그 버튼 (상단의 녹색 오른쪽 삼각형, 또는 F5와 같은 적절한 단축키 사용)을 누릅니다.

Visual Studio에서 디버그 버튼을 누른 후 Cataclysm이 시작 후 반환 코드 1로 종료되면 작업 디렉토리가 잘못되었기 때문입니다.

### Debug vs Release 빌드

`Debug` 빌드는 `Release` 빌드보다 훨씬 느리게 실행되지만 추가 안전성 검사를 제공합니다.

실행 파일을 빌드하고 게임을 플레이하려면 `Release`가 권장됩니다.

코드를 편집할 계획이라면 `Debug`가 권장됩니다.

C++에 대한 충분한 경험이 있어 다음을 알고 있다면:

- `Debug`와 `Release`의 내부 차이점
- `Release` 최적화가 디버거에 미치는 영향
- 코드에서 정의되지 않은 동작을 피하는 방법

개발 프로세스 속도를 높이기 위해 항상 `Release` 빌드를 사용하고 파일 상단에 다음을 추가하여 파일별로 최적화를 비활성화할 수 있습니다:

```cpp
#pragma optimize("", off)
```

### 단위 테스트 실행

Cataclysm 테스트 바이너리 프로젝트 (`Cataclysm-test-vcpkg-static`)가 선택된 시작 프로젝트인지 확인하고 프로젝트 속성에서 작업 디렉토리를 `$(ProjectDir)..`로 구성한 다음 디버그 버튼을 누릅니다 (또는 F5와 같은 적절한 단축키 사용). 모든 단위 테스트가 실행됩니다. 추가 명령줄 인수는 프로젝트의 명령줄 인수 설정에서 구성할 수 있으며, 호환되는 단위 테스트 러너 (예: Resharper)를 사용하는 경우 단위 테스트 세션에서 개별 테스트를 실행하거나 디버그할 수 있습니다.

### 코드 스타일

C++ 코드의 스타일을 일관되게 유지하기 위해 `Artistic Style` 소스 코드 포매터를 사용합니다. 미리 빌드된 Windows 실행 파일로 제공되며 설치하여 실행하거나 커밋 전에 자동으로 코드를 포맷하도록 구성할 수 있지만, Visual Studio 사용자에게 훨씬 더 편리한 옵션은 특정 확장을 설치하는 것입니다. 자세한 내용은 [tooling의 "Visual Studio용 Astyle 확장"](../../reference/tooling.md#astyle-extensions-for-visual-studio)을 참조하세요.

2022년 10월부터 코드 스타일 검사는 GitHub의 각 PR에서 자동으로 실행되므로 변경 사항을 스타일링하는 것을 잊었다면 해당 검사가 실패하는 것을 볼 수 있습니다.

### 배포 만들기

`msvc-full-features` 폴더에 배치 스크립트 `distribute.bat`이 있습니다. 하위 폴더 `distribution`을 만들고 필요한 모든 파일 (예: `data/`, `Cataclysm.exe` 및 dll)을 해당 폴더에 복사합니다. 그런 다음 압축하여 인터넷에서 공유할 수 있습니다.
