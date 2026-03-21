---
title: Makefile
---

> [!CAUTION]
>
> Makefile ビルドは非推奨であり、更新されません。[CMake](./cmake.md) を使用してビルドしてください。

ソースから Cataclysm をビルドするには、少なくとも C++ コンパイラ、いくつかの基本的な開発者ツール、および必要なビルド依存関係が必要です。正確なパッケージ名はディストリビューションによって大きく異なるため、このガイドのこの部分は、プロセスのより高度な理解を提供することを目的としています。

## コンパイラ

ここには主に 3 つの選択肢があります: GCC、Clang、そして MXE です。

- GCC は Linux システムではほぼ既定であるため、すでにお持ちの可能性が高いです。
- Clang は通常 GCC よりも高速であるため、最新のナイトリービルドに追いつく予定がある場合はインストールする価値があります。
- MXE はクロスコンパイラであるため、Linux マシンで Windows 向けにコンパイルする予定がある場合にのみ重要です。

(ディストリビューションによってはパッケージが分かれている場合があることに注意してください。たとえば、`gcc` には C コンパイラのみが含まれ、C++ の場合は `g++`をインストールする必要があります。)

Cataclysm は C++23 標準をターゲットとしており、これをサポートするコンパイラが必要です。お使いの `g++` のバージョンが C++23 をサポートしているかどうかは、以下のコマンドを実行することで簡単に確認できます:

```sh
$ g++ --std=c++23
g++: fatal error: no input files
compilation terminated.
```

次のような行が表示された場合:

```sh
g++: error: unrecognized command line option ‘--std=c++23’
```

これは、より新しいバージョンの GCC (`g++`)が必要であることを意味します。

一般的なルールとして、コンパイラは新しいほど優れています。

## ツール

ほとんどのディストリビューションでは、必須のビルドツールを単一のパッケージ(Debian および派生ディストリビューションには `build-essential`があります) またはパッケージグループ (Arch `base-devel`があります)としてパッケージ化しているようです。利用可能であれば、これらを使用する必要があります。そうでない場合は、少なくとも `make` が必要であり、不足している依存関係があれば（もしあれば）それらを追って見つけ出す必要があります。

必須のツールの他に、`git`が必要です。

ナイトリービルドに追いつく予定がある場合は、 `ccache`もインストールする必要があります。これは部分ビルドを大幅に高速化します。

## 依存関係

いくつかの一般的な依存関係、オプションの依存関係、そして Curses または Tiles ビルドのいずれかに固有の依存関係があります。正確なパッケージ名は、使用しているディストリビューション、およびディストリビューションがライブラリとその開発ファイルを個別にパッケージ化しているかどうか（例: Debian および派生ディストリビューション）によって再び異なります。

Arch でのビルドに基づいたおおまかなリスト:

- 一般: `gcc-libs`, `glibc`, `zlib`, `bzip2`, `sqlite3`
- オプション: `intltool`
- Curses: `ncurses`
- Tiles: `sdl2`, `sdl2_image`, `sdl2_ttf`, `sdl2_mixer`, `freetype2`

例として、Debian および派生ディストリビューションでの Curses ビルドには、さらに`libncurses5-dev`または
`libncursesw5-dev`が必要になります。

オプションの依存関係に関する注記:

- `intltool` - ローカライゼーションファイルをビルドするためのものです。英語のみを使用する予定がある場合はスキップできます。

コンパイルエラーや、コンパイルされたバイナリに対する `ldd` の出力を読むことで、不足しているものを把握できるはずです。

## Make フラグ

ソースからビルドする場合、いくつかの選択を行う必要があります:

