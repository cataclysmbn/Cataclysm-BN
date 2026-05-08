---
title: Makefile
---

> [!CAUTION]
>
> makefile 빌드는 더 이상 사용되지 않으며 업데이트를 받지 않습니다. 대신 [CMake](./cmake.md)로 빌드하세요.

소스에서 Cataclysm을 빌드하려면 최소한 C++ 컴파일러, 몇 가지 기본 개발자 도구 및 필요한 빌드 의존성이 필요합니다. 정확한 패키지 이름은 배포판마다 크게 다르므로 이 가이드 부분은 프로세스에 대한 상위 수준의 이해를 제공하기 위한 것입니다.

## 컴파일러

여기에 세 가지 주요 선택이 있습니다: GCC, Clang, MXE.

- GCC는 거의 항상 Linux 시스템의 기본값이므로 이미 있을 가능성이 높습니다.
- Clang은 일반적으로 GCC보다 빠르므로 최신 nightly를 따라가려면 설치할 가치가 있습니다.
- MXE는 크로스 컴파일러이므로 Linux 머신에서 Windows용으로 컴파일하려는 경우에만 중요합니다.

(배포판에 별도의 패키지가 있을 수 있습니다. 예: `gcc`는 C 컴파일러만 포함하고 C++의 경우 `g++`를 설치해야 합니다.)

Cataclysm은 C++23 표준을 대상으로 하므로 이를 지원하는 컴파일러가 필요합니다. `g++` 버전이 C++23를 지원하는지 쉽게 확인할 수 있습니다:

```sh
$ g++ --std=c++23
g++: fatal error: no input files
compilation terminated.
```

다음과 같은 줄이 표시되면:

```sh
g++: error: unrecognized command line option '--std=c++23'
```

더 최신 버전의 GCC (`g++`)가 필요합니다.

일반적인 규칙은 새 컴파일러일수록 좋습니다.

## 도구

대부분의 배포판은 필수 빌드 도구를 단일 패키지 (Debian 및 파생 배포판에는 `build-essential`)로 패키징하거나 패키지 그룹 (Arch에는 `base-devel`)으로 패키징하는 것 같습니다. 위의 것을 사용할 수 있으면 사용해야 합니다. 그렇지 않으면 최소한 `make`가 필요하고 누락된 의존성이 있으면 파악해야 합니다 (있는 경우).

필수 사항 외에도 `git`이 필요합니다.

nightly를 따라가려면 `ccache`도 설치해야 하며, 이는 부분 빌드 속도를 상당히 높입니다.

## 의존성

일반 의존성, 선택적 의존성, 그리고 curses 또는 tiles 빌드를 위한 특정 의존성이 있습니다. 정확한 패키지 이름은 사용 중인 배포판과 배포판이 라이브러리와 개발 파일을 별도로 패키징하는지 여부 (예: Debian 및 파생 배포판)에 따라 다릅니다.

Arch에서 빌드한 대략적인 목록:

- General: `gcc-libs`, `glibc`, `zlib`, `bzip2`, `sqlite3`
- Optional: `intltool`
- Curses: `ncurses`
- Tiles: `sdl2`, `sdl2_image`, `sdl2_ttf`, `sdl2_mixer`, `freetype2`

예를 들어 Debian 및 파생 배포판의 curses 빌드에는 `libncurses5-dev` 또는 `libncursesw5-dev`도 필요합니다.

선택적 의존성에 대한 참고 사항:

- `intltool` - 현지화 파일 빌드용. 영어만 사용할 계획이라면 건너뛸 수 있습니다.

컴파일 오류 및/또는 컴파일된 바이너리에 대한 `ldd`의 출력을 읽어 누락된 것을 파악할 수 있어야 합니다.

## Make 플래그

소스에서 빌드하고 있으므로 여러 가지 선택을 해야 합니다:

