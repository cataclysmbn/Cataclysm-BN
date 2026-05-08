---
title: CMake
---

## 前提条件

『Cataclysm-BN』をビルドするには、以下のライブラリとその開発用ヘッダーがインストールされている必要があります。

- 一般
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
- タイル
  - `SDL` >= 2.0.0
  - `SDL_image` >= 2.0.0 (with PNG and JPEG support)
  - `SDL_mixer` >= 2.0.0 (with Ogg Vorbis support)
  - `SDL_ttf` >= 2.0.0
  - `freetype`
- サウンド
  - `vorbis`
  - `libbz2`
  - `libz`

ローカライゼーション（地域化）ファイルをコンパイルするためには、さらに `gettext` パッケージが必要です。

## ビルド環境の構築

最新バージョンのソースコードは、
[git](https://github.com/cataclysmbnteam/Cataclysm-BN)から tarball またはクローンで取得できます。

```sh
git clone --filter=blob:none https://github.com/cataclysmbnteam/Cataclysm-BN.git
cd Cataclysm-BN
```

> [!TIP]
> `filter=blob:none` を使用すると、[ブロブなしクローン](https://github.blog/open-source/git/get-up-to-speed-with-partial-clone-and-shallow-clone/)が作成されます。これにより、ファイルが必要に応じてダウンロードされるため、最初のクローンがはるかに高速になります。

### UNIX 環境

上記で指定されたパッケージを、お使いのシステムパッケージマネージャで取得してください。

- Ubuntu ベースのディストリビューション (24.04 以降):

```sh
sudo apt install git cmake ninja-build mold g++-14 clang-20 ccache \
libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- Fedora ベースのディストリビューション:

```sh
sudo dnf install git cmake ninja-build mold clang ccache \
SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

#### コンパイラバージョンの確認

Cataclysm-BN』をビルドするには、少なくとも `gcc` 14 **および** `clang` が必要です。コンパイラのバージョンは、以下のコマンドで確認できます:

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
> **`gcc-{version}` はインストールされているが `gcc` が見つからない場合**
>
> `update-alternatives` を使用して、デフォルトのgcc バージョンを設定します:
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
> `clang`も同様に適用されます。
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
> ```

### Windows Subsystem for Linux (WSL)

`UNIX 環境`と同じ手順に従ってください。そのまま動作します (TM)。

`tiles`バージョンを使用する予定がある場合は、 [WSL 2 that supports GUI](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)を使用していること、および[対応するドライバーがインストールされていること](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#prerequisites)を確認してください。

### Windows 環境 (MSYS2)

1. こちらの手順に従ってください: https://msys2.github.io/
2. 『Cataclysm-BN』のビルド依存関係をインストールします:

```sh
pacman -S mingw-w64-x86_64-toolchain msys/git \
   	  mingw-w64-x86_64-cmake \
   	  mingw-w64-x86_64-SDL2_{image,mixer,ttf} \
   	  ncurses-devel \
      gettext \
      base-devel
```

これで、コンソール版および Tiles 版の Windows ビルド環境が整うはずです。

> [!NOTE]
>
> Jetbrains CLion でテストしようとしている場合は、内蔵の CMake バージョンを使用する代わりに、`msys32/mingw32`パスにある CMake のバージョンを指定してください。これにより、CMake がインストールされたパッケージを検出できるようになります。

### CMake によるビルド

CMake には、設定 (Configuration) とビルド (Build) のステップが分かれています。設定は CMake 自体を使用して行い、実際のビルドは `make` (Makefile ジェネレータの場合) またはビルドシステムに依存しない
`cmake --build .` を使用して行います。

CMake で『Cataclysm-BN』をビルドする方法には、ソースツリーの内部で行う方法と外部で行う方法の2つがあります。ソースツリー外ビルド（Out-of-source builds）には、1つのソースディレクトリから異なるオプションで複数のビルドを作成できるという利点があります。

> [!CAUTION]
>
> ソースツリーの内部でのビルドは**サポートされていません**。

#### プリセットを使用したビルド (推奨)

複数の事前定義された [ビルドプリセット](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) が利用可能であり、これによりビルドプロセスがわずか2つのコマンドに簡略化されます。

```sh
cmake --preset linux-slim
cmake --build build --preset linux-slim --target cataclysm-bn-tiles
```

これにより、実行ファイルは `out/build/linux-slim/`に配置されます。

> [!TIP]
> [clang-tidy plugin](../../reference/tooling.md#clang-tidy)と Tracy プロファイラを組み込んでビルドするには、`linux-full`を試してください。

> [!NOTE]
> 複数のターゲットを一度にビルドできます:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> または、`--parallel` オプションで最大スレッド数を制限できます:
>
> ```sh
> cmake --build build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### プリセットを使用しないビルド

『Cataclysm-BN』をソースツリー外でビルドするには:

```sh
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

上記の例では、ソースディレクトリ内にビルドディレクトリを作成していますが、必須ではありません。完全に別の場所に作成することも容易です。

ビルド後に『Cataclysm-BN』をインストールするには (必要に応じて su または sudo を使用して root 権限で):

```sh
cmake --install build
```

ビルドオプションを変更するには、コマンドラインでオプションを渡すか:

```sh
cmake .. -DOPTION_NAME=option_value
```

または、`ccmake` または `cmake-gui` フロントエンドのいずれかを使用します。これらは、すべてのオプションとそのキャッシュされた値をコンソールおよびグラフィカルな UI にそれぞれ表示します。

```sh
ccmake ..
cmake-gui ..
```

## Visual Studio / MSBuild 向けビルド

> [!CAUTION]
>
> このガイドはかなり古く、手動での依存関係管理が必要です。
>
> 最新の代替手段については、[vcpkgを使用した CMake Visual Studio ビルド](./vs_cmake.md)を参照してください。

CMake は、Visual Studio 自体で使用されるか、MSBuild コマンドラインコンパイラで使用される `.sln` および `.vcxproj` ファイルを生成できます
(フル機能の IDE を必要としない場合)。これにより、MSYS/Cygwin が提供するものよりも「ネイティブな」バイナリが得られます。

現時点では、限られたオプションの組み合わせのみがサポートされています（Tiles のみ、ローカライゼーションなし、バックトレースなし）。

必要なツールを入手します。

- CMake 公式サイトから - <https://cmake.org/download/>.
- Microsoft コンパイラ - <https://visualstudio.microsoft.com/downloads/?q=build+tools> "Build Tools for Visual Studio 2017" を選択します。インストール時に "Visual C++ Build Tools" オプションを選択します。
  - または、完全な Visual Studio をダウンロードしてインストールすることもできますが、必須ではありません。

必要なライブラリを入手します:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3) (「
  (Visual C++ 32/64-bit)」バージョンが必要です。以下も同様です)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (省略可能、サウンドサポート用)
- サポートされていない（以下の手順では未使用の）オプションライブラリ:
  - `ncurses` - ???

ライブラリのアーカイブを解凍します。

Windows のコマンドライン（または PowerShell）を開き、上記ライブラリを指すように環境変数を設定します（パスは適切に調整してください）:

```sh
set SDL2DIR=C:\path\to\SDL2-devel-2.0.9-VC
set SDL2TTFDIR=C:\path\to\SDL2_ttf-devel-2.0.15-VC
set SDL2IMAGEDIR=C:\path\to\SDL2_image-devel-2.0.4-VC
set SDL2MIXERDIR=C:\path\to\SDL2_mixer-devel-2.0.4-VC
```

(PowerShell の構文は `$env:SDL2DIR="C:\path\to\SDL2-devel-2.0.9-VC"`です)。

ビルドディレクトリを作成し、CMake の設定ステップを実行します。

```sh
cd <path to cbn sources>
mkdir build
cmake -B build -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

ビルドを実行します！

```
cmake --build build -j 2 -- /p:Configuration=Release
```

`-j 2` フラグはビルドの並列処理を制御します。省略することも可能です。
`/p:Configuration=Release` フラグは MSBuild に直接渡され、最適化を制御します。これを省略すると、代わりに `Debug`構成がビルドされます。PowerShell の場合は、最初の `--` の後にさらに `--`が必要になります。

結果のファイルは、ソースの Cataclysm-BN フォルダ内の `Release` ディレクトリに配置されます。それらを実行可能にするには、まずゲームデータにアクセスできるようにバイナリをソースの Cataclysm-BN ディレクトリ自体に移動し、次に必要な `.dll`ファイルを同じフォルダに配置する必要があります。これらは開発ライブラリのディレクトリ内の `lib/x86/` または `lib/x64/` の下に見つけることができます（64ビットマシンであっても x86 のものが必要になる可能性が高いです）。

DLL のコピーは一度限りの作業ですが、ビルドのたびにバイナリを `Release/` から移動する必要があります。これを少し自動化するために、CMake を設定し、 `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=` オプション（および
`CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG`の同様のオプション）を使用して、目的のバイナリ出力ディレクトリを設定できます。

ゲームを実行します。動作するはずです。

## ビルドオプション

CMake でサポートされているオプションの完全なリストを確認するには、`ccmake` または `cmake-gui`フロントエンドを実行するか、`cmake` を実行してビルドディレクトリから生成された`CMakeCache.txt`をテキストエディタで開きます。

```
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
```

### CMake 固有のオプション

- CMAKE_BUILD_TYPE=`<build type>`

コンパイル時に特定のビルド構成を選択します。 `release` は、通常の使用のためにデフォルトで最適化された
(-Os) ビルドを生成します。 `debug` は、バグ報告時に詳細なバックトレースを取得するためによく必要とされる、完全なデバッグシンボルを持つ、低速でサイズの大きい非最適化(-O0) ビルドを生成します。

**注記**: 既定では、コマンドラインで異なる構成オプションが渡されない限り、CMake は `debug` ビルドを生成します。

- CMAKE_INSTALL_PREFIX=`<full path>`

バイナリ、リソース、およびドキュメントファイルのインストールプレフィックス。

### CataclysmBN 固有のオプション

- CURSES=`<boolean>`

Curses バージョンをビルドします。

- TILES=`<boolean>`

グラフィック Tileset バージョンをビルドします。

- SOUND=`<boolean>`

ゲーム内サウンドと音楽のサポート。

- USE_HOME_DIR=`<boolean>`

セーブファイルにユーザーのホームディレクトリを使用します。

- LANGUAGES=`<str>`

指定された言語のローカライゼーションファイルをコンパイルします。例:

```
-DLANGUAGES="cs;de;el;es_AR;es_ES"
```

言語ファイルは、`RELEASE` ビルドタイプをビルドする場合にのみ自動的にコンパイルされることに注意してください。他のビルドタイプでは、 `translations_compile` `make` コマンドに追加する必要があります。例: `make all translations_compile`。

- DYNAMIC_LINKING=`<boolean>`

動的リンクを使用します。または、静的リンクを使用して MinGW の依存関係を削除することもできます。

- CUSTOM LINKER=`<str>`

[gold]、[lld]、[mold]などのカスタムリンカーを選択します。

- libbacktraceを使用しない場合は ld を選択します。
- libbacktraceを使用する場合は mold を選択します。これは最速のリンカーであり、gold よりも 24 倍優れています。

[gold]: https://en.wikipedia.org/wiki/Gold_(linker)
[lld]: https://lld.llvm.org
[mold]: https://github.com/rui314/mold

- BACKTRACE=`<boolean>`

クラッシュ時に、コンソールにバックトレースを出力します。デバッグビルドではデフォルトで `ON` です。

- LIBBACKTRACE=`<boolean>`

[libbacktrace]を使用してバックトレースを出力します。これにより、lld および mold がバックトレースを出力できるようになり、一般的に非常に高速です。

[libbacktrace]: https://github.com/ianlancetaylor/libbacktrace

- USE_TRACY=`<boolean>`

Tracy プロファイラを使用します。詳細については、[Tracyを使用したプロファイリング](../tracy.md)を参照してください。

- GIT_BINARY=`<str>`

既定の Git バイナリ名またはパスを上書きします。

- USE_PREFIX_DATA_DIR=`<boolean>`

リリースビルドでゲームデータに UNIX システムディレクトリを使用します。

- USE_XDG_DIR=`<boolean>`

セーブファイルおよび設定ファイルに XDG ディレクトリを使用します。

- TESTS=`<boolean>`

テストをビルドするかどうか。

したがって、Tiles と Sound サポートを使用して『Cataclysm-BN』をリリースモードでビルドするための CMake コマンドは、プロジェクト内の build ディレクトリで実行されていると仮定すると、次のようになります。

```sh
cmake ../ -DCMAKE_BUILD_TYPE=Release -DTILES=ON -DSOUND=ON
```