- `NATIVE=` - クロスコンパイルしている場合にのみ気にする必要があります。
- `RELEASE=1` -これがないとデバッグビルドになります（下記の注記を参照）。
- `LTO=1` - GCC/Clang でリンク時最適化を有効にします。
- `TILES=1` - これにより Tiles バージョンが得られ、ない場合は Curses バージョンが得られます。
- `SOUND=1` - サウンドが必要な場合。これには `TILES=1`が必要です。
- `LANGUAGES=` - ローカライゼーションを指定します。詳細については [こちら](#compiling-localization-files)を参照してください。
- `CLANG=1` - GCC の代わりに Clang を使用します。
- `CCACHE=1` - ccache を使用します。
- `USE_LIBCXX=1` - Clang で libstdc++ の代わりに libc++ を使用します（OS X のデフォルト）。
- `BACKTRACE=1` - クラッシュ時にスタックバックトレースを出力するサポート。

他にもいくつかのオプションがあります。ご自由に `Makefile`を読んでみてください。

マルチコアコンピューターをお持ちの場合は、オプションに `-jX` を追加することをお勧めします。ここで `X` は利用可能なコア数の約 2 倍にする必要があります。

例: `make -j4 CLANG=1 CCACHE=1 NATIVE=linux64 RELEASE=1 TILES=1`

上記は、Clang と ccache を使用し、4 つの並列プロセスで、64 ビット Linux 向けに明示的に Tiles リリースをビルドします。

例: `make -j2`

上記は、GCC と 2 つの並列プロセスを使用して、使用しているアーキテクチャ向けのデバッグ有効化された Curses バージョンをビルドします。

**デバッグに関する注記**: セグメンテーション違反が発生し、スタックトレースを提供する意思がある場合を除き、おそらく常に `RELEASE=1` でビルドすべきです。

## ローカライゼーションファイルのコンパイル

既定では、英語のみが利用可能であり、ローカライゼーションファイルは必要ありません。

特定の言語のファイルをコンパイルしたい場合は、make コマンドに
`LANGUAGES="<lang_id_1> [lang_id_2] [...]"` オプションを追加する必要があります:

```sh
make LANGUAGES="zh_CN zh_TW"
```

言語 ID は `lang/po`ディレクトリ内の`*.po` ファイル名から取得するか、
`LANGUAGES="all"` を使用して利用可能なすべてのローカライゼーションをコンパイルできます。

# Debian

Debian ベースのシステムでコンパイルするための手順です。ここでのパッケージ名は Ubuntu 12.10 で有効であり、お使いのシステムで動作する場合と動作しない場合があります。

以下のビルド手順は、常に Cataclysm:BN ソースディレクトリ内から実行することを前提としています。

## Linux (ネイティブ) ncurses ビルド

依存関係:

- ncurses または ncursesw (マルチバイトロケール用)
- ビルドエッセンシャル (build essentials)

インストール:

```sh
sudo apt-get install libncurses5-dev libncursesw5-dev build-essential astyle libsqlite3-dev zlib1g-dev
```

### ビルド

Run:

```sh
make
```

## Linux (ネイティブ) SDL ビルド

依存関係:

- SDL
- SDL_ttf
- freetype
- ビルドエッセンシャル
- libsdl2-mixer-dev - サウンドサポート付きでコンパイルする場合に使用されます。

インストール:

```sh
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libfreetype6-dev build-essential
```

sdl2-config --version を実行して、正しいバージョンの SDL2 がインストールされていることを確認してください。

```sh
> sdl2-config --version
2.0.22
```

古いバージョンの SDL を使用すると、
[IME が機能しない](https://github.com/cataclysmbnteam/Cataclysm-BN/issues/1497)可能性があります。

### Bビルド

簡単なインストールは、単に以下を実行することで実行できます:

```sh
make TILES=1
```

より包括的な代替案は次のとおりです:

```sh
make -j2 TILES=1 SOUND=1 RELEASE=1 USE_HOME_DIR=1
```

-j2 フラグは、2 つの並列プロセスでコンパイルすることを意味します。より最新のプロセッサでは、省略するか、 -j4 に変更できます。サウンドが必要ない場合は、これらのフラグも省略できます。
USE_HOME_DIR フラグは、構成やセーブファイルなどのユーザーファイルをホームフォルダに配置し、バックアップを容易にします。これも省略できます。

## Linux 64ビットから Linux 32ビットへのクロスコンパイル

依存関係:

- 32ビットツールチェーン
- 32ビット ncursesw (マルチバイトおよび 8ビットロケールの両方に対応)

インストール:

```sh
sudo apt-get install libc6-dev-i386 lib32stdc++-dev g++-multilib lib32ncursesw5-dev
```

### ビルド

実行:

```sh
make NATIVE=linux32
```

## Linux から Windows へのクロスコンパイル

Linux から Windows へのクロスコンパイルを行うには、[MXE](http://mxe.cc)が必要です。

これらの手順は Ubuntu 20.04 向けに書かれていますが、任意の Debian ベースの環境に適用できるはずです。パッケージマネージャの指示は、お使いの環境に合わせて調整してください。

MXE は、MXE apt リポジトリからインストールするか（はるかに高速）、ソースからコンパイルすることができます。

### バイナリディストリビューションからの MXE インストール

```sh
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9
sudo add-apt-repository "deb [arch=amd64] https://pkg.mxe.cc/repos/apt `lsb_release -sc` main"
sudo apt-get update
sudo apt-get install astyle bzip2 git make mxe-x86-64-w64-mingw32.static-{sdl2,sdl2-ttf,sdl2-image,sdl2-mixer}
```

`~/.profile` を次のように編集します:

```sh
export PLATFORM_64="/usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

これは、電源サイクル後も `make` コマンドの変数がリセットされないようにするためです。

### ソースからの MXE インストール

[MXE の要件](http://mxe.cc/#requirements) とビルド依存関係をインストールします。:

```sh
sudo apt install astyle autoconf automake autopoint bash bison bzip2 cmake flex gettext git g++ gperf intltool libffi-dev libgdk-pixbuf2.0-dev libtool libltdl-dev libssl-dev libxml-parser-perl lzip make mingw-w64 openssl p7zip-full patch perl pkg-config python ruby scons sed unzip wget xz-utils g++-multilib libc6-dev-i386 libtool-bin
```

MXE リポジトリをクローンし、CBN に必要なパッケージをビルドします:

```sh
mkdir -p ~/src
cd ~/src
git clone https://github.com/mxe/mxe.git
cd mxe
make -j$((`nproc`+0)) MXE_TARGETS='x86_64-w64-mingw32.static' sdl2 sdl2_ttf sdl2_image sdl2_mixer sqlite
```

高速なコンピューターであっても、MXE からこれらすべてのパッケージをビルドするにはしばらく時間がかかる場合があります。辛抱強く待ってください。
`-j` フラグは、プロセッサのすべてのコアを活用します。

32ビットと 64ビットの両方でビルドする予定がない場合は、MXE_TARGETS を調整することをお勧めします。

`~/.profile` を次のように編集します:

```sh
export PLATFORM_64="~/src/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

これは、電源サイクル後も`make` コマンドの変数がリセットされないようにするためです。

### ビルド (SDL)

```sh
make -j$((`nproc`+0)) CROSS="${PLATFORM_64}" TILES=1 SOUND=1 RELEASE=1 BACKTRACE=0 PCH=0 bindist
```

## Linux から Mac OS X へのクロスコンパイル

手順は Linux から Windows へのクロスコンパイルと非常に似ています。Ubuntu 14.04 LTS でテストされていますが、他のディストリビューションでも機能するはずです。

クロスコンパイルのエラーに関する歴史的な問題のため、Mac OS X ターゲットへのクロスコンパイルでは実行時最適化が無効になっていることに注意してください (コンパイルフラグとして`-O0` が指定されています) 。詳細については、
[プルリクエスト #26564](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/26564) を参照してください。

### 依存関係

- OSX クロスコンパイルツールチェーン [osxcross](https://github.com/tpoechtrager/osxcross)

- `genisoimage` および [libdmg-hfsplus](https://github.com/planetbeing/libdmg-hfsplus.git) dmg ディストリビューションを作成するため

コンパイルする前に、すべての依存ツールが検索 `PATH` に含まれていることを確認してください。

### セットアップ

コンパイル環境をセットアップするには、次のコマンドを実行します。

```
git clone https://github.com/tpoechtrager/osxcross.git #clone the toolchain
cd osxcross
cp ~/MacOSX10.11.sdk.tar.bz2 ./tarballs/ # copy prepared MacOSX SDK tarball on place.
OSX_VERSION_MIN=11 ./build.sh # build everything
```

[詳細はこちら](https://github.com/tpoechtrager/osxcross/blob/master/README.md#packaging-the-sdk)
をご覧ください。ターゲットとする OSX の最小サポートバージョンに注意してください。

`osxcross` に組み込まれている MacPorts でのコンパイルはかなり難しく、現時点ではサポートされていないため、事前にパッケージ化されたライブラリとフレームワークを所定の場所に用意してください。ディレクトリツリーは次のようになっている必要があります:

```
~/
├── Frameworks
│   ├── SDL2.framework
│   ├── SDL2_image.framework
│   ├── SDL2_mixer.framework
│   └── SDL2_ttf.framework
└── libs
    └── ncurses
        ├── include
        └── lib
```

それぞれに適切なフレームワーク、dylibs、およびヘッダーが格納されています。ncurses は libncurses.5.4.dylib のバージョンでテストされています。これらのライブラリは OS X 10.11 の `homebrew`バイナリディストリビューションから取得され、フレームワークは次の [セクション](#sdl)で説明されているように SDL 公式ウェブサイトから取得されました。

### ビルド (SDL)

フル機能の Tiles および Sound 有効バージョンをローカライゼーション有効でビルドするには:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all TILES=1 SOUND=1 FRAMEWORK=1
  OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

`x86_64-apple-darwin15-clang++` が `PATH` 環境変数に含まれていることを確認してください。

### ビルド (ncurses)

フル Curses バージョンをローカライゼーション有効でビルドするには:

```sh
make dmgdist CROSS=x86_64-apple-darwin15- NATIVE=osx USE_HOME_DIR=1 CLANG=1
  RELEASE=1 LANGUAGES=all OSXCROSS=1 LIBSDIR=../libs FRAMEWORKSDIR=../Frameworks
```

`x86_64-apple-darwin15-clang++` が `PATH` 環境変数に含まれていることを確認してください。

## Linux から Android へのクロスコンパイル

Android ビルドは [Gradle](https://gradle.org/) を使用して Java およびネイティブ C++ コードをコンパイルし、SDL の
[Android プロジェクトテンプレート](https://github.com/libsdl-org/SDL/tree/main/android-project)に大きく基づいています。詳細については、公式の SDL ドキュメント
[README-android.md](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md) を参照してください。

Gradle プロジェクトはリポジトリの `android/`の下にあります。コマンドライン経由でビルドするか、 [Android Studio](https://developer.android.com/studio/)で開くことができます。簡潔にするため、Tiles、Sound、ローカライゼーションを含むすべての機能が有効になった SDL バージョンのみをビルドします。

### 依存関係

- Java JDK 11
- SDL2 (2.0.8でテスト済みですが、プロジェクト固有のバグ修正が施されたカスタムフォークが推奨されます)
- SDL2_ttf (2.0.14でテスト済み)
- SDL2_mixer (2.0.2でテスト済み)
- SDL2_image (2.0.3でテスト済み)

Gradle ビルドプロセスは、
[deps.zip](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/android/app/deps.zip)から依存関係を自動的にインストールします。

### セットアップ

Linux の依存関係をインストールします。デスクトップ Ubuntu インストールの場合:

```sh
sudo apt-get install openjdk-8-jdk-headless
```

Android SDK および NDK をインストールします:

```sh
wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip
unzip sdk-tools-linux-4333796.zip -d ~/android-sdk
rm sdk-tools-linux-4333796.zip
~/android-sdk/tools/bin/sdkmanager --update
~/android-sdk/tools/bin/sdkmanager "tools" "platform-tools" "ndk-bundle"
~/android-sdk/tools/bin/sdkmanager --licenses
```

Android 環境変数をエクスポートします (これらを `~/.bashrc`の末尾に追加できます):

```sh
export ANDROID_SDK_ROOT=~/android-sdk
export ANDROID_HOME=~/android-sdk
export ANDROID_NDK_ROOT=~/android-sdk/ndk-bundle
export PATH=$PATH:$ANDROID_SDK_ROOT/platform-tools
export PATH=$PATH:$ANDROID_SDK_ROOT/tools
export PATH=$PATH:$ANDROID_NDK_ROOT
```

サブシーケントなビルドを高速化するために `ccache` を使用したい場合は、次の追加変数を使用できます:

```sh
export USE_CCACHE=1
export NDK_CCACHE=/usr/local/bin/ccache
```

**注記:** `ccache` へのパスは、お使いのシステムによって異なる場合があります。

### Android デバイスのセットアップ

E
Android デバイスで[開発者向けオプションを有効にします](https://developer.android.com/studio/debug/dev-options)。USB ケーブルを介してデバイスを PC に接続し、実行します。

```sh
adb devices
adb connect <devicename>
```

### ビルド

APK をビルドするには、Gradle ラッパーコマンドラインツール (gradlew)を使用します。Android Studio のドキュメントには、
[コマンドラインからアプリをビルドする方法](https://developer.android.com/studio/build/building-cmdline)の概要が詳しく記載されています。

デバッグ APK をビルドするには、リポジトリの `android/` サブフォルダから実行します:

```sh
./gradlew assembleDebug
```

これにより、`./android/app/build/outputs/apk/` にデバッグ APK が作成され、デバイスにインストールする準備が整います。

デバッグ APK をビルドし、経由で接続されているデバイスにすぐにデプロイするには、実行します。

```sh
./gradlew installDebug
```

署名付きリリース APK（つまり、デバイスにインストールできる APK）をビルドするには、
[署名されていないリリースAPKをビルドし、手動で署名します](https://developer.android.com/studio/publish/app-signing#signing-manually)。

### Github フォークでのナイトリービルドのトリガー

独自の Github フォークで Android APK のナイトリービルドを正常にビルドするには、ダミーの Android 署名キーのセットを初期化する必要があります。これは、Github Actions ワークフローが APK に署名するためのキーのセットを必要とするためです。

1. 6 文字を超えるパスワードを作成します。それを覚えておき、
   `KEYSTORE_PASSWORD`として Github シークレットに保存します。
2. `keytool -genkey -v -keystore release.keystore -keyalg RSA -keysize 2048 -validity 10000 -alias dummy-key`でキーを作成します。パスワードを尋ねられたら、上記のパスワードを使用します。
3. 次の内容の`keystore.properties.asc` というファイルを作成します:

```text
storeFile=release.keystore
storePassword=<INSERT PASSWORD FROM STEP 1>
keyAlias=dummy-key
keyPassword=<INSERT PASSWORD FROM STEP 1>
```

4. Eステップ 1 のパスワードを使用して
   `gpg --symmetric --cipher-algo AES256 --armor release.keystore`で`release.keystore` を暗号化します。結果を
   `KEYSTORE`として Github シークレットに保存します。
5. ステップ 1 のパスワードを使用して
   `gpg --symmetric --cipher-algo AES256 --armor keystore.properties`で`keystore.properties` を暗号化します。結果を `KEYSTORE_PROPERTIES`として Github シークレットに保存します。

### 追加の注記

アプリは、デバイス上の
`/sdcard/Android/data/com.cleverraven/cataclysmdda/files`にデータファイルを保存します。データはデスクトップバージョンと下位互換性があります。

## 開発者向けの高度な情報

ほとんどの人にとって、単純な Homebrew インストールで十分です。開発者向けに、Mac OS X で Cataclysm をビルドするためのより技術的な詳細をいくつか紹介します。

### SDL

Tiles ビルドには、SDL2、SDL2_image、および SDL2_ttf が必要です。オプションで、サウンドサポートのために SDL2_mixer を追加できます。Cataclysm は、SDL フレームワークまたはソースからビルドされた共有ライブラリのいずれかを使用してビルドできます。

SDL フレームワークファイルは、ここからダウンロードできます:

- [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.3)
- [SDL2_image](https://github.com/libsdl-org/SDL_image)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf)

`SDL2.framework`、`SDL2_image.framework`、 および `SDL2_ttf.framework` を`/Library/Frameworks` または
`/Users/name/Library/Frameworks`にコピーします。

サウンドサポートが必要な場合は、追加の SDL フレームワークが必要です:

- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) (オプション、サウンドサポート用)

`SDL2_mixer.framework` を `/Library/Frameworks` または `/Users/name/Library/Frameworks`にコピーします。

あるいは、SDL 共有ライブラリはパッケージマネージャを使用してインストールできます:

Homebrew の場合:

```sh
brew install sdl2 sdl2_image sdl2_ttf
```

サウンド付き:

```sh
brew install sdl2_mixer libvorbis libogg
```

MacPorts の場合:

```sh
sudo port install libsdl2 libsdl2_image libsdl2_ttf
```

サウンド付き:

```sh
sudo port install libsdl2_mixer libvorbis libogg
```

### ncurses

Cataclysm は Unicode 文字を広範に使用するため、ワイド文字サポートが有効になっている ncurses が必要です。

Homebrew の場合:

```sh
brew install ncurses
```

MacPorts の場合:

```sh
sudo port install ncurses
hash -r
```

### gcc

[Xcode用コマンドラインツール](https://developer.apple.com/downloads/) にインストールされている gcc/g++ のバージョンは、実際には clang と同じ Apple LLVM のフロントエンドにすぎません。これにより問題が発生するとは限りませんが、このバージョンの gcc/g++ は clang のエラーメッセージを表示し、本質的に clang を使用した場合と同じ結果を生成します。「本物の」gcc/g++ でコンパイルするには、homebrew でインストールします:

```sh
brew install gcc
```

ただし、homebrew は競合を避けるために gcc を gcc-{version} ({version} はバージョン) としてインストールします。 `/usr/bin/gcc`にある Apple LLVM バージョンの代わりに、`/usr/local/bin/gcc-{version}` にある homebrew バージョンを使用する最も簡単な方法は、必要なものをシンボリックリンクすることです。

```sh
cd /usr/local/bin
ln -s gcc-12 gcc
ln -s g++-12 g++
ln -s c++-12 c++
```

または、`/usr/local/bin/` にある`-12`で終わるすべてのものに対してこれを行うには:

```sh
find /usr/local/bin -name "*-12" -exec sh -c 'ln -s "$1" $(echo "$1" | sed "s/..$//")' _ {} \;
```

また、 `$PATH`で`/usr/local/bin` が `/usr/bin` の前に出現するように確認する必要があります。そうしないと、これは機能しません。

`gcc -v` がインストールした homebrew バージョンを表示していることを確認してください。

### brew clang

Apple clang の代わりに通常の clang を使用したい場合は、Homebrew でインストールできます:

```sh
brew install llvm
```

その後、makeコマンドで `COMPILER=$(brew --prefix llvm)/bin/clang++` を使用してコンパイラを指定できます。

インストールされているコンパイラが目的のものであることを確認するのは常に良いことです。

```sh
$(brew --prefix llvm)/bin/clang++ --version
```

### コンパイル

Cataclysm のソースは `make` を使用してコンパイルされます。

### Make オプション

- `NATIVE=osx` OS X 向けにビルドします。すべての Mac ビルドに必要です。
- `OSX_MIN=version` `-mmacosx-version-min=`を設定します。デフォルトは 11 です。
- `TILES=1` グラフィックタイル付きの SDL バージョンをビルドします（およびグラフィック ASCII）。省略すると
  `ncurses`でビルドされます。
- `SOUND=1` - サウンドが必要な場合。これには `TILES=1` と上記で言及された追加の依存関係が必要です。
- `FRAMEWORK=1` (tiles のみ) OS X の Frameworks フォルダにある SDL ライブラリにリンクします。省略すると、Homebrew または Macports の SDL 共有ライブラリが使用されます。
- `LANGUAGES="<lang_id_1>[lang_id_2][...]"` 指定された言語のローカライゼーションファイルをコンパイルします。例
  `LANGUAGES="zh_CN zh_TW"`。`LANGUAGES=all` を使用してすべてのローカライゼーションファイルをコンパイルすることもできます。
- `RELEASE=1` 最適化されたリリースバージョンをビルドします。省略するとデバッグビルドになります。
- `CLANG=1` 最新の Xcode 用コマンドラインツールに含まれているコンパイラである [Clang](http://clang.llvm.org/)でビルドします。省略すると gcc/g++ を使用してビルドされます。
- `MACPORTS=1` Macports 経由でインストールされた依存関係（現時点では `ncurses`のみ）に対してビルドします。
- `USE_HOME_DIR=1`ユーザーファイル（設定、セーブファイル、墓地など）をユーザーのホームディレクトリに配置します。Curses ビルドの場合、これは `/Users/<user>/.cataclysm-bn`であり、SDL ビルドの場合、これは
  `/Users/<user>/Library/Application Support/Cataclysm`です。
- `DEBUG_SYMBOLS=1` 最適化されたリリースバイナリをビルドする際にデバッグシンボルを保持し、開発者がクラッシュ箇所を簡単に見つけられるようにします。

上記のオプションに加えて、Tiles ビルドを `Cataclysm.app`にパッケージ化する`app` make ターゲットがあります。これは、ターミナルなしで実行できる Mac アプリケーション内の完全な Tiles ビルドです。

詳細については、`Makefile`のコメントを参照してください。

### Make の例

Clang を使用してリリース SDL バージョンをビルドする:

```sh
make NATIVE=osx RELEASE=1 TILES=1 CLANG=1
```

BClang を使用してリリース SDL バージョンをビルドし、OS X Frameworks フォルダ内のライブラリにリンクし、すべての言語ファイルをビルドし、`Cataclysm.app`にパッケージ化する:

```sh
make app NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 LANGUAGES=all CLANG=1
```

Macports によって提供される Curses を使用してリリース Curses バージョンをビルドする:

```sh
make NATIVE=osx RELEASE=1 MACPORTS=1 CLANG=1
```

### 実行

Curses ビルドの場合:

```sh
./cataclysm
```

SDL の場合:

```sh
./cataclysm-bn-tiles
```

`app` ビルドの場合、Finder から `Cataclysm.app` を起動します。

### テストスイート

ビルドでは、tests/cata_testにテスト実行可能ファイルも生成されます。他の実行可能ファイルと同じように呼び出すと、完全なテストスイートが実行されます。オプションのリストを表示するには、`--help` フラグを渡します。

### dmg ディストリビューション

`dmgdist` ターゲットを使用して、適切なdmgディストリビューションファイルをビルドできます。これには、
[dmgbuild](https://pypi.python.org/pypi/dmgbuild)と呼ばれるツールが必要です。このツールをインストールするには、まず Python が必要です。Mac OS X >= 10.8 の場合、Python 2.7 は OS にプリインストールされています。それより古いバージョンの OS X を使用している場合は、
[公式ウェブサイト](https://www.python.org/downloads/) で Python をダウンロードするか、homebrew
`brew install python`.でインストールできます。Python をインストールしたら、以下を実行して `dmgbuild` をインストールできるはずです。

```sh
# これは pip をインストールします。すでにインストールされている場合は必要ないかもしれません。
curl --silent --show-error --retry 5 https://bootstrap.pypa.io/get-pip.py | sudo python
# dmgbuild をインストール
sudo pip install dmgbuild pyobjc-framework-Quartz
```

`dmgbuild` がインストールされたら、次のように `dmgdist` ターゲットを使用できます。
`USE_HOME_DIR=1` の使用は、ユーザー構成とそのセーブファイルをホームディレクトリに保持しながら、ゲームの簡単なアップグレードを可能にするため、ここで重要です。

    make dmgdist NATIVE=osx RELEASE=1 TILES=1 FRAMEWORK=1 CLANG=1 USE_HOME_DIR=1

`Cataclysm.dmg` ファイルが表示されるはずです。

## Mac OS X トラブルシューティング

### ISSUE: 色が正しく表示されない

ターミナルの設定を開き、`Use bright colors for bold text` で
`Preferences -> Settings -> Text`をオンにします。

# Windows

Windows で Visual Studio および類似の IDE を使用することに慣れている人にとっては、おそらく最も簡単なソリューションです。

Windows で Visual Studio を使用してビルド環境をセットアップおよび使用する方法については、 [COMPILING-VS-VCPKG.md](./vs_vcpkg.md) を参照してください。

## MSYS2 でのビルド

Windows で MSYS2 を使用してビルド環境をセットアップおよび使用する方法については、[COMPILING-MSYS.md](./msys.md)を参照してください。

MSYS2 は、ネイティブ Windows アプリケーションと UNIX ライクな環境のバランスを取っています。私たちのプロジェクトが使用するコマンドラインツール（特に JSON リンター）の一部は、MSYS2 や CYGWIN が提供するようなコマンドライン環境なしでは使用するのがより困難です。

## CYGWIN でのビルド

Windows で CYGWIN を使用してビルド環境をセットアップおよび使用する方法については、[COMPILING-CYGWIN.md](./cygwin.md) を参照してください。

CYGWIN は、POSIX 環境をより完全にエミュレートしようとし、MSYS2 よりも「より Unix 的」であろうとします。いくつかの点で少し時代遅れであり、MSYS2 パッケージマネージャの利便性が欠けています。

## Clang と MinGW64 でのビルド

Clang は Windows 上でデフォルトで MSVC を使用しますが、MinGW64 ライブラリもサポートしています。バッチスクリプトで
`CLANG=1` を `"CLANG=clang++ -target x86_64-pc-windows-gnu -pthread"` に置き換え、MinGW64 がパスに含まれていることを確認するだけです。また、ユニットテストをコンパイルするには、MinGW64 の `float.h` に
[パッチ](https://sourceforge.net/p/mingw-w64/mailman/message/36386405/) の適用を要する場合があります。

# BSDs

最近の OpenBSD および FreeBSD マシン（適切な最新のコンパイラを使用）で CBN が正常にビルドされたという報告があり、 `Makefile` が「ただ動作する」ようにするための作業も行われています。しかし、それに到達するには程遠く、BSDs のサポートは主にユーザーの貢献に基づいています。結果は異なる場合があります。これまでのところ、基本的にすべてのテストは amd64 で行われていますが、原理的には他のアーキテクチャで動作しない（既知の）理由はありません。

### システムコンパイラを使用した FreeBSD/amd64 10.1 でのビルド

FreeBSD は 10.0 以降、clang を既定のコンパイラとして使用し、libc++ と組み合わせて C++17 サポートを標準で提供します。ただし、gmake が必要になります（バイナリパッケージの例）。

`pkg install gmake`

Tiles ビルドには、SDL2 も必要になります:

`pkg install sdl2 sdl2_image sdl2_mixer sdl2_ttf`

その後、次のようなものでビルドできるはずです（もちろん、CXXFLAGS と LDFLAGS は .profile などで設定できます）。

```sh
export CXXFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"
gmake # ncurses builds
gmake TILES=1 # tiles builds
```

ビルド VM に X がないため、筆者は Tiles ビルドをテストしていません。少なくともコンパイル/リンクは正常に成功します。

### ports からの GCC 4.8.4 を使用した FreeBSD/amd64 9.3 での ncurses バージョンのビルド

ncurses ビルドの場合、`VERSION` の前に `Makefile` に追加します。

```make
OTHERS += -D_GLIBCXX_USE_C99
CXX = g++48
CXXFLAGS += -I/usr/local/lib/gcc48/include
LDFLAGS += -rpath=/usr/local/lib/gcc48
```

注記: または、上記を `setenv` することもできます (`OTHERS` を `CXXFLAGS`にマージします)が、それはご存知のとおりです。
そして、 `gmake RELEASE=1`でビルドします。

### ports/packages からの GCC 4.9.2 を使用した OpenBSD/amd64 5.8 でのビルド

まず、パッケージから g++、gmake、および libexecinfo をインストールします（g++ 4.8 または 4.9 が機能するはずです。4.9 がテストされています）。

`pkg_add g++ gmake libexecinfo`

その後、次のようなものでビルドできるはずです。

`CXX=eg++ gmake`

SDL2 が壊れているため、5.8-release では ncurses ビルドのみが可能です。ただし、最近の -current またはスナップショットでは、SDL2 パッケージをインストールできます。

`pkg_add sdl2 sdl2-image sdl2-mixer sdl2-ttf`

そして、次のようにビルドします。

`CXX=eg++ gmake TILES=1`

### システムコンパイラを使用した NetBSD/amd64 7.0RC1 でのビルド

NetBSD はバージョン 7.0 の時点で gcc 4.8.4 を搭載しています（または搭載する予定です）。これは Cataclysm をビルドするのに十分な新しさです。gmake と ncursesw をインストールする必要があります。

`pkgin install gmake ncursesw`

その後、次のようなものでビルドできるはずです（ncurses ビルドの LDFLAGS は ncurses 構成スクリプトによって処理されます。もちろん、CXXFLAGS/LDFLAGS は .profile などで設定できます）。

```sh
export CXXFLAGS="-I/usr/pkg/include"
gmake # ncurses builds
LDFLAGS="-L/usr/pkg/lib" gmake TILES=1 # tiles builds
```

SSDL ビルドは現在コンパイルされますが、私のテストでは実行されませんでした。セグメンテーション違反が発生するだけでなく、デバッグシンボルを読み取るときに gdb もセグメンテーション違反を起こします！おそらく結果は異なるでしょう。