- `NATIVE=` - 크로스 컴파일하는 경우에만 신경 써야 합니다.
- `RELEASE=1` - 이것이 없으면 디버그 빌드를 얻습니다 (아래 참고 참조)
- `LTO=1` - GCC/Clang으로 링크 타임 최적화를 활성화합니다.
- `TILES=1` - 이것이 있으면 tiles 버전을, 없으면 curses 버전을 얻습니다.
- `SOUND=1` - 사운드를 원하는 경우. `TILES=1` 필요
- `LANGUAGES=` - 현지화를 지정합니다. 자세한 내용은 [여기](#compiling-localization-files)를 참조하세요.
- `CLANG=1` - GCC 대신 Clang 사용
- `CCACHE=1` - ccache 사용
- `USE_LIBCXX=1` - Clang과 함께 libstdc++ 대신 libc++ 사용 (OS X 기본값)
- `BACKTRACE=1` - 크래시 시 스택 백트레이스 출력 지원

다른 가능한 옵션도 몇 가지 있습니다 - `Makefile`을 자유롭게 읽어보세요.

멀티 코어 컴퓨터가 있다면 옵션에 `-jX`를 추가하는 것이 좋습니다. 여기서 `X`는 사용 가능한 코어 수의 약 두 배여야 합니다.

예: `make -j4 CLANG=1 CCACHE=1 NATIVE=linux64 RELEASE=1 TILES=1`

위는 Clang과 ccache를 사용하여 64비트 Linux용으로 명시적으로 타일 릴리스를 빌드하고 4개의 병렬 프로세스를 사용합니다.

예: `make -j2`

위는 사용 중인 아키텍처용 디버그 가능한 curses 버전을 GCC를 사용하여 2개의 병렬 프로세스로 빌드합니다.

**디버그 참고**: segfault가 발생하고 스택 추적을 제공할 의향이 없는 한 항상 `RELEASE=1`로 빌드해야 합니다.

## 현지화 파일 컴파일

기본적으로 영어만 사용할 수 있으며 현지화 파일이 필요하지 않습니다.

특정 언어에 대한 파일을 컴파일하려면 make 명령에 `LANGUAGES="<lang_id_1> [lang_id_2] [...]"` 옵션을 추가해야 합니다:

```sh
make LANGUAGES="zh_CN zh_TW"
```

`lang/po` 디렉토리의 `*.po` 파일 이름에서 언어 ID를 얻거나 `LANGUAGES="all"`을 사용하여 사용 가능한 모든 현지화를 컴파일할 수 있습니다.

# Debian

Debian 기반 시스템에서 컴파일하는 지침입니다. 여기의 패키지 이름은 Ubuntu 12.10에서 유효하며 시스템에서 작동하거나 작동하지 않을 수 있습니다.

아래 빌드 지침은 항상 Cataclysm:BN 소스 디렉토리에서 실행한다고 가정합니다.

## Linux (native) ncurses 빌드

의존성:

- ncurses 또는 ncursesw (멀티바이트 로케일용)
- build essentials

설치:

```sh
sudo apt-get install libncurses5-dev libncursesw5-dev build-essential astyle libsqlite3-dev zlib1g-dev
```

### 빌드

실행:

```sh
make
```

## Linux (native) SDL 빌드

의존성:

- SDL
- SDL_ttf
- freetype
- build essentials
- libsdl2-mixer-dev - 사운드 지원과 함께 컴파일하는 경우 사용됩니다.

설치:

```sh
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libfreetype6-dev build-essential
```

올바른 버전의 SDL2가 설치되었는지 확인:

```sh
> sdl2-config --version
2.0.22
```

이전 버전의 SDL을 사용하면 [IME가 작동하지 않을 수 있습니다](https://github.com/cataclysmbn/Cataclysm-BN/issues/1497).

### 빌드

간단한 설치는 다음을 실행하여 수행할 수 있습니다:

```sh
make TILES=1
```

더 포괄적인 대안:

```sh
make -j2 TILES=1 SOUND=1 RELEASE=1 USE_HOME_DIR=1
```

-j2 플래그는 두 개의 병렬 프로세스로 컴파일한다는 의미입니다. 더 최신 프로세서에서는 -j4로 변경하거나 생략할 수 있습니다. 사운드를 원하지 않으면 해당 플래그도 생략할 수 있습니다. USE_HOME_DIR 플래그는 구성 및 저장과 같은 사용자 파일을 홈 폴더에 배치하여 백업을 쉽게 하며 생략할 수도 있습니다.

## Linux 32비트로 Linux 64비트에서 크로스 컴파일

의존성:

- 32비트 툴체인
- 32비트 ncursesw (멀티바이트 및 8비트 로케일 모두와 호환)

설치:

```sh
sudo apt-get install libc6-dev-i386 lib32stdc++-dev g++-multilib lib32ncursesw5-dev
```

### 빌드

실행:

```sh
make NATIVE=linux32
```

## Linux에서 Windows로 크로스 컴파일

Linux에서 Windows로 크로스 컴파일하려면 [MXE](http://mxe.cc)가 필요합니다.

이 지침은 Ubuntu 20.04용으로 작성되었지만 Debian 기반 환경에 적용할 수 있어야 합니다. 환경에 맞게 모든 패키지 관리자 지침을 조정하세요.

MXE는 MXE apt 저장소에서 설치하거나 (훨씬 빠름) 소스에서 컴파일할 수 있습니다.

### 바이너리 배포에서 MXE 설치

```sh
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9
sudo add-apt-repository "deb [arch=amd64] https://pkg.mxe.cc/repos/apt `lsb_release -sc` main"
sudo apt-get update
sudo apt-get install astyle bzip2 git make mxe-x86-64-w64-mingw32.static-{sdl2,sdl2-ttf,sdl2-image,sdl2-mixer}
```

`~/.profile`을 다음과 같이 편집:

```sh
export PLATFORM_64="/usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

전원 사이클 후 `make` 명령의 변수가 재설정되지 않도록 하기 위함입니다.

### 소스에서 MXE 설치

[MXE 요구 사항](http://mxe.cc/#requirements) 및 빌드 의존성 설치:

```sh
sudo apt install astyle autoconf automake autopoint bash bison bzip2 cmake flex gettext git g++ gperf intltool libffi-dev libgdk-pixbuf2.0-dev libtool libltdl-dev libssl-dev libxml-parser-perl lzip make mingw-w64 openssl p7zip-full patch perl pkg-config python ruby scons sed unzip wget xz-utils g++-multilib libc6-dev-i386 libtool-bin
```

MXE 저장소를 클론하고 CBN에 필요한 패키지를 빌드:

```sh
mkdir -p ~/src
cd ~/src
git clone https://github.com/mxe/mxe.git
cd mxe
make -j$((`nproc`+0)) MXE_TARGETS='x86_64-w64-mingw32.static' sdl2 sdl2_ttf sdl2_image sdl2_mixer sqlite
```

MXE에서 이 모든 패키지를 빌드하는 데 시간이 걸릴 수 있습니다 (빠른 컴퓨터에서도). 인내심을 가지세요. `-j` 플래그는 모든 프로세서 코어를 활용합니다.

32비트 및 64비트 모두를 위해 빌드할 계획이 아니라면 MXE_TARGETS를 조정할 수 있습니다.

`~/.profile`을 다음과 같이 편집:

```sh
export PLATFORM_64="~/src/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

전원 사이클 후 `make` 명령의 변수가 재설정되지 않도록 하기 위함입니다.

### 빌드 (SDL)

```sh
make -j$((`nproc`+0)) CROSS="${PLATFORM_64}" TILES=1 SOUND=1 RELEASE=1 BACKTRACE=0 PCH=0 bindist
```

## Linux에서 Mac OS X로 크로스 컴파일

절차는 Linux에서 Windows로 크로스 컴파일하는 것과 매우 유사합니다. Ubuntu 14.04 LTS에서 테스트되었지만 다른 배포판에서도 작동해야 합니다.

역사적 어려움으로 인해 런타임 최적화는 Mac OS X 대상으로 크로스 컴파일할 때 비활성화되어 있습니다. (컴파일 플래그로 `-O0`이 지정됩니다.) 자세한 내용은 [Pull Request #26564](https://github.com/cataclysmbn/Cataclysm-BN/pull/26564)를 참조하세요.

### 의존성

- OSX 크로스 컴파일 툴체인 [osxcross](https://github.com/tpoechtrager/osxcross)
- dmg 배포를 만들기 위한 `genisoimage` 및 [libdmg-hfsplus](https://github.com/planetbeing/libdmg-hfsplus.git)

컴파일하기 전에 모든 의존성 도구가 검색 `PATH`에 있는지 확인하세요.

### 설정

컴파일 환경을 설정하려면 다음 명령을 실행하세요:

```
git clone https://github.com/tpoechtrager/osxcross.git # 툴체인 클론
cd osxcross
cp ~/MacOSX10.11.sdk.tar.bz2 ./tarballs/ # 준비된 MacOSX SDK tarball을 제자리에 복사
OSX_VERSION_MIN=11 ./build.sh # 모든 것 빌드
```

[자세히 읽기](https://github.com/tpoechtrager/osxcross/blob/master/README.md#packaging-the-sdk)
대상으로 하는 최소 지원 OSX 버전에 유의하세요.

`osxcross` 내장 MacPorts로 컴파일하는 것은 다소 어렵고 현재 지원되지 않으므로 미리 패키징된 라이브러리 및 프레임워크 세트를 준비하세요. 디렉토리 트리는 다음과 같아야 합니다:

```
~/
├── Frameworks
│   ├── SDL2.framework
│   ├── SDL2_image.framework
│   ├── SDL2_mixer.framework
│   └── SDL2_ttf.framework
└── libs
    └── ncurses
        ├── include
        └── lib
```

각 프레임워크, dylib 및 헤더로 채워집니다. libncurses.5.4.dylib 버전으로 테스트되었습니다. 이러한 라이브러리는 OS X 10.11의 `homebrew` 바이너리 배포에서 얻었습니다. 프레임워크는 다음 [섹션](#sdl)에 설명된 대로 SDL 공식 웹사이트에서 얻었습니다.

### 빌드 (SDL)

타일 및 사운드 활성화 버전과 현지화 활성화를 빌드하려면:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all TILES=1 SOUND=1 FRAMEWORK=1
  OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

`x86_64-apple-darwin15-clang++`가 `PATH` 환경 변수에 있는지 확인하세요.

### 빌드 (ncurses)

현지화가 활성화된 전체 curses 버전을 빌드하려면:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

`x86_64-apple-darwin15-clang++`가 `PATH` 환경 변수에 있는지 확인하세요.

## Linux에서 Android로 크로스 컴파일

Android 빌드는 [Gradle](https://gradle.org/)을 사용하여 java 및 네이티브 C++ 코드를 컴파일하며 SDL의 [Android 프로젝트 템플릿](https://github.com/libsdl-org/SDL/tree/main/android-project)을 기반으로 합니다. 자세한 내용은 공식 SDL 문서 [README-android.md](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md)를 참조하세요.

Gradle 프로젝트는 저장소의 `android/` 아래에 있습니다. 명령줄을 통해 빌드하거나 [Android Studio](https://developer.android.com/studio/)에서 열 수 있습니다. 단순화를 위해 타일, 사운드 및 현지화를 포함한 모든 기능이 활성화된 SDL 버전만 빌드합니다.

### 의존성

- Java JDK 11
- SDL2 (2.0.8로 테스트됨, 프로젝트별 버그 수정이 있는 커스텀 포크 권장)
- SDL2_ttf (2.0.14로 테스트됨)
- SDL2_mixer (2.0.2로 테스트됨)
- SDL2_image (2.0.3로 테스트됨)

Gradle 빌드 프로세스는 [deps.zip](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/android/app/deps.zip)에서 의존성을 자동으로 설치합니다.

### 설정

Linux 의존성을 설치합니다. 데스크톱 Ubuntu 설치의 경우:

```sh
sudo apt-get install openjdk-8-jdk-headless
```

Android SDK 및 NDK 설치:

```sh
wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip
unzip sdk-tools-linux-4333796.zip -d ~/android-sdk
rm sdk-tools-linux-4333796.zip
~/android-sdk/tools/bin/sdkmanager --update
~/android-sdk/tools/bin/sdkmanager "tools" "platform-tools" "ndk-bundle"
~/android-sdk/tools/bin/sdkmanager --licenses
```

Android 환경 변수 내보내기 (`~/.bashrc` 끝에 추가 가능):

```sh
export ANDROID_SDK_ROOT=~/android-sdk
export ANDROID_HOME=~/android-sdk
export ANDROID_NDK_ROOT=~/android-sdk/ndk-bundle
export PATH=$PATH:$ANDROID_SDK_ROOT/platform-tools
export PATH=$PATH:$ANDROID_SDK_ROOT/tools
export PATH=$PATH:$ANDROID_NDK_ROOT
```

후속 빌드 속도를 높이기 위해 `ccache`를 사용하려면 이 추가 변수를 사용할 수도 있습니다:

```sh
export USE_CCACHE=1
export NDK_CCACHE=/usr/local/bin/ccache
```

**참고:** 시스템에서 `ccache` 경로가 다를 수 있습니다.

### Android 기기 설정

[Android 기기에서 개발자 옵션을 활성화](https://developer.android.com/studio/debug/dev-options)합니다. USB 케이블을 통해 기기를 PC에 연결하고 다음을 실행합니다:

```sh
adb devices
adb connect <devicename>
```

### 빌드

APK를 빌드하려면 Gradle 래퍼 명령줄 도구 (gradlew)를 사용합니다. Android Studio 문서는 [명령줄에서 앱을 빌드하는 방법](https://developer.android.com/studio/build/building-cmdline)에 대한 좋은 요약을 제공합니다.

디버그 APK를 빌드하려면 저장소의 `android/` 하위 폴더에서 다음을 실행합니다:

```sh
./gradlew assembleDebug
```

`./android/app/build/outputs/apk/`에 기기에 설치할 준비가 된 디버그 APK를 만듭니다.

디버그 APK를 빌드하고 adb를 통해 연결된 기기에 즉시 배포하려면 다음을 실행합니다:

```sh
./gradlew installDebug
```

서명된 릴리스 APK를 빌드하려면 (즉, 기기에 설치할 수 있는 것) [서명되지 않은 릴리스 APK를 빌드하고 수동으로 서명](https://developer.android.com/studio/publish/app-signing#signing-manually)합니다.

### Github 포크에서 Nightly 빌드 트리거

자체 Github 포크에서 nightly 빌드를 사용하여 Android APK를 성공적으로 빌드하려면 더미 Android 서명 키 세트를 초기화해야 합니다. Github Actions 워크플로우가 APK에 서명할 키 세트가 필요하기 때문에 필요합니다.

1. 6자 이상의 비밀번호를 만듭니다. 기억하고 github secrets에 `KEYSTORE_PASSWORD`로 저장합니다.
2. `keytool -genkey -v -keystore release.keystore -keyalg RSA -keysize 2048 -validity 10000 -alias dummy-key`로 키를 생성합니다. 비밀번호를 묻는 메시지가 표시되면 위의 비밀번호를 사용합니다.
3. 다음 내용으로 `keystore.properties.asc` 파일을 만듭니다:

```text
storeFile=release.keystore
storePassword=<1단계의 비밀번호 삽입>
keyAlias=dummy-key
keyPassword=<1단계의 비밀번호 삽입>
```

4. 1단계의 비밀번호를 사용하여 `gpg --symmetric --cipher-algo AES256 --armor release.keystore`로 `release.keystore`를 암호화합니다. 결과를 github secrets에 `KEYSTORE`로 저장합니다.
5. 1단계의 비밀번호를 사용하여 `gpg --symmetric --cipher-algo AES256 --armor keystore.properties`로 `keystore.properties`를 암호화합니다. 결과를 github secrets에 `KEYSTORE_PROPERTIES`로 저장합니다.

### 추가 참고 사항

앱은 `/sdcard/Android/data/com.cleverraven/cataclysmdda/files`에 데이터 파일을 저장합니다. 데이터는 데스크톱 버전과 하위 호환됩니다.

## 개발자를 위한 고급 정보

대부분의 사람들에게 간단한 Homebrew 설치로 충분합니다. 개발자를 위해 Mac OS X에서 Cataclysm을 빌드하는 데 대한 더 많은 기술적 세부 정보가 있습니다.

### SDL

타일 빌드에는 SDL2, SDL2_image, SDL2_ttf가 필요합니다. 선택적으로 사운드 지원을 위해 SDL2_mixer를 추가할 수 있습니다. Cataclysm은 SDL 프레임워크 또는 소스에서 빌드된 공유 라이브러리를 사용하여 빌드할 수 있습니다.

SDL 프레임워크 파일은 여기에서 다운로드할 수 있습니다:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)

`SDL2.framework`, `SDL2_image.framework`, `SDL2_ttf.framework`를 `/Library/Frameworks` 또는 `/Users/name/Library/Frameworks`에 복사합니다.

사운드 지원을 원하면 추가 SDL 프레임워크가 필요합니다:

- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (선택 사항, 사운드 지원용)

`SDL2_mixer.framework`를 `/Library/Frameworks` 또는 `/Users/name/Library/Frameworks`에 복사합니다.

또는 패키지 관리자를 사용하여 SDL 공유 라이브러리를 설치할 수 있습니다:

Homebrew:

```sh
brew install sdl2 sdl2_image sdl2_ttf
```

사운드 포함:

```sh
brew install sdl2_mixer libvorbis libogg
```

MacPorts:

```sh
sudo port install libsdl2 libsdl2_image libsdl2_ttf
```

사운드 포함:

```sh
sudo port install libsdl2_mixer libvorbis libogg
```

### ncurses

Cataclysm은 유니코드 문자를 광범위하게 사용하므로 와이드 문자 지원이 활성화된 ncurses가 필요합니다.

Homebrew:

```sh
brew install ncurses
```

MacPorts:

```sh
sudo port install ncurses
hash -r
```

### gcc

[Xcode용 명령줄 도구](https://developer.apple.com/downloads/)와 함께 설치된 gcc/g++ 버전은 실제로 clang과 동일한 Apple LLVM의 프론트엔드일 뿐입니다. 이것이 반드시 문제를 일으키는 것은 아니지만 이 버전의 gcc/g++는 clang 오류 메시지를 가지며 clang을 사용하는 것과 본질적으로 동일한 결과를 생성합니다. "실제" gcc/g++로 컴파일하려면 homebrew로 설치하세요:

```sh
brew install gcc
```

그러나 homebrew는 충돌을 피하기 위해 gcc를 gcc-{version} (여기서 {version}은 버전)으로 설치합니다. `/usr/local/bin/gcc-{version}`의 homebrew 버전을 `/usr/bin/gcc`의 Apple LLVM 버전 대신 사용하는 가장 간단한 방법은 필요한 것을 심볼릭 링크하는 것입니다.

```sh
cd /usr/local/bin
ln -s gcc-12 gcc
ln -s g++-12 g++
ln -s c++-12 c++
```

또는 `-12`로 끝나는 `/usr/local/bin/`의 모든 것에 대해 이 작업을 수행하려면:

```sh
find /usr/local/bin -name "*-12" -exec sh -c 'ln -s "$1" $(echo "$1" | sed "s/..$//")' _ {} \;
```

또한 이것이 작동하려면 `/usr/local/bin`이 `$PATH`에서 `/usr/bin` 앞에 나타나는지 확인해야 합니다.

`gcc -v`가 설치한 homebrew 버전을 표시하는지 확인하세요.

### brew clang

apple clang 대신 일반 clang을 사용하려면 Homebrew로 설치할 수 있습니다:

```sh
brew install llvm
```

그런 다음 make 명령에서 `COMPILER=$(brew --prefix llvm)/bin/clang++`로 컴파일러를 지정할 수 있습니다.

설치된 컴파일러가 원하는 컴파일러인지 항상 확인하는 것이 좋습니다.

```sh
$(brew --prefix llvm)/bin/clang++ --version
```

### 컴파일

Cataclysm 소스는 `make`를 사용하여 컴파일됩니다.

### Make 옵션

- `NATIVE=osx` OS X용 빌드. 모든 Mac 빌드에 필요합니다.
- `OSX_MIN=version` `-mmacosx-version-min=` 설정. 기본값은 11입니다.
- `TILES=1` 그래픽 타일 (및 그래픽 ASCII)이 있는 SDL 버전 빌드. `ncurses`로 빌드하려면 생략합니다.
- `SOUND=1` - 사운드를 원하는 경우. `TILES=1` 및 위에 언급된 추가 의존성 필요
- `FRAMEWORK=1` (타일만) OS X Frameworks 폴더 아래의 SDL 라이브러리에 링크. Homebrew 또는 Macports의 SDL 공유 라이브러리를 사용하려면 생략합니다.
- `LANGUAGES="<lang_id_1>[lang_id_2][...]"` 지정된 언어에 대한 현지화 파일 컴파일. 예: `LANGUAGES="zh_CN zh_TW"`. `LANGUAGES=all`을 사용하여 모든 현지화 파일을 컴파일할 수도 있습니다.
- `RELEASE=1` 최적화된 릴리스 버전 빌드. 디버그 빌드를 위해 생략합니다.
- `CLANG=1` 최신 Xcode용 명령줄 도구에 포함된 컴파일러인 [Clang](http://clang.llvm.org/)으로 빌드. gcc/g++를 사용하여 빌드하려면 생략합니다.
- `MACPORTS=1` Macports를 통해 설치된 의존성에 대해 빌드. 현재 `ncurses`만 해당됩니다.
- `USE_HOME_DIR=1` 사용자 파일 (구성, 저장, 묘지 등)을 사용자의 홈 디렉토리에 배치합니다. curses 빌드의 경우 `/Users/<user>/.cataclysm-bn`, SDL 빌드의 경우 `/Users/<user>/Library/Application Support/Cataclysm`입니다.
- `DEBUG_SYMBOLS=1` 최적화된 릴리스 바이너리를 빌드할 때 디버그 심볼을 유지하여 개발자가 크래시 사이트를 쉽게 찾을 수 있습니다.

위의 옵션 외에도 타일 빌드를 Mac 애플리케이션으로 패키징하는 `app` make 대상이 있으며, 터미널 없이 실행할 수 있는 완전한 타일 빌드인 `Cataclysm.app`에 넣습니다.

자세한 내용은 `Makefile`의 주석을 참조하세요.

### Make 예제

Clang을 사용하여 릴리스 SDL 버전 빌드:

```sh
make NATIVE=osx RELEASE=1 TILES=1 CLANG=1
```

Clang을 사용하여 릴리스 SDL 버전 빌드, OS X Frameworks 폴더의 라이브러리에 링크, 모든 언어 파일 빌드, `Cataclysm.app`로 패키징:

```sh
make app NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 LANGUAGES=all CLANG=1
```

Macports에서 제공하는 curses로 릴리스 curses 버전 빌드:

```sh
make NATIVE=osx RELEASE=1 MACPORTS=1 CLANG=1
```

### 실행

curses 빌드:

```sh
./cataclysm
```

SDL:

```sh
./cataclysm-bn-tiles
```

`app` 빌드의 경우 Finder에서 Cataclysm.app을 시작합니다.

### 테스트 스위트

빌드는 tests/cata_test에 테스트 실행 파일도 생성합니다. 다른 실행 파일처럼 호출하면 전체 테스트 스위트가 실행됩니다. `--help` 플래그를 전달하여 옵션을 나열합니다.

### dmg 배포

`dmgdist` 대상으로 멋진 dmg 배포 파일을 빌드할 수 있습니다. [dmgbuild](https://pypi.python.org/pypi/dmgbuild)라는 도구가 필요합니다. 이 도구를 설치하려면 먼저 Python이 필요합니다. Mac OS X >= 10.8을 사용하는 경우 Python 2.7이 OS와 함께 사전 설치되어 있습니다. 이전 버전의 OS X를 사용하는 경우 [공식 웹사이트](https://www.python.org/downloads/)에서 Python을 다운로드하거나 homebrew `brew install python`으로 설치할 수 있습니다. Python이 있으면 다음을 실행하여 `dmgbuild`를 설치할 수 있어야 합니다:

```sh
# pip를 설치합니다. 이미 설치되어 있으면 필요하지 않을 수 있습니다.
curl --silent --show-error --retry 5 https://bootstrap.pypa.io/get-pip.py | sudo python
# dmgbuild 설치
sudo pip install dmgbuild pyobjc-framework-Quartz
```

`dmgbuild`가 설치되면 다음과 같이 `dmgdist` 대상을 사용할 수 있습니다. 여기서 `USE_HOME_DIR=1` 사용은 사용자 구성 및 저장을 홈 디렉토리에 유지하면서 게임을 쉽게 업그레이드할 수 있게 하므로 중요합니다.

    make dmgdist NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 CLANG=1 USE_HOME_DIR=1

`Cataclysm.dmg` 파일이 표시되어야 합니다.

## Mac OS X 문제 해결

### 문제: 색상이 올바르게 표시되지 않음

터미널의 환경설정을 열고 `Preferences -> Settings -> Text`에서 `Use bright colors for bold text`를 켭니다.

# Windows

Windows에서 Visual Studio를 사용하여 빌드 환경을 설정하고 사용하는 방법에 대한 지침은 [COMPILING-VS-VCPKG.md](./vs_vcpkg.md)를 참조하세요.

Visual Studio 및 유사한 IDE 작업에 익숙한 사람에게는 아마도 가장 쉬운 솔루션일 것입니다. -->

## MSYS2로 빌드

Windows에서 MSYS2를 사용하여 빌드 환경을 설정하고 사용하는 방법에 대한 지침은 [COMPILING-MSYS.md](./msys.md)를 참조하세요.

MSYS2는 네이티브 Windows 애플리케이션과 UNIX 유사 환경 사이의 균형을 맞춥니다. 우리 프로젝트가 사용하는 명령줄 도구 (특히 JSON 린터)가 있으며 MSYS2 또는 CYGWIN이 제공하는 것과 같은 명령줄 환경 없이는 사용하기 어렵습니다.

## CYGWIN으로 빌드

Windows에서 CYGWIN을 사용하여 빌드 환경을 설정하고 사용하는 방법에 대한 지침은 [COMPILING-CYGWIN.md](./cygwin.md)를 참조하세요.

CYGWIN은 POSIX 환경을 더 완전하게 에뮬레이트하려고 시도하여 MSYS2보다 "더 유닉스"입니다. 일부 측면에서 덜 최신이며 MSYS2 패키지 관리자의 편의성이 부족합니다.

## Clang 및 MinGW64로 빌드

Clang은 기본적으로 Windows에서 MSVC를 사용하지만 MinGW64 라이브러리도 지원합니다. 배치 스크립트에서 `CLANG=1`을 `"CLANG=clang++ -target x86_64-pc-windows-gnu -pthread"`로 바꾸고 MinGW64가 경로에 있는지 확인하기만 하면 됩니다. 단위 테스트가 컴파일되려면 MinGW64의 `float.h`에 [패치](https://sourceforge.net/p/mingw-w64/mailman/message/36386405/)를 적용해야 할 수도 있습니다.

# BSD

최근 OpenBSD 및 FreeBSD 머신 (적절히 최신 컴파일러 포함)에서 CBN이 잘 빌드된다는 보고가 있으며 `Makefile`을 "그냥 작동"하게 만드는 작업이 진행 중이지만 우리는 그것에서 멀고 BSD 지원은 주로 사용자 기여를 기반으로 합니다. 결과는 다를 수 있습니다. 지금까지 기본적으로 모든 테스트는 amd64에서 수행되었지만 다른 아키텍처가 작동하지 않을 (알려진) 이유는 없습니다.

### 시스템 컴파일러로 FreeBSD/amd64 10.1에서 빌드

FreeBSD는 10.0부터 clang을 기본 컴파일러로 사용하고 libc++와 결합하여 기본적으로 C++17 지원을 제공합니다. 그러나 gmake가 필요합니다 (바이너리 패키지 예):

`pkg install gmake`

Tiles 빌드에는 SDL2도 필요합니다:

`pkg install sdl2 sdl2_image sdl2_mixer sdl2_ttf`

그런 다음 다음과 같이 빌드할 수 있어야 합니다 (`.profile`이나 다른 곳에서 CXXFLAGS 및 LDFLAGS를 설정할 수 있음):

```sh
export CXXFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
gmake # ncurses 빌드
gmake TILES=1 # tiles 빌드
```

저자는 빌드 VM에 X가 없어 타일 빌드를 테스트하지 않았습니다. 최소한 성공적으로 컴파일/링크됩니다.

### 포트에서 GCC 4.8.4로 FreeBSD/amd64 9.3에서 ncurses 버전 빌드

ncurses 빌드의 경우 `VERSION` 전에 `Makefile`에 추가:

```make
OTHERS += -D_GLIBCXX_USE_C99
CXX = g++48
CXXFLAGS += -I/usr/local/lib/gcc48/include
LDFLAGS += -rpath=/usr/local/lib/gcc48
```

참고: 또는 위를 `setenv`할 수 있습니다 (`OTHERS`를 `CXXFLAGS`에 병합), 알고 계셨죠.

그런 다음 `gmake RELEASE=1`로 빌드합니다.

### 포트/패키지에서 GCC 4.9.2로 OpenBSD/amd64 5.8에서 빌드

먼저 패키지에서 g++, gmake, libexecinfo를 설치합니다 (g++ 4.8 또는 4.9가 작동해야 함. 4.9가 테스트됨):

`pkg_add g++ gmake libexecinfo`

그런 다음 다음과 같이 빌드할 수 있어야 합니다:

`CXX=eg++ gmake`

5.8-release에서는 SDL2가 깨져 있어 ncurses 빌드만 가능합니다. 그러나 최근 -current 또는 스냅샷에서는 SDL2 패키지를 설치할 수 있습니다:

`pkg_add sdl2 sdl2-image sdl2-mixer sdl2-ttf`

그리고 다음으로 빌드:

`CXX=eg++ gmake TILES=1`

### 시스템 컴파일러로 NetBSD/amd64 7.0RC1에서 빌드

NetBSD는 버전 7.0부터 gcc 4.8.4를 가지게 되며 (또는 가질 예정), 이는 cataclysm을 빌드하기에 충분히 최신입니다. gmake 및 ncursesw를 설치해야 합니다:

`pkgin install gmake ncursesw`

그런 다음 다음과 같이 빌드할 수 있어야 합니다 (ncurses 빌드에 대한 LDFLAGS는 ncurses 구성 스크립트에서 처리됩니다. `.profile`이나 다른 곳에서 CXXFLAGS/LDFLAGS를 설정할 수 있음):

```sh
export CXXFLAGS="-I/usr/pkg/include"
gmake # ncurses 빌드
LDFLAGS="-L/usr/pkg/lib" gmake TILES=1 # tiles 빌드
```

SDL 빌드는 현재 컴파일되지만 테스트에서 실행되지 않았습니다 - segfault뿐만 아니라 gdb도 디버그 심볼을 읽을 때 segfault합니다! 결과는 다를 수 있습니다.
