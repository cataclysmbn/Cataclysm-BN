# Tracy を用いたプロファイリング

[Tracy](https://github.com/wolfpld/tracy) は、パフォーマンス上のボトルネックを分析するために使用できるリアルタイムプロファイラです。これは、クライアントとプロファイラの2つの部分で構成されています。BN に統合されたクライアントは、プロファイリングデータをプロファイラに送信します。クライアントはオプトインであるため、プロファイリングを開始するには、tracy クライアントを含めて BN をビルドする必要があります。

## Tracy Profiler のインストール

> [!CAUTION]
>
> 適切に機能するためには、ゲームとプロファイラの両方が同じバージョンの tracy でビルドされている必要があります。
> [多数の問題](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/3253#discussion_r1545267113)のため、Windows バージョンは [`v0.10`](https://github.com/wolfpld/tracy/releases/tag/v0.10)を使用してい
> ますが、Linux バージョンは
> [`6d1deb5640ed11da01995fb1791115cfebe54dbf`](https://github.com/wolfpld/tracy/commit/6d1deb5640ed11da01995fb1791115cfebe54dbf)を使用しています。

### Linux

```sh
$ git clone https://github.com/wolfpld/tracy
$ cd tracy
$ git checkout 6d1deb5640ed11da01995fb1791115cfebe54dbf # BN tracy クライアントが使用するコミット
```

1. https://github.com/wolfpld/tracy をクローンします。

```sh
# ubuntu の場合 (X11)
$ sudo apt install cmake clang git libcapstone-dev xorg-dev dbus libgtk-3-dev

# ubuntu の場合 (wayland)
$ sudo apt install libglfw-dev libgtk-3-dev libfreetype6-dev libtbb-dev debuginfod libwayland-dev dbus libxkbcommon-dev libglvnd-dev meson cmake git wayland-protocols

# arch の場合、https://github.com/wolfpld/tracy/blob/master/.github/workflows/linux.yml#L16C12-L16C163
$ pacman -Syu --noconfirm && pacman -S --noconfirm --needed freetype2 tbb debuginfod wayland dbus libxkbcommon libglvnd meson cmake git wayland-protocols
```

2. 依存関係をインストールします。

```sh
$ cmake -B profiler/build -S profiler # wayland を使用している場合
```

3. cmake を設定します。tracy はデフォルトで wayland を使用します。X11 を使用したい場合は、`LEGACY=1` フラグを追加する必要があります。

> [!NOTE]
>
> #### X11 の場合
>
> ```sh
> $ cmake -DLEGACY=ON -B profiler/build -S profiler # X11 を使用している場合
> ```
>
> ctracy はデフォルトで wayland を使用します。X11 を使用したい場合は、LEGACY=1 フラグを追加する必要があります。

> [!NOTE]
>
> #### fileselector の修正
>
> ```sh
> $ cmake -DGTK_FILESELECTOR=ON -B profiler/build -S profiler
> ```
>
> [デフォルトのファイルセレクター (xdg-portal)](https://github.com/wolfpld/tracy/issues/764)の問題により、tracy はトレース履歴の開閉に失敗する場合があります。回避策として、`GTK_FILESELECTOR=ON` をコンパイルフラグに追加して gtk ファイルセレクターを使用します。

```sh
$ cmake --build profiler/build --config Release --parallel $(nproc)
```

4. バイナリをビルドします。ビルドされたバイナリは `./profiler/build/tracy-profiler`で利用可能になります。

> [!TIP]
>
> #### デスクトップエントリの追加
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
> アプリランチャーでプロファイラを利用可能にするには、上記の内容で `$HOME/.local/share/applications/tracy.desktop`
> ファイルを作成します。`<THE_PATH_WHERE_YOU_INSTALLED_TRACY>`
> を tracy をインストールしたパスに置き換えることを忘れないでください！

### Windows

![image](https://github.com/cataclysmbnteam/Cataclysm-BN/assets/54838975/b6f73c09-969c-4305-b8fb-070d14fb834a)

<https://github.com/wolfpld/tracy/releases>から、プリコンパイルされた実行可能ファイルをダウンロードしてください。

## Tracy クライアントを含めた BN のビルド

[cmake](../guides/building/cmake.md) を用いて `-D USE_TRACY=ON` フラグを付けてビルドします。例:

```sh
$ cmake -B build -DUSE_TRACY=ON ...other flags...
```

詳細については、[CMake オプション](building/cmake.md#cataclysmbn-specific-options) を参照してください。

## プロファイルするゾーンのマーク付け

プロファイルしたい関数に `ZoneScoped` をマークします。これは tracy GUI に表示されます。例:

```cpp
bool game::do_turn()
{
    ZoneScoped;

    /** Other code... */
}
```

他にもより複雑なプロファイリングマクロが利用可能です。詳細については、以下のリンクを確認してください。

- <https://github.com/wolfpld/tracy>
- <https://luxeengine.com/integrating-tracy-profiler-in-cpp/>
- [An Introduction to Tracy Profiler in C++ - Marcos Slomp - CppCon 2023](https://www.youtube.com/watch?v=ghXk3Bk5F2U)

## Tracy Profiler の使用

1. BN (`USE_TRACY=ON`でビルドしたもの)を起動し、tracy プロファイラを実行します。

![](../../../../../assets/img/tracy/main.png)

2. `connect` ボタンをクリックしてゲームに接続します。

![](../../../../../assets/img/tracy/stats.png)

3. プロファイリングデータが GUI に表示されます。
