# tracy로 프로파일링하기

[Tracy](https://github.com/wolfpld/tracy)는 성능 병목 지점을 분석하는 데 사용할 수 있는 실시간 프로파일러입니다. 클라이언트와 프로파일러의 두 부분으로 구성됩니다. BN에 통합된 클라이언트가 프로파일링 데이터를 프로파일러로 전송합니다. 클라이언트는 opt-in이므로 프로파일링을 시작하려면 tracy 클라이언트와 함께 BN을 빌드해야 합니다.

## Tracy Profiler 설치

> [!CAUTION]
>
> 게임과 프로파일러는 동일한 버전의 tracy로 빌드되어야 제대로 작동합니다. [여러 문제](https://github.com/cataclysmbn/Cataclysm-BN/pull/3253#discussion_r1545267113)로 인해 Windows 버전은 [`v0.10`](https://github.com/wolfpld/tracy/releases/tag/v0.10)을 사용하고 Linux 버전은 [`6d1deb5640ed11da01995fb1791115cfebe54dbf`](https://github.com/wolfpld/tracy/commit/6d1deb5640ed11da01995fb1791115cfebe54dbf)를 사용합니다.

### Linux

```sh
$ git clone https://github.com/wolfpld/tracy
$ cd tracy
$ git checkout 6d1deb5640ed11da01995fb1791115cfebe54dbf # BN tracy 클라이언트가 사용하는 커밋
```

1. <https://github.com/wolfpld/tracy>를 클론합니다.

```sh
# ubuntu (X11)용
$ sudo apt install cmake clang git libcapstone-dev xorg-dev dbus libgtk-3-dev

# ubuntu (wayland)용
$ sudo apt install libglfw-dev libgtk-3-dev libfreetype6-dev libtbb-dev debuginfod libwayland-dev dbus libxkbcommon-dev libglvnd-dev meson cmake git wayland-protocols

# arch용, https://github.com/wolfpld/tracy/blob/master/.github/workflows/linux.yml#L16C12-L16C163에서 복사
$ pacman -Syu --noconfirm && pacman -S --noconfirm --needed freetype2 tbb debuginfod wayland dbus libxkbcommon libglvnd meson cmake git wayland-protocols
```

2. 의존성을 설치합니다.

```sh
$ cmake -B profiler/build -S profiler # wayland 사용시
```

3. cmake를 설정합니다. 기본적으로 tracy는 wayland를 사용합니다. X11을 사용하려면 `LEGACY=1` 플래그를 추가해야 합니다.

> [!NOTE]
>
> #### X11용
>
> ```sh
> $ cmake -DLEGACY=ON -B profiler/build -S profiler # X11 사용시
> ```
>
> tracy는 기본적으로 wayland를 사용하므로 X11을 사용하려면 `LEGACY=1` 플래그를 추가해야 합니다.

> [!NOTE]
>
> #### fileselector 수정
>
> ```sh
> $ cmake -DGTK_FILESELECTOR=ON -B profiler/build -S profiler
> ```
>
> [기본 fileselector (xdg-portal)](https://github.com/wolfpld/tracy/issues/764)의 문제로 인해 tracy가 추적 히스토리를 열거나 저장하는 데 실패할 수 있습니다. 해결 방법으로 컴파일 플래그에 `GTK_FILESELECTOR=ON`을 추가하여 gtk fileselector를 사용합니다.

```sh
$ cmake --build profiler/build --config Release --parallel $(nproc)
```

4. 바이너리를 빌드합니다. `./profiler/build/tracy-profiler`에서 사용할 수 있습니다.

> [!TIP]
>
> #### 데스크톱 항목 추가
>
> ```
> [Desktop Entry]
> Version=1.0
> Type=Application
> Name=Tracy Profiler
> GenericName=Code profiler
> GenericName[pl]=Profiler kodu
> GenericName[ko]=코드 프로파일러
> Comment=Examine code to see where it is slow
> Comment[pl]=Znajdowanie wolno wykonującego się kodu
> Comment[ko]=코드 분석해서 느린 곳 찾기
> Exec=<THE_PATH_WHERE_YOU_INSTALLED_TRACY>/profiler/build/tracy-profiler %f
> Icon=<THE_PATH_WHERE_YOU_INSTALLED_TRACY>/icon/icon.ico
> Terminal=false
> Categories=Development;Profiling;
> MimeType=application/tracy;
> X-Desktop-File-Install-Version=0.26
> ```
>
> 앱 런처에서 프로파일러를 사용할 수 있게 하려면 `$HOME/.local/share/applications/tracy.desktop` 파일을 다음 내용으로 생성합니다. `<THE_PATH_WHERE_YOU_INSTALLED_TRACY>`를 tracy를 설치한 경로로 바꿔야 합니다!

### Windows

![image](https://github.com/cataclysmbn/Cataclysm-BN/assets/54838975/b6f73c09-969c-4305-b8fb-070d14fb834a)

<https://github.com/wolfpld/tracy/releases>에서 미리 컴파일된 실행 파일을 다운로드합니다.

## Tracy 클라이언트와 함께 BN 빌드

[cmake](../guides/building/cmake.md)에서 `-D USE_TRACY=ON` 플래그로 빌드합니다. 예를 들어:

```sh
$ cmake -B build -DUSE_TRACY=ON ...기타 플래그...
```

더 많은 정보는 [CMake 옵션](building/cmake.md#cataclysmbn-specific-options)을 참조하세요.

## 프로파일링할 영역 표시

프로파일링하려는 함수에 `ZoneScoped`를 표시합니다. tracy GUI에 표시됩니다. 예를 들어:

```cpp
bool game::do_turn()
{
    ZoneScoped;

    /** 기타 코드... */
}
```

더 복잡한 프로파일링 매크로도 사용할 수 있습니다. 자세한 내용은 다음 링크를 확인하세요:

- <https://github.com/wolfpld/tracy>
- <https://luxeengine.com/integrating-tracy-profiler-in-cpp/>
- [An Introduction to Tracy Profiler in C++ - Marcos Slomp - CppCon 2023](https://www.youtube.com/watch?v=ghXk3Bk5F2U)

## Tracy Profiler 사용

1. BN (`USE_TRACY=ON`으로 빌드)을 시작하고 tracy 프로파일러를 실행합니다.

![](../../../../../assets/img/tracy/main.png)

2. `connect` 버튼을 클릭하여 게임에 연결합니다.

![](../../../../../assets/img/tracy/stats.png)

3. 프로파일링 데이터가 GUI에 표시됩니다.
