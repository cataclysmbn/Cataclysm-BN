---
title: Cygwin
---

> [!CAUTION]
>
> Cygwin ビルドは非常に長い間テストされておらず、この記事は主に記録のために残されています。

このガイドには、Windows 環境で Cygwin を使用して『Cataclysm-BN』をコンパイルする手順が含まれています。

> [!NOTE]
>
> これらの手順は、再配布可能な CBN のコピーを作成することを意図していません。 再配布が目的の場合は、公式サイトから公式ビルドをダウンロードするか、
> [LinuxからWindows向けにクロスコンパイル](./makefile.md#cross-compile-to-windows-from-linux) してください。

これらの手順は、64ビット版の Windows 7 と 64ビット版の Cygwin を使用して作成されましたが、他のバージョンの Windows でも手順は同じはずです。

環境設定と生成されたバイナリの実行が遅いため、MSYS2 を使用したコンパイルが推奨されます。

## 前提条件:

- 64ビット版の Windows 7、8、8.1、または 10
- 約 10 GB の空き容量がある NTFS パーティション (Cygwin インストールに約 2 GB、リポジトリに約 3 GB、ccache に約 5 GB)
- 64ビット版の Cygwin

## インストール:

1. [Cygwin のホームページ](https://cygwin.com/) にアクセスし、64ビット版のインストーラー (例
   [setup-x86_64.exe](https://cygwin.com/setup-x86_64.exe))をダウンロードします。

2. ダウンロードしたファイルを実行し、Cygwin をインストールします。`Install from Internet` を選択し、任意のインストールディレクトリを選択します。開発専用のディレクトリ (例:
   `C:\dev\cygwin64`)にインストールすることが推奨されますが、厳密には必須ではありません。すべてのユーザー向けにインストールしてください。

3. ローカルパッケージディレクトリとして `\downloads\` フォルダを指定します(例: `C:\dev\cygwin64\downloads`)。

4. 特に理由がない限り、システムプロキシ設定を使用してください。

5. 次の画面で、好きなミラーを選択します。

6. 検索ボックスに`wget` と入力し、「Web」カテゴリを展開して、ドロップダウンから最新バージョンを選択します（このガイド作成時点では 1.21.1-1）。

7. 画面を一番下までスクロールし、`wget` が表示されていることを確認します。これが、`Cygwin` をダウンロードおよびインストールするパッケージの完全なリストです。

8. エラーが発生したパッケージがあれば、再試行します。

9. スタートメニューやデスクトップにショートカットを追加するチェックボックスをオンにし、`Finish` ボタンを押します。

## 設定:

1. デスクトップから Cygwin64 ターミナルを起動します。

2. より簡単なパッケージインストールのために `apt-cyg` をインストールします:

```bash
wget https://raw.githubusercontent.com/transcode-open/apt-cyg/master/apt-cyg -O /bin/apt-cyg
chmod 755 /bin/apt-cyg
```

3. コンパイルに必要なパッケージをインストールします:

```bash
apt-cyg install astyle ccache gcc-g++ intltool git libSDL2_image-devel libSDL2_mixer-devel libSDL2_ttf-devel make xinit
```

一部のパッケージが既にインストールされているというメッセージや、Cygwin が要求していないパッケージをインストールしているというメッセージが表示されますが、これは Cygwin のパッケージマネージャが依存関係を自動的に解決した結果です。

## クローンとコンパイル:

1. 次のコマンドで Cataclysm-BN リポジトリをクローンします。:

**注記:** これにより、CBN リポジトリとそのすべての履歴（3GB）全体がダウンロードされます。単にテストしているだけの場合は、おそらく `--depth=1` (約350MB)を追加すべきです。

```bash
cd /cygdrive/c/dev
git clone https://github.com/cataclysmbnteam/Cataclysm-BN.git
cd Cataclysm-BN
```

2. コンパイルします。

```bash
make -j$((`nproc`+0)) CCACHE=1 RELEASE=1 CYGWIN=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 BACKTRACE=0 RUNTESTS=0
```

文字定数が終了していないという警告が表示されますが、この作成者が知る限り、コンパイルには影響しません。

**注記**: これは、Sound および Tiles サポートとすべてのローカライゼーション言語を含むリリースバージョンをコンパイルし、チェックとテストをスキップし、ccache を使用して高速ビルドを行います。他のスイッチを使用することもできますが、問題を発生させずにコンパイルするには `CYGWIN=1`、`DYNAMIC_LINKING=1` および `BACKTRACE=0` が必須です。

## 実行:

1. スタートメニューから XWin Server を実行します。

2. システムトレイにアイコンが表示されたら、黒い C のようなアイコン（X Applications Menu）を右クリックします。

3. 「System Tools」をポイントし、「UXTerm」をクリックします。

```bash
cd /cygdrive/c/dev/Cataclysm-BN
./cataclysm-bn-tiles
```

Cygwin でコンパイルされた CBN を UXTerm の外部から実行する機能はありません。
