---
title: CMake
---

## 전제 조건

CataclysmBN을 빌드하려면 다음 라이브러리와 개발 헤더가 설치되어 있어야 합니다:

- General
  - `cmake` >= 3.0.0
  - `gcc` >= 14
  - `clang` >= 19
  - `gcc-libs`
  - `glibc`
  - `zlib`
  - `bzip2`
  - `sqlite3`
- Curses
  - `ncurses`
- Tiles
  - `SDL` >= 2.0.0
  - `SDL_image` >= 2.0.0 (PNG 및 JPEG 지원 포함)
  - `SDL_mixer` >= 2.0.0 (Ogg Vorbis 지원 포함)
  - `SDL_ttf` >= 2.0.0
  - `freetype`
- Sound
  - `vorbis`
  - `libbz2`
  - `libz`

현지화 파일을 컴파일하려면 `gettext` 패키지도 필요합니다.

## 빌드 환경

[git](https://github.com/cataclysmbn/Cataclysm-BN)에서 최신 버전의 소스 코드 tarball을 얻을 수 있습니다.

```sh
git clone --filter=blob:none https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

> [!TIP]
> `filter=blob:none`은 [blobless clone](https://github.blog/open-source/git/get-up-to-speed-with-partial-clone-and-shallow-clone/)을 생성하여 파일을 필요에 따라 다운로드함으로써 초기 클론을 훨씬 빠르게 만듭니다.

### UNIX 환경

시스템 패키지 관리자로 위에 지정된 패키지를 얻습니다.

- Ubuntu 기반 배포판 (24.04 이상):

```sh
sudo apt install git cmake ninja-build mold g++-14 clang-20 ccache \
libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- Fedora 기반 배포판:

```sh
sudo dnf install git cmake ninja-build mold clang ccache \
SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

#### 컴파일러 버전 확인

CataclysmBN을 빌드하려면 최소한 `gcc` 14 **및** `clang` 19가 있어야 합니다. 컴파일러 버전을 확인할 수 있습니다:

```sh
$ g++ --version
g++ (GCC) 15.2.1 20250808 (Red Hat 15.2.1-1)
Copyright (C) 2025 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ clang++ --version
clang version 20.1.8 (Fedora 20.1.8-4.fc42)
Target: x86_64-redhat-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
Configuration file: /etc/clang/x86_64-redhat-linux-gnu-clang++.cfg
```

> [!TIP]
>
> **`gcc-{version}` 설치했지만 `gcc`를 찾을 수 없을 때**
>
> `update-alternatives`를 사용하여 기본 gcc 버전을 설정합니다:
>
> ```sh
> sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
> sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
> sudo update-alternatives --display gcc
> gcc - auto mode
>   link best version is /usr/bin/gcc-14
>   link currently points to /usr/bin/gcc-14
>   link gcc is /usr/bin/gcc
> /usr/bin/gcc-14 - priority 100
> sudo update-alternatives --display g++
> g++ - auto mode
>   link best version is /usr/bin/g++-14
>   link currently points to /usr/bin/g++-14
>   link g++ is /usr/bin/g++
> /usr/bin/g++-14 - priority 100
> ```
>
> `clang`에도 동일하게 적용됩니다.
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
> ```

### Windows Subsystem for Linux (WSL)

`UNIX environment`와 동일한 지침을 따릅니다. 그냥 작동합니다 (TM)

`tiles`를 사용할 계획이라면 [GUI를 지원하는 최신 WSL 2](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)와 [일치하는 드라이버를 설치](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#prerequisites)했는지 확인하세요.

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

Windows의 콘솔 및 타일 버전을 빌드하기 위한 환경이 설정되어야 합니다.

> [!NOTE]
>
> Jetbrains CLion으로 테스트하려는 경우 내장 cmake 대신 `msys32/mingw32` 경로의 cmake 버전을 가리킵니다. 이렇게 하면 cmake가 설치된 패키지를 감지할 수 있습니다.

### CMake 빌드

CMake에는 별도의 구성 및 빌드 단계가 있습니다. 구성은 CMake 자체를 사용하여 수행되며 실제 빌드는 `make` (Makefiles 생성기용) 또는 빌드 시스템에 구애받지 않는 `cmake --build .`를 사용하여 수행됩니다.

소스 트리 내부 또는 외부에서 CataclysmBN을 빌드하는 두 가지 방법이 있습니다. 소스 외부 빌드는 하나의 소스 디렉토리에서 다른 옵션을 가진 여러 빌드를 가질 수 있다는 장점이 있습니다.

> [!CAUTION]
>
> 소스 트리 내부 빌드는 **지원되지 않습니다**.

#### 프리셋으로 빌드 (권장)

여러 사전 정의된 [빌드 프리셋](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)을 사용할 수 있어 빌드 프로세스가 두 가지 명령으로 간단해집니다:

```sh
cmake --preset linux-slim
cmake --build build --preset linux-slim --target cataclysm-bn-tiles
```

실행 파일이 `out/build/linux-slim/`에 생성됩니다.

> [!TIP]
> [clang-tidy 플러그인](../../reference/tooling.md#clang-tidy)과 tracy 프로파일러가 내장된 빌드를 하려면 `linux-full`을 시도해보세요.

> [!NOTE]
> 여러 대상을 한 번에 빌드할 수 있습니다:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> 또는 `--parallel` 옵션으로 최대 스레드 수를 제한합니다:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### 프리셋 없이 빌드

CataclysmBN을 소스 외부에서 빌드하려면:

```sh
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

위의 예제는 소스 디렉토리 내부에 빌드 디렉토리를 만들지만 필수는 아닙니다 - 완전히 다른 위치에 쉽게 만들 수 있습니다.

빌드 후 CataclysmBN을 설치하려면 (필요시 su 또는 sudo를 사용하여 root로):

```sh
cmake --install build
```

### 배포 패키지 생성

포터블 배포 패키지를 만들려면 `dist-tiles` 또는 `dist-curses` 프리셋을 사용합니다:

```sh
# 타일 배포 구성
cmake --preset dist-tiles

# 게임 및 도구 빌드
cmake --build --preset dist-tiles

# 배포 패키지 생성
cmake --install build --prefix cataclysmbn-linux-tiles
```

curses 전용 빌드:

```sh
cmake --preset dist-curses
cmake --build --preset dist-curses
cmake --install build --prefix cataclysmbn-linux-curses
```

다음 구조로 자체 포함 디렉토리를 만듭니다:

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
> `cataclysm-launcher` 스크립트는 올바른 작업 디렉토리와 라이브러리 경로를 설정합니다.
> 어느 위치에서든 게임을 실행하는 데 사용하세요.

#### 배포 프리셋

| 프리셋        | 설명                           |
| ------------- | ------------------------------ |
| `dist-tiles`  | Tiles + Sound + Languages      |
| `dist-curses` | Curses + Languages             |
| `lint`        | 포매팅 도구만을 위한 최소 빌드 |

#### 포터블 vs 시스템 설치

| 옵션        | `USE_PREFIX_DATA_DIR=OFF` | `USE_PREFIX_DATA_DIR=ON`   |
| ----------- | ------------------------- | -------------------------- |
| 데이터 위치 | `./data/`                 | `/usr/share/cataclysm-bn/` |
| 설정 위치   | `./config/`               | `~/.config/cataclysm-bn/`  |
| 적합 용도   | 포터블/릴리스 빌드        | 시스템 패키지 (deb/rpm)    |

빌드 옵션을 변경하려면 명령줄에서 옵션을 전달할 수 있습니다:

```sh
cmake .. -DOPTION_NAME=option_value
```

또는 콘솔 및 그래픽 UI에서 모든 옵션과 캐시된 값을 각각 표시하는 `ccmake` 또는 `cmake-gui` 프론트엔드를 사용합니다.

```sh
ccmake ..
cmake-gui ..
```

## Visual Studio / MSBuild용 빌드

> [!CAUTION]
>
> 이 가이드는 꽤 오래되었으며 수동 의존성 관리가 필요합니다.
>
> 최신 대안은 [vcpkg와 함께 CMake Visual Studio 빌드](./vs_cmake.md)를 참조하세요.

CMake는 Visual Studio 자체 또는 MSBuild 명령줄 컴파일러 (전체 IDE를 원하지 않는 경우)에서 사용하는 `.sln` 및 `.vcxproj` 파일을 생성할 수 있으며 MSYS/Cygwin이 제공할 수 있는 것보다 더 "네이티브" 바이너리를 가질 수 있습니다.

현재 제한된 옵션 조합만 지원됩니다 (타일만, 현지화 없음, 백트레이스 없음).

도구 얻기:

- 공식 사이트의 CMake - <https://cmake.org/download/>.
- Microsoft 컴파일러 - <https://visualstudio.microsoft.com/downloads/?q=build+tools>, "Build Tools for Visual Studio 2017" 선택. 설치 시 "Visual C++ Build Tools" 옵션을 선택합니다.
  - 또는 전체 Visual Studio를 다운로드하고 설치할 수 있지만 필수는 아닙니다.

필요한 라이브러리 얻기:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3) ("Visual C++ 32/64-bit" 버전 필요. 아래도 동일)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (선택 사항, 사운드 지원용)
- 지원되지 않는 (다음 지침에서 사용되지 않는) 선택적 라이브러리:
  - `ncurses` - ???

라이브러리와 함께 아카이브의 압축을 풉니다.

Windows 명령줄 (또는 powershell)을 열고 환경 변수를 다음과 같이 설정하여 위의 라이브러리를 가리킵니다 (경로를 적절히 조정):

```sh
set SDL2DIR=C:\path\to\SDL2-devel-2.0.9-VC
set SDL2TTFDIR=C:\path\to\SDL2_ttf-devel-2.0.15-VC
set SDL2IMAGEDIR=C:\path\to\SDL2_image-devel-2.0.4-VC
set SDL2MIXERDIR=C:\path\to\SDL2_mixer-devel-2.0.4-VC
```

(powershell의 경우 구문은 `$env:SDL2DIR="C:\path\to\SDL2-devel-2.0.9-VC"`).

빌드 디렉토리를 만들고 cmake 구성 단계를 실행합니다:

```sh
cd <path to cbn sources>
mkdir build
cmake -B build -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

빌드!

```
cmake --build build -j 2 -- /p:Configuration=Release
```

`-j 2` 플래그는 빌드 병렬 처리를 제어합니다 - 원하면 생략할 수 있습니다. `/p:Configuration=Release` 플래그는 MSBuild에 직접 전달되어 최적화를 제어합니다. 생략하면 `Debug` 구성이 대신 빌드됩니다. powershell의 경우 첫 번째 `--` 뒤에 추가 `--`가 필요합니다.

결과 파일은 소스 Cataclysm-BN 폴더 내부의 `Release` 디렉토리에 저장됩니다. 실행하려면 먼저 소스 Cataclysm-BN 디렉토리 자체로 이동해야 합니다 (바이너리가 게임 데이터에 접근할 수 있도록). 두 번째로 필요한 `.dll`을 동일한 폴더에 넣어야 합니다 - 개발 라이브러리용 디렉토리 내부의 `lib/x86/` 또는 `lib/x64/` 아래에서 찾을 수 있습니다 (64비트 머신이어도 `x86` 것이 필요할 가능성이 높음).

dll 복사는 일회성 작업이지만 빌드될 때마다 바이너리를 `Release/` 밖으로 이동해야 합니다. 이를 자동화하려면 `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=` 옵션 (및 `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`와 유사)으로 cmake를 구성하고 원하는 바이너리 대상 디렉토리를 설정할 수 있습니다.

게임을 실행합니다. 작동해야 합니다.

## 빌드 옵션

CMake가 지원하는 전체 옵션 목록은 `ccmake` 또는 `cmake-gui` 프론트엔드를 실행하거나 `cmake`를 실행하고 빌드 디렉토리에서 생성된 CMakeCache.txt를 텍스트 에디터에서 열 수 있습니다.

```
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
```

### CMake 특정 옵션

- CMAKE_BUILD_TYPE=`<build type>`

컴파일 시 특정 빌드 구성을 선택합니다. `release`는 일반 사용을 위한 기본 최적화된 (-Os) 빌드를 생성합니다. `debug`는 버그를 보고할 때 자세한 백트레이스를 얻는 데 종종 필요한 전체 디버그 심볼이 있는 느리고 큰 최적화되지 않은 (-O0) 빌드를 생성합니다.

**참고**: 기본적으로 CMake는 명령줄에서 다른 구성 옵션이 전달되지 않으면 `debug` 빌드를 생성합니다.

- CMAKE_INSTALL_PREFIX=`<full path>`

바이너리, 리소스 및 문서 파일의 설치 접두사입니다.

### CataclysmBN 특정 옵션

- CURSES=`<boolean>`

curses 버전을 빌드합니다.

- TILES=`<boolean>`

그래픽 타일셋 버전을 빌드합니다.

- SOUND=`<boolean>`

게임 내 사운드 및 음악 지원.

- USE_HOME_DIR=`<boolean>`

저장 파일에 사용자의 홈 디렉토리를 사용합니다.

- LANGUAGES=`<str>`

지정된 언어에 대한 현지화 파일을 컴파일합니다. 예:

```
-DLANGUAGES="cs;de;el;es_AR;es_ES"
```

언어 파일은 `RELEASE` 빌드 유형을 빌드할 때만 자동으로 컴파일됩니다. 다른 빌드 유형의 경우 `make` 명령에 `translations_compile` 대상을 추가해야 합니다 (예: `make all translations_compile`).

- DYNAMIC_LINKING=`<boolean>`

동적 링킹을 사용합니다. 또는 정적을 사용하여 MinGW 의존성을 제거합니다.

- CUSTOM LINKER=`<str>`

[gold], [lld] 또는 [mold]와 같은 커스텀 링커를 선택합니다.

- libbacktrace를 사용하지 않으면 ld를 선택하세요.
- libbacktrace를 사용하면 mold를 선택하세요. gold보다 24배 빠른 가장 빠른 링커입니다.

[gold]: https://en.wikipedia.org/wiki/Gold_(linker)
[lld]: https://lld.llvm.org
[mold]: https://github.com/rui314/mold

- BACKTRACE=`<boolean>`

크래시 시 콘솔에 백트레이스를 출력합니다. 디버그 빌드의 경우 기본적으로 `ON`입니다.

- LIBBACKTRACE=`<boolean>`

[libbacktrace]로 백트레이스를 출력합니다. lld와 mold가 백트레이스를 출력할 수 있게 하며 일반적으로 훨씬 빠릅니다.

[libbacktrace]: https://github.com/ianlancetaylor/libbacktrace

- USE_TRACY=`<boolean>`

tracy 프로파일러를 사용합니다. 자세한 내용은 [tracy로 프로파일링하기](../tracy.md)를 참조하세요.

- GIT_BINARY=`<str>`

기본 Git 바이너리 이름 또는 경로를 재정의합니다.

- USE_PREFIX_DATA_DIR=`<boolean>`

릴리스 빌드에서 게임 데이터에 UNIX 시스템 디렉토리를 사용합니다.

- USE_XDG_DIR=`<boolean>`

저장 및 설정 파일에 XDG 디렉토리를 사용합니다.

- TESTS=`<boolean>`

테스트를 빌드할지 여부입니다.

- JSON_FORMAT=`<boolean>`

JSON 파일 포매팅을 위한 `json_formatter` 도구를 빌드하고 `style-json` / `style-json-parallel` 대상을 활성화합니다. 사용법은 [포매팅 및 린팅](../formatting.md)을 참조하세요.

따라서 타일 및 사운드 지원이 있는 릴리스 모드로 Cataclysm-BN을 빌드하기 위한 CMake 명령은 다음과 같습니다 (프로젝트에 위치한 빌드 디렉토리에서 실행된다고 가정).

```sh
cmake ../ -DCMAKE_BUILD_TYPE=Release -DTILES=ON -DSOUND=ON
```
