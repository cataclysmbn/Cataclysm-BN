---
title: CMake
---

## 전제 조건

CataclysmBN을 빌드하려면 다음 라이브러리와 개발 헤더가 설치되어 있어야 합니다:

- 일반
  - `cmake` >= 3.2.4
  - `clang` >= 22
  - `gcc-libs` 또는 이에 준하는 C++ 런타임 라이브러리
  - `glibc`
  - `zlib`
  - `bzip2`
  - `sqlite3`
  - `SDL` >= 3.0.0 *
  - `SDL_shadercross` >= 3.0.0 *
- Curses
  - `ncurses`
- 타일
  - `SDL_image` >= 3.0.0 (PNG 및 JPEG 지원 포함) *
  - `SDL_mixer` >= 3.0.0 (Ogg Vorbis 지원 포함) *
  - `SDL_ttf` >= 3.0.0 *
  - `freetype`
- 사운드
  - `vorbis`
  - `libbz2`
  - `libz`

> [!NOTE]
>
> `*` 표시가 붙은 의존성은 없으면 자동으로 빌드됩니다.

현지화 파일을 컴파일하려면 `gettext` 패키지도 필요합니다.

## 빌드 환경

[git](https://github.com/cataclysmbn/Cataclysm-BN)에서 최신 버전의 소스 코드 tarball을 받을 수 있습니다.

```sh
git clone --filter=blob:none https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

> [!TIP]
> `filter=blob:none`은 [blobless clone](https://github.blog/open-source/git/get-up-to-speed-with-partial-clone-and-shallow-clone/)을 생성하여 파일을 필요할 때 다운로드하므로 초기 클론이 훨씬 빨라집니다.

### UNIX 환경

시스템 패키지 관리자로 위에 지정된 패키지를 설치하세요.

- Ubuntu 기반 배포판 (26.04 이상):

```sh
sudo apt install git cmake ninja-build mold clang-22 llvm-22 ccache \
libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- Fedora 기반 배포판:

```sh
sudo dnf install git cmake ninja-build mold clang llvm ccache \
SDL3-devel SDL3_image-devel SDL3_ttf-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

> [!NOTE]
> Ubuntu와 Fedora는 SDL3_mixer 또는 shadercross를 제공하지 않습니다.
> 이 라이브러리들은 컴파일할 때 자동으로 빌드됩니다.

#### 컴파일러 버전 확인

CataclysmBN을 빌드하려면 Clang 22 이상이 필요합니다. 다음 명령으로 컴파일러 버전을 확인할 수 있습니다:

```sh
$ clang++ --version
clang version 22.1.6 (Fedora 22.1.6-1.fc44)
Target: x86_64-redhat-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
```

> [!TIP]
>
> **`clang-{version}`을 설치했지만 `clang`을 찾을 수 없을 때**
>
> `update-alternatives`를 사용하여 기본 Clang 버전을 설정합니다:
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100
> ```
>
> Ubuntu가 `llvm-ar-22`, `llvm-ranlib-22`처럼 버전이 붙은 LLVM binutils 이름만 설치하는 경우에는 이 이름도 등록하세요:
>
> ```sh
> sudo update-alternatives --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-22 100
> sudo update-alternatives --install /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-22 100
> ```

### macOS 환경

[Homebrew](https://brew.sh/)로 의존성을 설치하세요:

```sh
brew install cmake ninja ccache sdl3 sdl3_image sdl3_ttf sdl3_mixer \
  freetype gettext sqlite pkg-config ncurses flac
```

> [!NOTE]
> Xcode 16 이상에 포함된 Apple Clang은 CataclysmBN에 필요한 C++23 기능을 지원합니다.
> 별도의 컴파일러를 설치할 필요가 없습니다.

### Windows Subsystem for Linux (WSL)

`UNIX 환경`과 동일한 지침을 따르면 됩니다. 그대로 작동합니다 (TM).

`tiles`를 사용할 계획이라면 [GUI를 지원하는 최신 WSL 2](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)와 [일치하는 드라이버](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#prerequisites)를 설치했는지 확인하세요.

### Windows 환경 (MSYS2)

1. https://msys2.github.io/의 단계를 따릅니다.
2. CataclysmBN 빌드 의존성을 설치합니다:

```sh
pacman -S mingw-w64-x86_64-toolchain msys/git \
   	  mingw-w64-x86_64-cmake \
   	  mingw-w64-x86_64-SDL2_{image,mixer,ttf} \
   	  ncurses-devel \
      gettext \
      base-devel
```

이렇게 하면 Windows 콘솔판과 타일판을 빌드할 환경이 준비됩니다.

> [!NOTE]
>
> Jetbrains CLion으로 테스트하려는 경우 내장 CMake 대신 `msys32/mingw32` 경로의 CMake 버전을 지정하세요. 그러면 CMake가 설치된 패키지를 감지할 수 있습니다.

### CMake 빌드

CMake에는 별도의 구성 단계와 빌드 단계가 있습니다. 구성은 CMake 자체로 수행하고, 실제 빌드는 빌드 시스템에 구애받지 않는 `cmake --build .`로 수행합니다.

CMake로 CataclysmBN을 빌드하는 방법은 소스 트리 안에서 빌드하는 방법과 밖에서 빌드하는 방법, 두 가지가 있습니다. 소스 외부 빌드는 하나의 소스 디렉터리에서 서로 다른 옵션의 여러 빌드를 만들 수 있다는 장점이 있습니다.

> [!CAUTION]
>
> 소스 트리 내부 빌드는 **지원되지 않습니다**.

#### 프리셋으로 빌드 (권장)

미리 정의된 [빌드 프리셋](#빌드-프리셋)이 여러 개 있어 빌드 과정을 두 명령으로 줄일 수 있습니다.

Linux:

```sh
cmake --preset linux-slim
cmake --build --preset linux-slim --target cataclysm-bn-tiles
```

macOS:

```sh
cmake --preset osx-arm-slim
cmake --build --preset osx-arm-slim
```

실행 파일은 각각 `out/build/linux-slim/` 또는 `out/build/osx-arm-slim/`에 생성됩니다.

> [!TIP]
> [clang-tidy 플러그인](../../reference/tooling.md#clang-tidy)과 Tracy 프로파일러 지원을 포함해 빌드하려면 `linux-full`을 사용해 보세요.

> [!NOTE]
> 여러 대상을 한 번에 빌드할 수 있습니다:
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> 또는 `--parallel` 옵션으로 최대 스레드 수를 제한할 수 있습니다:
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### 프리셋 없이 빌드

CataclysmBN을 소스 외부에서 빌드하려면:

```sh
mkdir -p out/build/custom
cmake -B out/build/custom -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/custom
```

위 예시는 `out/build/` 아래에 빌드 디렉터리를 만들지만, 필수는 아닙니다. 완전히 다른 위치에 만들어도 됩니다.

빌드 후 CataclysmBN을 설치하려면 필요한 경우 `su` 또는 `sudo`로 root 권한을 사용하여 실행하세요:

```sh
cmake --install out/build/custom
```

### 배포 패키지 생성

> [!TIP]
> 배포 프리셋 목록은 [빌드 프리셋](#빌드-프리셋)을 확인하세요.

빌드 프리셋으로 포터블 배포 패키지를 만드세요:

```sh
# 타일 배포 구성
cmake --preset dist-tiles

# 게임 및 도구 빌드
cmake --build --preset dist-tiles

# 배포 패키지 생성
cmake --install build --prefix cataclysmbn-linux-tiles
```

다음 구조의 자체 포함 디렉터리가 만들어집니다:

```
cataclysmbn-linux-tiles/
├── cataclysm-bn-tiles     # 게임 실행 파일
├── cataclysm-launcher     # 런처 스크립트
├── json_formatter         # JSON 포매팅 도구
├── data/                  # 게임 데이터 파일
├── gfx/                   # 타일셋
├── lang/                  # 번역
├── doc/                   # 문서
├── README.md
├── LICENSE.txt
└── VERSION.txt
```

배포용 tarball을 만들려면:

```sh
tar -czvf cataclysmbn-linux-tiles.tar.gz cataclysmbn-linux-tiles
```

> [!TIP]
> `cataclysm-launcher` 스크립트는 올바른 작업 디렉터리와 라이브러리 경로를 설정합니다.
> 어느 위치에서든 게임을 실행할 때 사용하세요.

#### 포터블 vs 시스템 설치

| 옵션        | `USE_PREFIX_DATA_DIR=OFF` | `USE_PREFIX_DATA_DIR=ON`   |
| ----------- | ------------------------- | -------------------------- |
| 데이터 위치 | `./data/`                 | `/usr/share/cataclysm-bn/` |
| 설정 위치   | `./config/`               | `~/.config/cataclysm-bn/`  |
| 적합 용도   | 포터블/릴리스 빌드        | 시스템 패키지 (deb/rpm)    |

> [!TIP]
> 빌드를 구성하는 방법은 [빌드 옵션](#빌드-옵션)을 확인하세요.

## Visual Studio / MSBuild용 빌드

> [!CAUTION]
>
> 이 가이드는 꽤 오래되었으며 수동 의존성 관리가 필요합니다.
>
> 최신 대안은 [vcpkg와 함께 CMake Visual Studio 빌드](./vs_cmake.md)를 참조하세요.

CMake는 Visual Studio 자체 또는 MSBuild 명령줄 컴파일러(전체 IDE를 원하지 않는 경우)에서 사용하는 `.sln` 및 `.vcxproj` 파일을 생성할 수 있으며, MSYS/Cygwin이 제공하는 것보다 더 “네이티브”인 바이너리를 얻을 수 있습니다.

현재는 제한된 옵션 조합만 지원됩니다(타일만, 현지화 없음, 백트레이스 없음).

도구를 받으세요:

- 공식 사이트의 CMake - <https://cmake.org/download/>.
- Microsoft 컴파일러 - <https://visualstudio.microsoft.com/downloads/?q=build+tools>, “Build Tools for Visual Studio 2017”을 선택합니다. 설치할 때 “Visual C++ Build Tools” 옵션을 선택하세요.
  - 또는 전체 Visual Studio를 다운로드해 설치할 수도 있지만 필수는 아닙니다.

필요한 라이브러리를 받으세요:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3) (“Visual C++ 32/64-bit” 버전이 필요합니다. 아래도 동일합니다)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (선택 사항, 사운드 지원용)
- 지원되지 않는(아래 지침에서 사용하지 않는) 선택적 라이브러리:
  - `ncurses` - ???

라이브러리 아카이브의 압축을 풉니다.

Windows 명령줄(또는 PowerShell)을 열고 위 라이브러리를 가리키도록 환경 변수를 설정합니다. 경로는 환경에 맞게 조정하세요:

```sh
set SDL2DIR=C:\path\to\SDL2-devel-2.0.9-VC
set SDL2TTFDIR=C:\path\to\SDL2_ttf-devel-2.0.15-VC
set SDL2IMAGEDIR=C:\path\to\SDL2_image-devel-2.0.4-VC
set SDL2MIXERDIR=C:\path\to\SDL2_mixer-devel-2.0.4-VC
```

PowerShell 구문은 `$env:SDL2DIR="C:\path\to\SDL2-devel-2.0.9-VC"`입니다.

CMake 구성 단계를 실행합니다:

```sh
cd <path to cbn sources>
cmake -B out/build/msbuild -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

빌드하세요!

```
cmake --build out/build/msbuild --config Release --parallel 2
```

`--parallel 2` 플래그는 빌드 병렬 처리를 제어합니다. 원하면 생략할 수 있습니다. `--config Release` 플래그는 최적화된 Visual Studio 구성을 선택합니다. 생략하면 대신 `Debug` 구성이 빌드됩니다.

결과 파일은 소스 Cataclysm-BN 폴더 안의 `Release` 디렉터리에 놓입니다. 실행하려면 먼저 바이너리가 게임 데이터에 접근할 수 있도록 소스 Cataclysm-BN 디렉터리 자체로 옮기고, 필요한 `.dll` 파일을 같은 폴더에 넣어야 합니다. 이 파일들은 개발 라이브러리 디렉터리의 `lib/x86/` 또는 `lib/x64/` 아래에서 찾을 수 있으며, 64비트 머신에서도 `x86` 파일이 필요할 가능성이 높습니다.

DLL 복사는 한 번만 하면 되지만, 바이너리는 빌드할 때마다 `Release/` 밖으로 옮겨야 합니다. 이를 조금 자동화하려면 `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=` 옵션 및 `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`에 해당하는 옵션으로 원하는 바이너리 출력 디렉터리를 설정할 수 있습니다.

게임을 실행하세요. 작동할 것입니다.

## 빌드 옵션

빌드 옵션을 변경하려면 명령줄에서 옵션을 전달할 수 있습니다:

```sh
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
```

또는 `ccmake`나 `cmake-gui` 프론트엔드를 사용하세요. 이들은 모든 옵션과 캐시된 값을 각각 콘솔 및 그래픽 UI에 표시합니다.

```sh
cmake --preset linux-slim
ccmake out/build/linux-slim
```

```sh
cmake --preset linux-slim
cmake-gui -S . -B out/build/linux-slim
```

### CMake 특정 옵션

- CMAKE_BUILD_TYPE=`<build type>`

컴파일할 때 특정 빌드 구성을 선택합니다. `release`는 일반 사용을 위한 기본 최적화(-Os) 빌드를 생성합니다. `debug`는 버그 보고 시 자세한 백트레이스를 얻는 데 자주 필요한, 전체 디버그 심볼이 포함된 느리고 큰 비최적화(-O0) 빌드를 생성합니다.

**참고**: 기본적으로 CMake는 명령줄에서 다른 구성 옵션이 전달되지 않으면 `debug` 빌드를 생성합니다.

- CMAKE_INSTALL_PREFIX=`<full path>`

바이너리, 리소스, 문서 파일의 설치 접두사입니다.

### CataclysmBN 특정 옵션

| 옵션                  | 기본값                                     | 효과                                                                                             |
| --------------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------ |
| `CURSES`              | `ON`                                       | curses 버전을 빌드합니다.                                                                        |
| `TILES`               | `OFF`                                      | 그래픽 타일셋 버전을 빌드합니다.                                                                 |
| `SOUND`               | `ON`                                       | 오디오 지원을 빌드합니다.                                                                        |
| `LANGUAGES`           | `" "`                                      | 지정된 언어 지원을 빌드합니다.                                                                   |
| `TESTS`               | `ON`                                       | 테스트 유닛을 빌드합니다.                                                                        |
| `USE_HOME_DIR`        | `ON` (WIN32에서는 `OFF`)                   | 저장 및 설정 파일에 `$HOME` 디렉터리를 사용합니다.                                               |
| `USE_XDG_DIR`         | `OFF`                                      | 저장 및 설정 파일에 XDG 디렉터리를 사용합니다.                                                   |
| `USE_PREFIX_DATA_DIR` | `OFF`                                      | 릴리스 빌드에서 게임 데이터에 UNIX 시스템 디렉터리를 사용합니다.                                 |
| `JSON_FORMAT`         | `OFF`                                      | JSON 포매터를 빌드합니다.                                                                        |
| `CATA_CCACHE`         | `ON`                                       | ccache를 찾아 사용해 빌드하려고 시도합니다.                                                      |
| `BUILD_SDL3`          | `OFF`                                      | 시스템 라이브러리 대신 SDL3을 강제로 빌드합니다.                                                 |
| `BUILD_SHADERCROSS`   | `ON`                                       | shadercross가 `PATH`에 없으면 소스에서 SDL_shadercross를 빌드합니다.                             |
| `SHADER_TARGETS`      | `spirv;msl` (WIN32에서는 `dxil;spirv;msl`) | 빌드할 셰이더입니다.                                                                             |
| `DYNAMIC_LINKING`     | `ON`                                       | 동적 링크를 사용합니다. 또는 정적 링크로 MinGW 의존성을 제거합니다.                              |
| `LINKER`              | `" "`                                      | 사용할 사용자 지정 링커입니다.                                                                   |
| `BACKTRACE`           | `ON`                                       | 크래시 시 스택 백트레이스 출력을 지원합니다.                                                     |
| `LIBBACKTRACE`        | `OFF`                                      | libbacktrace로 백트레이스를 출력합니다.                                                          |
| `USE_TRACY`           | `OFF`                                      | Tracy 프로파일러를 사용합니다. 자세한 내용은 [Tracy로 프로파일링하기](../tracy.md)를 참조하세요. |
| `GIT_BINARY`          | `" "`                                      | Git 바이너리 이름 또는 경로입니다.                                                               |

### 빌드 프리셋

자세한 내용은 [CMake 문서](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)를 확인하세요.

| 프리셋 이름                | 설명                                                |
| -------------------------- | --------------------------------------------------- |
| `linux-curses`             | Linux Slim Build (Curses)                           |
| `linux-slim`               | Linux Slim Build (Tiles, Sounds)                    |
| `linux-full`               | Linux Full Build (Tiles, Sounds, Clang-Tidy, Tracy) |
| `linux-cross-aarch64`      |                                                     |
| `linux-slim-cross-aarch64` |                                                     |
| `linux-full-cross-aarch64` |                                                     |
| `dist-tiles`               | Linux Distribution (Tiles, Sounds, Languages)       |
| `dist-curses`              | Linux Distribution (Curses, Languages)              |
| `osx-curses-x64-dist`      | macOS x64 Distribution (Curses, Languages)          |
| `osx-tiles-x64-dist`       | macOS x64 Distribution (Tiles, Sounds, Languages)   |
| `osx-arm-slim`             | macOS ARM Slim Build (Tiles, Sounds)                |
| `osx-curses-arm-dist`      | macOS ARM Distribution (Curses, Languages)          |
| `osx-tiles-arm-dist`       | macOS ARM Distribution (Tiles, Sounds, Languages)   |
| `osx-arm-dist`             | macOS ARM Distribution (Tiles, Sounds, Languages)   |
| `lint`                     | Lint and Format Only                                |
| `ci-curses`                | CI Build (Curses)                                   |
| `ci-tiles`                 | CI Build (Tiles, Sound)                             |

> [!NOTE]
>
> macOS 배포 프리셋은 `dmgdist` 대상으로 DMG를 빌드합니다. `dmgbuild`가 `PATH`에 있어야 합니다.
> dmgdist와 biplist를 아직 설치하지 않았다면 pip로 설치할 수 있습니다.
>
> ```sh
> python3 -m pip install dmgbuild biplist
> ```

### 로컬에서 번역 포함 빌드하기

2026년부터 번역 파일(`.po` 파일)은 더 이상 저장소에 포함되지 않습니다. CI가 Transifex에서 이를 가져와 워크플로 아티팩트로 저장하고 릴리스 패키지 빌드에 사용합니다.

로컬 빌드에서는 다음 방법 중 하나를 사용하세요:

#### 옵션 1: 번역 없이 빌드하기 (가장 빠름)

번역 작업을 하지 않는다면 비활성화하세요:

```sh
cmake --preset linux-full -DLANGUAGES=none
cmake --build --preset linux-full
```

#### 옵션 2: Transifex에서 번역 가져오기

로컬에서 번역을 테스트해야 하고 Transifex 접근 권한이 있다면:

1. Transifex CLI를 설치합니다:

```sh
curl -sL https://github.com/transifex/cli/releases/download/v1.6.17/tx-linux-amd64.tar.gz | sudo tar zxvf - -C /usr/bin tx
```

2. 번역 파일을 가져옵니다:

```sh
tx pull --force --all
```

3. 번역을 활성화한 상태로 빌드합니다:

```sh
cmake --preset linux-full -DLANGUAGES=all
cmake --build --preset linux-full
```

#### 옵션 3: `translations` 워크플로 아티팩트 다운로드

Transifex 접근 권한이 없다면 번역 워크플로에서 생성한 아티팩트를 사용하세요:

1. [Actions](https://github.com/cataclysmbn/Cataclysm-BN/actions)에서 최근 성공한 워크플로 실행을 엽니다
2. `translations` 아티팩트를 다운로드합니다
3. `lang/po`와 `src/lang_stats.inc`를 로컬 체크아웃에 압축 해제합니다
4. `-DLANGUAGES=all`로 평소처럼 빌드합니다

> [!NOTE]
> 대부분의 코드 변경은 번역이 필요하지 않습니다. 현지화된 출력을 테스트하는 경우가 아니라면 `-DLANGUAGES=none`을 사용하세요.

> [!NOTE]
> 릴리스 아카이브에는 패키지 빌드에 쓰이는 컴파일된 `lang/mo` 파일만 포함됩니다. 로컬에서 번역을 다시 빌드하는 데 필요한 `lang/po` 소스는 포함되지 않습니다.
