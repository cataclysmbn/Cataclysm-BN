---
title: CMake builds on Atomic Distros
---

この文書では、ベースインストールに依存関係をレイヤー化するのではなく、コンテナ化が推奨されるソリューションとなるイミュータブルなディストリビューション上で、BNをビルドする手順を説明します。

# 例: Fedora Atomicベース (Bazzite)

> [!CAUTION]
>
> 執筆時点では、Bazziteのデフォルトコンテナイメージは fedora-toolbox:38ですが、これにより依存関係のインストールスクリプトを編集する必要が生じる可能性があります。より新しいバージョンのFedoraをイメージとして取得するディストリビューション（または手動で取得した場合）では、標準のFedoraスクリプトをそのまま使用できる確実性が高まります。

> [!CAUTION]
>
> [distrobox](https://distrobox.it)を使用する場合、
> [exported](https://github.com/89luca89/distrobox/blob/main/docs/usage/distrobox-export.md) コンパイラ
> (例:`~/.local/bin/clang`)の使用は、
> [`/usr`にアクセスできないため動作しません](https://github.com/89luca89/distrobox/issues/1548)代わりに、
> `/usr/bin/clang`のようなコンパイラの絶対パスを使用してください。

## コンテナのセットアップ

Fedora LinuxのAtomicバージョンに基づいているBazziteには、Toolbxとして知られるコンテナ化ツールがプレインストールされています。したがって、本例ではこの方法を使用します。

まず、次のコマンドでToolboxを作成します:

```sh
$ toolbox create
```

これにより、コンテナのベースとなるイメージをダウンロードするように促される可能性があります。ここで 「fedora-toolbox:38」（または他の番号）のダウンロードについて尋ねられたり、Fedora Workstationのダウンロードについて尋ねられたりした場合は、正しい手順に進んでいます。「yes」（はい）と答えて、コンテナが構築されるのを待ちます。これはハードウェアに応じて時間がかかる場合がありますが、幸いにも一度だけ実行すればよい手順です。

Toolboxを作成した後、 **`src` フォルダがある** Cataclysm-BNフォルダ（つまり、GitHubリポジトリのローカルコピーまたはダウンロードしたソースコード）に移動します。そこで、簡単なコマンドでToolbox内に入ることができます:

```sh
$ toolbox enter
```

ビルドを意図するたびに、このコマンドを実行してコンテナに入る必要があります。

Toolbox内に入ったら、Fedoraベースのディストリビューションインストールスクリプトを使用して依存関係をインストールできます。Bazziteの場合、そのスクリプトは次のようになります:

```sh
$ sudo dnf install git cmake clang ninja-build mold ccache \
  SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
  freetype glibc bzip2 zlib-ng libvorbis ncurses gettext flac-devel \
  sqlite-devel zlib-devel
```

この後、あなたのコンテナは将来の全てのビルドのニーズに合わせてセットアップされます！次に、実際のビルド手順に進みます。

## ビルド

Cataclysm-BNフォルダ内にいて、まだコンテナ内に入っていない場合は、フォルダに移動してコンテナに入ります。そこから、cmakeスクリプトを実行してファイルを生成できます（buildフォルダを自分で作成する必要はありません。cmakeが存在しない場合は自動的に生成します）。Bazziteの場合、これは次のようになります。:

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

全てが順調に進んだとすれば、CMakeファイルが生成されているはずです！あとは、実際にビルドを実行するためにNinjaコマンドを実行するだけです。

```sh
$ ninja -C build -j $(nproc) -k 0 cataclysm-bn-tiles
```

`$(nproc)`は単にCPUが持つ「スレッド」の数を取得しますが、コンパイルに使用するスレッド数を減らしたい場合は、より少ない数を指定することも可能です（ただし、その分パフォーマンスは低下します）。
