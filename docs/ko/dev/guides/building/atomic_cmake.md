---
title: Atomic 배포판에서 CMake 빌드
---

이 문서는 immutable하여 기본 설치에 의존성을 레이어링하기보다 컨테이너화가 선호되는 배포판에서 BN을 빌드하는 방법을 다룹니다.

# 예제: Fedora Atomic 기반 (Bazzite)

> [!CAUTION]
>
> 이 글을 작성하는 시점에서 Bazzite의 기본 컨테이너 이미지는 fedora-toolbox:38이며, 이로 인해 의존성 설치 스크립트를 편집해야 할 _수 있습니다_. 더 최신 버전의 Fedora를 이미지로 가져오는 배포판이나 수동으로 가져오는 경우 표준 Fedora 스크립트를 그대로 사용할 수 있습니다.

> [!CAUTION]
>
> [distrobox](https://distrobox.it) 사용시 [exported](https://github.com/89luca89/distrobox/blob/main/docs/usage/distrobox-export.md) 컴파일러 (예: `~/.local/bin/clang`)는 [`/usr`에 접근할 수 없어](https://github.com/89luca89/distrobox/issues/1548) 작동하지 않습니다. 대신 `/usr/bin/clang`과 같은 절대 경로를 사용하세요.

## 컨테이너 설정

Bazzite는 Fedora Linux의 Atomic 버전을 기반으로 하며 컨테이너화 도구인 Toolbx가 사전 설치되어 있습니다. 따라서 이 예제에서는 이 방법을 사용합니다.

먼저 toolbox를 생성합니다:

```sh
$ toolbox create
```

"fedora-toolbox:38" (또는 다른 번호) 또는 Fedora Workstation을 다운로드할 것인지 묻는 메시지가 표시될 것입니다. 'y'를 입력하고 컨테이너가 빌드될 때까지 기다립니다. 하드웨어에 따라 시간이 걸릴 수 있지만 다행히 한 번만 수행하면 됩니다.

toolbox를 생성한 후 Cataclysm-BN 폴더 (**`src` 폴더가 있는**)로 이동합니다. (즉, github 저장소의 로컬 복사본이나 다운로드한 소스 코드) 거기서 간단한 명령으로 toolbox에 들어갈 수 있습니다:

```sh
$ toolbox enter
```

빌드할 때마다 이 명령을 실행하여 컨테이너에 들어가야 합니다.

toolbox에 들어가면 Fedora 기반 배포판 설치 스크립트로 의존성을 설치할 수 있습니다. Bazzite의 경우 스크립트는 다음과 같습니다:

```sh
$ sudo dnf install git cmake clang ninja-build mold ccache \
  SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
  freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
  sqlite-devel zlib-devel
```

이것으로 향후 모든 빌드 요구사항을 위한 컨테이너가 설정되었습니다! 이제 실제 빌드 단계로 넘어갑니다.

## 빌드

컨테이너에 아직 없다면 Cataclysm-BN 폴더로 이동하여 컨테이너에 들어갑니다. 거기서 cmake 스크립트를 실행하여 파일을 생성합니다 (빌드 폴더를 직접 만들 필요 없이 cmake가 없으면 자동으로 생성합니다). Bazzite의 경우:

```sh
cmake \
  -B build \
  -G Ninja \
  -DCATA_CCACHE=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INSTALL_PREFIX=$HOME/.local/share \
  -DJSON_FORMAT=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCURSES=OFF \
  -DTILES=ON \
  -DSOUND=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCATA_CLANG_TIDY_PLUGIN=OFF \
  -DLUA=ON \
  -DBACKTRACE=ON \
  -DLINKER=mold \
  -DUSE_XDG_DIR=ON \
  -DUSE_HOME_DIR=OFF \
  -DUSE_PREFIX_DATA_DIR=OFF \
  -DUSE_TRACY=ON \
  -DTRACY_VERSION=master \
  -DTRACY_ON_DEMAND=ON \
  -DTRACY_ONLY_IPV4=ON
```

모든 것이 잘 되면 CMake 파일이 생성됩니다! 이제 Ninja 명령을 실행하여 실제로 빌드하면 됩니다.

```sh
$ ninja -C build -j $(nproc) -k 0 cataclysm-bn-tiles
```

`$(nproc)`는 CPU의 "스레드" 수를 가져오지만, 컴파일에 더 적은 스레드를 사용하려면 더 낮은 숫자를 지정할 수 있습니다 (성능 손실 있음).
