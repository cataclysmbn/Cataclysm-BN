---
title: MSYS2
---

> [!CAUTION]
>
> この MSYS2 の手順は長期間更新されていません。MSYS2を用いたビルドには、代わりに [CMake](./cmake#windows-environment-msys2) の指示を参照することが推奨されます。

このガイドには、64ビット Windows 環境下の MSYS2 で Cataclysm-BN (CBN) をコンパイル（ビルド）するための手順が含まれています。

> [!WARNING]
>
> 注意: これらの手順は、CBNの再頒布可能なコピーを作成することを意図していません。再頒布が目的の場合は、公式サイトから公式ビルドをダウンロードするか、
> [LinuxからWindows向けにクロスコンパイル](./makefile#cross-compile-to-windows-from-linux) してください。

これらの手順は64ビット Windows 7および64ビット版 MSYS2を使用して書かれていますが、他のバージョンの Windows でも手順は同様であるはずです。

## 前提条件:

- Windows 7、8、8.1、または 10
- NTFSパーティションに約10GB の空き容量 (MSYS2インストールに約2GB、リポジトリに約3GB、ccacheに約5GB)
- 64ビット版の MSYS2

**注釈:** Windows XP はサポートされていません。

## インストール:

1. [MSYS2 ホームページ](http://www.msys2.org/)にアクセスし、インストーラーをダウンロードします。

2. インストーラーを実行します。開発専用のフォルダ (C:\dev\msys64\など)にインストールすることを推奨しますが、必須ではありません。

3. インストール後、MSYS2 64bit を起動します。

## 環境設定:

1. パッケージデータベースとコアシステムパッケージを更新します。

```bash
pacman -Syyu
```

2. MSYS2は、cygheap base mismatch の情報や、フォークされたプロセスが予期せず死亡したといったエラーを通知する場合があります。これらのエラーは `pacman`のアップグレードの性質に起因するものと思われ、安全に無視できる可能性があります。ターミナルウィンドウを閉じるように促されますので、閉じた後、MSYS2 MinGW 64-bit メニュー項目を使用して再起動します。

3. 残りのパッケージを更新します。

```bash
pacman -Su
```

4. コンパイルに必要なパッケージをインストールします。

```bash
pacman -S git make mingw-w64-x86_64-{astyle,ccache,gcc,libmad,libwebp,pkg-config,SDL2} mingw-w64-x86_64-SDL2_{image,mixer,ttf}
```

5. MSYS2を閉じます。

6. システム全体の設定ファイル (例:`C:\dev\msys64\etc\profile`) のパス変数を以下のように更新します。

- 以下の行を見つけます。

```
MSYS2_PATH="/usr/local/bin:/usr/bin:/bin"
MANPATH='/usr/local/man:/usr/share/man:/usr/man:/share/man'
INFOPATH='/usr/local/info:/usr/share/info:/usr/info:/share/info'
```

および

```
PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig:/lib/pkgconfig"
```

- そして、それらを以下に置き換えます:

```
MSYS2_PATH="/usr/local/bin:/usr/bin:/bin:/mingw64/bin"
MANPATH='/usr/local/man:/usr/share/man:/usr/man:/share/man:/mingw64/share/man'
INFOPATH='/usr/local/info:/usr/share/info:/usr/info:/share/info:/mingw64/share/man'
```

および

```
PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig:/lib/pkgconfig:/mingw64/lib/pkgconfig:/mingw64/share/pkgconfig"
```

## クローンとコンパイル:

1. MSYS2を起動し、Cataclysm-BNリポジトリをクローンします。

```bash
cd /c/dev/
git clone https://github.com/cataclysmbnteam/Cataclysm-BN.git
cd Cataclysm-BN
```

**注釈:** これにより、CBNリポジトリ全体とその全履歴 (3GB)がダウンロードされます。テスト目的のみの場合は、 `--depth=1` (~350MB)を追加することを推奨します。

2. 以下のコマンドラインでコンパイルします。

```bash
make -j$((`nproc`+0)) CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 RUNTESTS=0
```

「終了していない文字列型の定数です」 (unterminated character constants) に関する警告が表示されますが、この文書の筆者が知る限り、コンパイルには影響しません。

**注釈**: これにより、サウンドとタイル（グラフィック）サポート、および全てのローカライゼーション言語を含むリリースバージョンが、チェックとテストをスキップし、ビルド高速化のために ccache を使用してコンパイルされます。他のスイッチも使用できますが、`MSYS2=1` と `DYNAMIC_LINKING=1` は問題なくコンパイルするために必須です。

## 実行:

1. MSYS2内で、Cataclysmのディレクトリから以下のコマンドで実行します。

```bash
./cataclysm-bn-tiles
```

**注釈:** コンパイルされた実行可能ファイルをMSYS2の外部で実行したい場合は、MSYS2のランタイムバイナリへのパス (例:`C:\dev\msys64\mingw64\bin`) を、ユーザーまたはシステムの `PATH` 変数に追加する必要があります。
