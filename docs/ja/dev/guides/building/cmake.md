---
title: CMake
---

## 前提条件

CataclysmBN をビルドするには、以下のライブラリと開発用ヘッダーをインストールしておく必要があります。

- 一般
  - `cmake` >= 3.2.4
  - `clang` >= 22
  - `gcc-libs` または同等の C++ ランタイムライブラリ
  - `glibc`
  - `zlib`
  - `bzip2`
  - `sqlite3`
  - `SDL` >= 3.0.0 *
  - `SDL_shadercross` >= 3.0.0 *
- Curses
  - `ncurses`
- タイル
  - `SDL_image` >= 3.0.0 (PNG および JPEG サポート付き) *
  - `SDL_mixer` >= 3.0.0 (Ogg Vorbis サポート付き) *
  - `SDL_ttf` >= 3.0.0 *
  - `freetype`
- サウンド
  - `vorbis`
  - `libbz2`
  - `libz`

> [!NOTE]
>
> `*` 印の付いた依存関係は、見つからない場合に自動でビルドされます。

ローカライゼーションファイルをコンパイルするには、`gettext` パッケージも必要です。

## ビルド環境

最新バージョンのソースコード tarball は [git](https://github.com/cataclysmbn/Cataclysm-BN) から取得できます。

```sh
git clone --filter=blob:none https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

> [!TIP]
> `filter=blob:none` は [blobless clone](https://github.blog/open-source/git/get-up-to-speed-with-partial-clone-and-shallow-clone/) を作成します。ファイルを必要に応じてダウンロードするため、最初のクローンが大幅に高速になります。

### UNIX 環境

上記のパッケージを、使用しているシステムのパッケージマネージャーでインストールしてください。

- Ubuntu ベースのディストリビューション (26.04 以降):

```sh
sudo apt install git cmake ninja-build mold clang-22 llvm-22 ccache \
libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
libfreetype-dev bzip2 zlib1g-dev libvorbis-dev libncurses-dev \
gettext libflac++-dev libsqlite3-dev zlib1g-dev
```

- Fedora ベースのディストリビューション:

```sh
sudo dnf install git cmake ninja-build mold clang llvm ccache \
SDL3-devel SDL3_image-devel SDL3_ttf-devel \
freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
sqlite-devel zlib-devel
```

> [!NOTE]
> Ubuntu と Fedora は SDL3_mixer または shadercross を提供していません。
> これらのライブラリはコンパイル時に自動でビルドされます。

#### コンパイラバージョンの確認

CataclysmBN をビルドするには Clang 22 以降が必要です。コンパイラのバージョンは次のコマンドで確認できます。

```sh
$ clang++ --version
clang version 22.1.6 (Fedora 22.1.6-1.fc44)
Target: x86_64-redhat-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
```

> [!TIP]
>
> **`clang-{version}` はインストールされているが `clang` が見つからない場合**
>
> `update-alternatives` を使用してデフォルトの Clang バージョンを設定します。
>
> ```sh
> sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100
> sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100
> ```
>
> Ubuntu で `llvm-ar-22` や `llvm-ranlib-22` のようなバージョン付き LLVM binutils だけがインストールされる場合は、これらの名前も登録してください。
>
> ```sh
> sudo update-alternatives --install /usr/bin/llvm-ar llvm-ar /usr/bin/llvm-ar-22 100
> sudo update-alternatives --install /usr/bin/llvm-ranlib llvm-ranlib /usr/bin/llvm-ranlib-22 100
> ```

### macOS 環境

[Homebrew](https://brew.sh/) で依存関係をインストールします。

```sh
brew install cmake ninja ccache sdl3 sdl3_image sdl3_ttf sdl3_mixer \
  freetype gettext sqlite pkg-config ncurses flac
```

> [!NOTE]
> Xcode 16 以降に同梱されている Apple Clang は、CataclysmBN に必要な C++23 機能をサポートしています。
> 別のコンパイラをインストールする必要はありません。

### Windows Subsystem for Linux (WSL)

`UNIX 環境` と同じ手順に従ってください。そのまま動作します (TM)。

`tiles` を使用する予定がある場合は、[GUI をサポートする最新の WSL 2](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps) を使用しており、[対応するドライバー](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#prerequisites) をインストール済みであることを確認してください。

### Windows 環境 (MSYS2)

1. https://msys2.github.io/ の手順に従ってください。
2. CataclysmBN のビルド依存関係をインストールします。

```sh
pacman -S mingw-w64-x86_64-toolchain msys/git \
   	  mingw-w64-x86_64-cmake \
   	  mingw-w64-x86_64-SDL2_{image,mixer,ttf} \
   	  ncurses-devel \
      gettext \
      base-devel
```

これで、Windows のコンソール版とタイル版をビルドする環境が整います。

> [!NOTE]
>
> Jetbrains CLion でテストする場合は、内蔵 CMake ではなく `msys32/mingw32` パスにある CMake を指定してください。これにより、CMake がインストール済みパッケージを検出できるようになります。

### CMake によるビルド

CMake には設定とビルドの別々のステップがあります。設定は CMake 自体で行い、実際のビルドはビルドシステムに依存しない `cmake --build .` で行います。

CMake で CataclysmBN をビルドする方法は、ソースツリーの内部で行う方法と外部で行う方法の 2 つがあります。ソースツリー外ビルドには、1 つのソースディレクトリから異なるオプションの複数のビルドを作成できる利点があります。

> [!CAUTION]
>
> ソースツリー内部でのビルドは **サポートされていません**。

#### プリセットを使用したビルド (推奨)

事前定義された [ビルドプリセット](#ビルドプリセット) が複数用意されており、ビルド手順を 2 つのコマンドに簡略化できます。

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

実行ファイルはそれぞれ `out/build/linux-slim/` または `out/build/osx-arm-slim/` に配置されます。

> [!TIP]
> [clang-tidy プラグイン](../../reference/tooling.md#clang-tidy)と Tracy プロファイラサポートを含めてビルドするには、`linux-full` を試してください。

> [!NOTE]
> 複数のターゲットを一度にビルドできます。
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles cata_test-tiles
> ```
>
> また、`--parallel` オプションで最大スレッド数を制限できます。
>
> ```sh
> cmake --build --preset linux-slim --target cataclysm-bn-tiles --parallel 4
> ```

#### プリセットを使用しないビルド

CataclysmBN をソースツリー外でビルドするには:

```sh
mkdir -p out/build/custom
cmake -B out/build/custom -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/custom
```

上記の例では `out/build/` の下にビルドディレクトリを作成していますが、必須ではありません。完全に別の場所に作成してもかまいません。

ビルド後に CataclysmBN をインストールするには、必要に応じて `su` または `sudo` を使って root 権限で実行します。

```sh
cmake --install out/build/custom
```

### 配布パッケージの作成

> [!TIP]
> 配布用プリセットの一覧は [ビルドプリセット](#ビルドプリセット) を確認してください。

ビルドプリセットを使ってポータブル配布パッケージを作成します。

```sh
# タイル配布用に設定
cmake --preset dist-tiles

# ゲームとツールをビルド
cmake --build --preset dist-tiles

# 配布パッケージを作成
cmake --install build --prefix cataclysmbn-linux-tiles
```

次の構造を持つ自己完結型ディレクトリが作成されます。

```
cataclysmbn-linux-tiles/
├── cataclysm-bn-tiles     # ゲーム実行ファイル
├── cataclysm-launcher     # ランチャースクリプト
├── json_formatter         # JSON 整形ツール
├── data/                  # ゲームデータファイル
├── gfx/                   # タイルセット
├── lang/                  # 翻訳
├── doc/                   # ドキュメント
├── README.md
├── LICENSE.txt
└── VERSION.txt
```

配布用 tarball を作成するには:

```sh
tar -czvf cataclysmbn-linux-tiles.tar.gz cataclysmbn-linux-tiles
```

> [!TIP]
> `cataclysm-launcher` スクリプトは、正しい作業ディレクトリとライブラリパスを設定します。
> 任意の場所からゲームを実行するために使用してください。

#### ポータブル vs システムインストール

| オプション   | `USE_PREFIX_DATA_DIR=OFF` | `USE_PREFIX_DATA_DIR=ON`     |
| ------------ | ------------------------- | ---------------------------- |
| データの場所 | `./data/`                 | `/usr/share/cataclysm-bn/`   |
| 設定の場所   | `./config/`               | `~/.config/cataclysm-bn/`    |
| 適した用途   | ポータブル/リリースビルド | システムパッケージ (deb/rpm) |

> [!TIP]
> ビルドを設定する方法は [ビルドオプション](#ビルドオプション) を確認してください。

## Visual Studio / MSBuild 向けビルド

> [!CAUTION]
>
> このガイドはかなり古く、依存関係を手動で管理する必要があります。
>
> 最新の代替手段については、[vcpkg を使用した CMake Visual Studio ビルド](./vs_cmake.md)を参照してください。

CMake は、Visual Studio 自体または MSBuild コマンドラインコンパイラで使用される `.sln` と `.vcxproj` ファイルを生成できます。フル機能の IDE を使いたくない場合でも利用でき、MSYS/Cygwin が提供するものより「ネイティブ」なバイナリを得られます。

現時点では、限られたオプションの組み合わせのみがサポートされています (タイルのみ、ローカライゼーションなし、バックトレースなし)。

ツールを入手します。

- CMake 公式サイト - <https://cmake.org/download/>。
- Microsoft コンパイラ - <https://visualstudio.microsoft.com/downloads/?q=build+tools> から “Build Tools for Visual Studio 2017” を選択します。インストール時に “Visual C++ Build Tools” オプションを選択してください。
  - または、完全な Visual Studio をダウンロードしてインストールすることもできますが、必須ではありません。

必要なライブラリを入手します。

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3) (“Visual C++ 32/64-bit” 版が必要です。以下も同様です)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (省略可能、サウンドサポート用)
- サポートされていない (以下の手順では使用しない) 省略可能なライブラリ:
  - `ncurses` - ???

ライブラリのアーカイブを展開します。

Windows のコマンドライン (または PowerShell) を開き、上記ライブラリを指すように環境変数を設定します。パスは適宜調整してください。

```sh
set SDL2DIR=C:\path\to\SDL2-devel-2.0.9-VC
set SDL2TTFDIR=C:\path\to\SDL2_ttf-devel-2.0.15-VC
set SDL2IMAGEDIR=C:\path\to\SDL2_image-devel-2.0.4-VC
set SDL2MIXERDIR=C:\path\to\SDL2_mixer-devel-2.0.4-VC
```

PowerShell の構文は `$env:SDL2DIR="C:\path\to\SDL2-devel-2.0.9-VC"` です。

CMake の設定ステップを実行します。

```sh
cd <path to cbn sources>
cmake -B out/build/msbuild -DTILES=ON -DLANGUAGES=none -DBACKTRACE=OFF -DSOUND=ON
```

ビルドします!

```
cmake --build out/build/msbuild --config Release --parallel 2
```

`--parallel 2` フラグはビルドの並列処理を制御します。必要なければ省略できます。`--config Release` フラグは最適化された Visual Studio 構成を選択します。省略すると、代わりに `Debug` 構成がビルドされます。

結果のファイルは、ソース Cataclysm-BN フォルダ内の `Release` ディレクトリに置かれます。実行するには、まずゲームデータにアクセスできるようにバイナリをソース Cataclysm-BN ディレクトリ自体へ移動し、次に必要な `.dll` ファイルを同じフォルダに配置する必要があります。これらは開発ライブラリの `lib/x86/` または `lib/x64/` ディレクトリ内で見つかります。64 ビットマシンでも `x86` のものが必要になる可能性が高いです。

DLL のコピーは一度だけで済みますが、ビルドのたびにバイナリを `Release/` の外へ移動する必要があります。少し自動化するには、CMake を設定する際に `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=` オプションや `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` の同様のオプションで、目的のバイナリ出力ディレクトリを指定できます。

ゲームを実行します。動作するはずです。

## ビルドオプション

ビルドオプションを変更するには、コマンドラインでオプションを渡します。

```sh
cmake -DOPTION_NAME1=option_value1 [-DOPTION_NAME2=option_value2 [...]]
```

または、`ccmake` か `cmake-gui` フロントエンドを使用します。これらはすべてのオプションとキャッシュされた値を、それぞれコンソールとグラフィカル UI に表示します。

```sh
cmake --preset linux-slim
ccmake out/build/linux-slim
```

```sh
cmake --preset linux-slim
cmake-gui -S . -B out/build/linux-slim
```

### CMake 固有のオプション

- CMAKE_BUILD_TYPE=`<build type>`

コンパイル時に特定のビルド構成を選択します。`release` は通常使用向けのデフォルトの最適化 (-Os) ビルドを生成します。`debug` は、バグ報告時に詳細なバックトレースを取得するためによく必要になる、完全なデバッグシンボル付きの低速で大きな非最適化 (-O0) ビルドを生成します。

**注記**: 既定では、コマンドラインで別の構成オプションを渡さない限り、CMake は `debug` ビルドを生成します。

- CMAKE_INSTALL_PREFIX=`<full path>`

バイナリ、リソース、ドキュメントファイルのインストールプレフィックス。

### CataclysmBN 固有のオプション

| オプション            | 既定値                                    | 効果                                                                                                        |
| --------------------- | ----------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `CURSES`              | `ON`                                      | curses 版をビルドします。                                                                                   |
| `TILES`               | `OFF`                                     | グラフィックタイルセット版をビルドします。                                                                  |
| `SOUND`               | `ON`                                      | オーディオサポートをビルドします。                                                                          |
| `LANGUAGES`           | `" "`                                     | 指定した言語サポートをビルドします。                                                                        |
| `TESTS`               | `ON`                                      | テストユニットをビルドします。                                                                              |
| `USE_HOME_DIR`        | `ON` (WIN32 では `OFF`)                   | セーブファイルと設定ファイルに `$HOME` ディレクトリを使用します。                                           |
| `USE_XDG_DIR`         | `OFF`                                     | セーブファイルと設定ファイルに XDG ディレクトリを使用します。                                               |
| `USE_PREFIX_DATA_DIR` | `OFF`                                     | リリースビルドでゲームデータに UNIX システムディレクトリを使用します。                                      |
| `JSON_FORMAT`         | `OFF`                                     | JSON フォーマッタをビルドします。                                                                           |
| `CATA_CCACHE`         | `ON`                                      | ccache を探し、ccache を使ってビルドしようとします。                                                        |
| `BUILD_SDL3`          | `OFF`                                     | システムライブラリの代わりに SDL3 を強制的にビルドします。                                                  |
| `BUILD_SHADERCROSS`   | `ON`                                      | shadercross が `PATH` にない場合、SDL_shadercross をソースからビルドします。                                |
| `SHADER_TARGETS`      | `spirv;msl` (WIN32 では `dxil;spirv;msl`) | ビルドするシェーダー。                                                                                      |
| `DYNAMIC_LINKING`     | `ON`                                      | 動的リンクを使用します。または静的リンクを使って MinGW 依存を取り除きます。                                 |
| `LINKER`              | `" "`                                     | 使用するカスタムリンカー。                                                                                  |
| `BACKTRACE`           | `ON`                                      | クラッシュ時のスタックバックトレース出力をサポートします。                                                  |
| `LIBBACKTRACE`        | `OFF`                                     | libbacktrace でバックトレースを出力します。                                                                 |
| `USE_TRACY`           | `OFF`                                     | Tracy プロファイラを使用します。詳細は [Tracy を使用したプロファイリング](../tracy.md) を参照してください。 |
| `GIT_BINARY`          | `" "`                                     | Git バイナリの名前またはパス。                                                                              |

### ビルドプリセット

詳細は [CMake ドキュメント](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) を確認してください。

| プリセット名               | 説明                                                |
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
> macOS 配布プリセットは `dmgdist` ターゲットで DMG をビルドします。`dmgbuild` が `PATH` 上にある必要があります。
> dmgdist と biplist をまだインストールしていない場合は、pip でインストールできます。
>
> ```sh
> python3 -m pip install dmgbuild biplist
> ```

### ローカルで翻訳込みのビルドを行う

2026 年以降、翻訳ファイル (`.po` ファイル) はリポジトリに含まれなくなりました。CI が Transifex から取得し、ワークフローアーティファクトとして保存したうえで、リリースパッケージのビルドに使用します。

ローカルビルドでは、次のいずれかを使用してください。

#### オプション 1: 翻訳なしでビルドする (最速)

翻訳作業をしていない場合は無効化してください。

```sh
cmake --preset linux-full -DLANGUAGES=none
cmake --build --preset linux-full
```

#### オプション 2: Transifex から翻訳を取得する

ローカルで翻訳をテストする必要があり、Transifex へのアクセス権がある場合:

1. Transifex CLI をインストールします。

```sh
curl -sL https://github.com/transifex/cli/releases/download/v1.6.17/tx-linux-amd64.tar.gz | sudo tar zxvf - -C /usr/bin tx
```

2. 翻訳ファイルを取得します。

```sh
tx pull --force --all
```

3. 翻訳を有効にしてビルドします。

```sh
cmake --preset linux-full -DLANGUAGES=all
cmake --build --preset linux-full
```

#### オプション 3: `translations` ワークフローアーティファクトをダウンロードする

Transifex へのアクセス権がない場合は、翻訳ワークフローが生成したアーティファクトを使用してください。

1. [Actions](https://github.com/cataclysmbn/Cataclysm-BN/actions) で最近成功したワークフロー実行を開きます
2. `translations` アーティファクトをダウンロードします
3. `lang/po` と `src/lang_stats.inc` をローカルチェックアウトに展開します
4. 通常どおり `-DLANGUAGES=all` でビルドします

> [!NOTE]
> ほとんどのコード変更では翻訳は不要です。ローカライズされた出力をテストするのでなければ `-DLANGUAGES=none` を使用してください。

> [!NOTE]
> リリースアーカイブに含まれるのは、パッケージ用にコンパイル済みの `lang/mo` ファイルだけです。ローカルで翻訳を再ビルドするために必要な `lang/po` ソースは含まれません。
