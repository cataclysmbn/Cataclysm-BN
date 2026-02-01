---
title: MSYS2
---

> [!CAUTION]
>
> MSYS2에 대한 이 지침 세트는 오랫동안 업데이트되지 않았습니다. MSYS2로 빌드하는 선호 지침은 대신 [CMake](./cmake#windows-environment-msys2)에 있습니다.

64비트 Windows의 MSYS2에서 Cataclysm-BN을 컴파일하는 가이드입니다.

> [!WARNING]
>
> 이 지침은 CBN의 재배포 가능한 복사본을 만들기 위한 것이 _아닙니다_. 재배포가 목적이라면 웹사이트에서 공식 빌드를 다운로드하거나 [Linux에서 Windows로 크로스 컴파일](./makefile#cross-compile-to-windows-from-linux)하세요.

이 지침은 64비트 Windows 7과 64비트 버전의 MSYS2를 사용하여 작성되었습니다. 다른 버전의 Windows에서도 단계는 동일해야 합니다.

## 전제 조건:

- Windows 7, 8, 8.1 또는 10
- 약 10GB 여유 공간이 있는 NTFS 파티션 (MSYS2 설치에 약 2GB, 저장소에 약 3GB, ccache에 약 5GB)
- 64비트 버전의 MSYS2

**참고:** Windows XP는 지원되지 않습니다!

## 설치:

1. [MSYS2 홈페이지](http://www.msys2.org/)로 가서 인스톨러를 다운로드합니다.

2. 인스톨러를 실행합니다. 개발 전용 폴더 (C:\dev\msys64\ 또는 유사)에 설치하는 것이 좋지만 필수는 아닙니다.

3. 설치 후 MSYS2 64bit를 지금 실행합니다.

## 구성:

1. 패키지 데이터베이스 및 핵심 시스템 패키지를 업데이트합니다:

```bash
pacman -Syyu
```

2. MSYS는 cygheap 베이스 불일치를 알리고 포크된 프로세스가 예기치 않게 종료되었다고 알릴 수 있습니다. 이러한 오류는 `pacman` 업그레이드의 특성으로 인한 것으로 보이며 _안전하게 무시할 수 있습니다._ 터미널 창을 닫으라는 메시지가 표시됩니다. 그렇게 한 다음 MSYS2 MinGW 64비트 메뉴 항목을 사용하여 다시 시작합니다.

3. 나머지 패키지를 업데이트합니다:

```bash
pacman -Su
```

4. 컴파일에 필요한 패키지를 설치합니다:

```bash
pacman -S git make mingw-w64-x86_64-{astyle,ccache,gcc,libmad,libwebp,pkg-config,SDL2} mingw-w64-x86_64-SDL2_{image,mixer,ttf}
```

5. MSYS2를 닫습니다.

6. 시스템 전체 프로필 파일 (예: `C:\dev\msys64\etc\profile`)에서 경로 변수를 다음과 같이 업데이트합니다:

- 다음 줄을 찾습니다:

```
MSYS2_PATH="/usr/local/bin:/usr/bin:/bin"
MANPATH='/usr/local/man:/usr/share/man:/usr/man:/share/man'
INFOPATH='/usr/local/info:/usr/share/info:/usr/info:/share/info'
```

그리고

```
PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig:/lib/pkgconfig"
```

- 다음으로 바꿉니다:

```
MSYS2_PATH="/usr/local/bin:/usr/bin:/bin:/mingw64/bin"
MANPATH='/usr/local/man:/usr/share/man:/usr/man:/share/man:/mingw64/share/man'
INFOPATH='/usr/local/info:/usr/share/info:/usr/info:/share/info:/mingw64/share/man'
```

그리고

```
PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig:/lib/pkgconfig:/mingw64/lib/pkgconfig:/mingw64/share/pkgconfig"
```

## 클론 및 컴파일:

1. MSYS2를 열고 Cataclysm-BN 저장소를 클론합니다:

```bash
cd /c/dev/
git clone https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

**참고:** 전체 CBN 저장소와 모든 히스토리 (3GB)를 다운로드합니다. 테스트만 하는 경우 `--depth=1` (~350MB)을 추가하세요.

2. 다음 명령줄로 컴파일합니다:

```bash
make -j$((`nproc`+0)) CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 RUNTESTS=0
```

종료되지 않은 문자 상수에 대한 경고를 받을 것입니다. 제가 아는 한 컴파일에는 영향을 주지 않습니다.

**참고**: Sound와 Tiles 지원 및 모든 현지화 언어가 포함된 릴리스 버전을 컴파일하며 검사 및 테스트를 건너뛰고 빌드 가속화를 위해 ccache를 사용합니다. 다른 스위치를 사용할 수 있지만 `MSYS2=1`과 `DYNAMIC_LINKING=1`은 문제 없이 컴파일하는 데 필요합니다.

## 실행:

1. Cataclysm 디렉토리에서 MSYS2 내부에서 다음 명령으로 실행합니다:

```bash
./cataclysm-bn-tiles
```

**참고:** MSYS2 외부에서 컴파일된 실행 파일을 실행하려면 사용자 또는 시스템 `PATH` 변수에 MSYS2의 런타임 바이너리 경로 (예: `C:\dev\msys64\mingw64\bin`)를 추가해야 합니다.
