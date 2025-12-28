# 開発者向けツール

## コードスタイル (astyle)

ソースコードの自動フォーマットは、
[Artistic Style](http://astyle.sourceforge.net/)（略して astyle）によって実行されます。

システムまたは個人の好みに応じて、コードベースでこれを呼び出す方法は複数あります。

### astyle を直接呼び出す

`astyle` のみがインストールされている場合は、以下を使用します。

```sh
astyle --options=.astylerc --recursive src/*.cpp,*.h tests/*.cpp,*.h tools/*.cpp,*.h
```

### make を介して astyle を呼び出す

`make` と `astyle` の両方がインストールされている場合は、以下を使用します。

```sh
make astyle
```

### pre-commit hook を介して astyle を呼び出す

関連するすべてのツールがインストールされている場合は、これらのコマンドを Git の pre-commit フック（通常は `.git/hooks/pre-commit`）に追加することで、Git にコードと JSON のスタイルのチェックを自動的に実行させることができます。

```sh
git diff --cached --name-only -z HEAD | grep -z 'data/.*\.json' | \
    xargs -r -0 -L 1 ./tools/format/json_formatter.[ce]* || exit 1

make astyle-check || exit 1
```

### Visual Studio 向け Astyle 拡張機能

Visual Studio Marketplace に astyle 拡張機能はありますが、VS2019 または VS2022 で私たちの目的に対して正しく機能することが確認されているものは（まだ）ありません。

#### Visual Studio 2022

<https://github.com/olanti-p/BN_Astyle> にアクセスし、
[README.md](https://github.com/olanti-p/BN_Astyle/blob/master/README.md)の手順に従ってください。ソースから拡張機能をコンパイルおよびインストールすることも、
[リリースセクション](https://github.com/olanti-p/BN_Astyle/releases)にあるプリビルドバージョンを利用することもできます。

#### Visual Studio 2019

拡張機能のソースコードは https://github.com/lukamicoder/astyle-extensionにあります。これをインストールおよびコンパイルするには：

1. Visual Studio インストーラーを介して、`Visual Studio 拡張機能開発`ワークロードを VS2019 に追加します。
2. ソースコードをダウンロードして抽出するか、リポジトリをクローンします (単純に
   `git clone --depth 1 https://github.com/lukamicoder/astyle-extension.git` で十分です)。
3. ルートフォルダーから `astyle-extension/AStyleExtension2017.sln` を開きます。
4. `Release` ビルド構成を選択します (VS はデフォルトで `Debug` 構成を選択する可能性が高いです)。
5. ソリューションをビルドします。
6. ビルドが成功すると、コンパイルされた拡張機能が `AStyleExtension\bin\Release`に表示されます。
   ダブルクリックしてインストールします。
7. [設定手順 (Visual Studio 2019 以前)](#configuration-instructions-visual-studio-2019-or-older)
   セクションに従って拡張機能を設定します。

#### Visual Studio 2017 以前

VS2019 の手順に従ってソースからコンパイルすることもできますが、Visual Studio Marketplace でプリビルドバージョンが[利用可能](https://marketplace.visualstudio.com/items?itemName=Lukamicoder.AStyleExtension2017) です。VS の拡張機能マネージャーを介して拡張機能をインストールし、同じ方法で設定できるはずです。

#### 設定手順 (Visual Studio 2019 以前):

1. `ツール` - `オプション` - `AStyle Formatter` - `全般`.

2. `エクスポート/インポート` タブで `インポート` ボタンを使用して、
   `https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/msvc-full-features/AStyleExtension-Cataclysm-BN.cfg`をインポートします。

![image](./img/VS_Astyle_Step_1.png)

3. インポートが成功すると、`C/C++` タブでインポートされたルールを確認できます。

![image](./img/VS_Astyle_Step_2.png)

4. `オプション` メニューを閉じ、astyle 処理をしたいファイルを開き、`編集` - `アドバンス` メニューから
   `Format Document (Astyle)` または`Format Selection (Astyle)` コマンドを使用します。

![image](./img/VS_Astyle_Step_3.png)

注釈: `ツール` - `オプション` - `環境` - `キーボード` メニューで、上記のコマンドのキーバインドを設定
することもできます。

![image](./img/VS_Astyle_Step_4.png)

## JSON スタイル

[JSON スタイルガイド](../../mod/json/explanation/json_style)を参照してください。

## ctags

[`ctags`](http://ctags.sourceforge.net/)などによる`tags` ファイル作成の通常の方法に加えて、CDDA の JSON データから取得した定義の位置で `tags` ファイルを拡張するために `tools/json_tools/cddatags.py`を提供しています。
`cddatags.py` は、ソースコードタグを含むタグファイルを安全に更新するように設計されているため、
両方のタイプのタグを `tags` ファイルに含めたい場合は、
`ctags -R . && tools/json_tools/cddatags.py`を実行できます。あるいは、これを実行するためのルールが `Makefile` にあるため、`make ctags` または `make etags`を実行するだけでも構いません。

## clang-tidy

Cataclysm には
[clang-tidy 設定ファイル](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/.clang-tidy)
があり、`clang-tidy` が利用可能であれば、コードベースの静的解析を実行するためにそれを実行できます。CI では LLVM 18 の `clang-tidy` でテストを行っているため、最も一貫した結果を得るには、そのバージョンを使用することをお勧めします。

これを実行するには、いくつかのオプションがあります。

- `clang-tidy` にはラッパースクリプト `run-clang-tidy.py` が同梱されています。

- CMake の組み込みサポートを使用します。 `-DCMAKE_CXX_CLANG_TIDY=clang-tidy` または類似のフラグを追加し、選択した `clang-tidy` のバージョンを指すようにします。

- `clang-tidy` を直接実行するには、次のようなものを試してください。

```sh
grep '"file": "' build/compile_commands.json | \
    sed "s+.*$PWD/++;s+\"$++" | \
    egrep '.' | \
    xargs -P 9 -n 1 clang-tidy -quiet
```

ファイルのサブセットに焦点を当てるには、コマンドラインの中央にある `egrep` 正規表現にファイル名を追加します。

## カスタム clang-tidy プラグイン

独自の clang-tidy チェックをカスタムプラグインとして記述しました。ubuntu 24.04 でプラグインをビルドするための正確な手順については、
[clang-tidy.yml](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/.github/workflows/clang-tidy.yml)を参照してください。

### Ubuntu 24.04 でのプラグインのビルド

[cmake guide](../../dev/guides/building/cmake.md) の正確な手順に従い、以下の変更を加えます。

さらに、以下の依存関係をインストールします。

```sh
sudo apt-get install \
  clang-18 libclang-18-dev llvm-18 llvm-18-dev clang-tidy-18
```

ビルドを設定する際に、cmake フラグに `CATA_CLANG_TIDY_PLUGIN=ON` を追加します。

### Fedora 40 でのプラグインのビルド

[atomic cmake guide](../../dev/guides/building/atomic_cmake.md) の正確な手順に従い、以下の変更を加えます。

さらに、以下の依存関係をインストールします。

```sh
sudo dnf install clang-devel llvm-devel clang-tools-extra-devel
```

ビルドを設定する際に、cmake フラグに `CATA_CLANG_TIDY_PLUGIN=ON` を追加します。

### プラグインの使用

単一のファイルでプラグインを実行するには、プロジェクトルートで以下のコマンドを実行します。

```sh
$ ./build-scripts/clang-tidy-wrapper.sh -fix src/achievement.cpp
```

複数のファイルでプラグインを実行するには、[GNU parallel](https://www.gnu.org/software/parallel/)を使用します。

```sh
$ parallel ./build-scripts/clang-tidy-wrapper.sh -fix ::: src/*.cpp
```

## include-what-you-use

[include-what-you-use](https://github.com/include-what-you-use/include-what-you-use) (IWYU) は、インクルードを最適化することを目的としたプロジェクトです。必要なヘッダーを計算し、適切にインクルードを追加および削除します。

このコードベースで実行すると、いくつかの問題が明らかになりました。以下の PR がマージされているバージョンの IWYU が必要になります（執筆時点ではまだ行われていませんが、うまくいけば clang-10 リリースの IWYU に含まれる可能性があります）。

- https://github.com/include-what-you-use/include-what-you-use/pull/775

IWYU をビルドしたら、`CMAKE_EXPORT_COMPILE_COMMANDS=ON` をオンにして cmake を使用してコードベースをビルドし、コンパイルデータベースを作成します（ビルドディレクトリで `compile_commands.json` を探して、それが機能したかどうかを確認してください）。

次に、以下を実行します。

```sh
iwyu_tool.py -p $CMAKE_BUILD_DIR/compile_commands.json -- -Xiwyu --mapping_file=$PWD/tools/iwyu/cata.imp | fix_includes.py --nosafe_headers --reorder
```

IWYU は clang-tidy が好まない C スタイルのライブラリヘッダーを追加することがあるため、clang-tidy（上記で説明）を実行し、IWYU を2回目に再実行する必要がある場合があります。

`tools/iwyu` には、IWYU が適切なヘッダーを選択するのに役立つマッピングファイルがあります。ほとんどは非常に明白であるはずですが、SDL マッピングについてはさらなる説明が必要かもしれません。プラットフォーム依存の問題（Windows ではインクルードパスが異なる）を処理するため、ほとんどの SDL インクルードを `sdl_wrappers.h`, 経由で強制的に行うようにしたいと考えています。いくつかの例外があります (`SDL_version.h` and
`SDL_mixer.h`)。前者は、`main.cpp` がすべての SDL ヘッダーをインクルードできないためです。これらは
`#define WinMain`を定義します。`sdl.imp` のすべてのマッピングは、これを実現するように設計されています。

私たちは、いくつかの状況で IWYU プラグマを使用する必要があります。理由の一部は次のとおりです。

- IWYU には
  [関連ヘッダー](https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md#iwyu-pragma-associated)の概念があり、各 cpp ファイルには任意の数の関連ヘッダーを含めることができます。cpp ファイルは、それらのヘッダーで宣言されたものを定義することが期待されます。Cata では、ヘッダーと cpp ファイル間のマッピングはそれほど単純ではないため、複数の関連ヘッダーを持つファイルと、関連ヘッダーを持たないファイルがあります。どの cpp ファイルにも関連付けられていないヘッダーは、そのインクルードが更新されないため、ビルドが壊れる可能性があり、理想的にはすべてのヘッダーが何らかの cpp ファイルに関連付けられるべきです。次のコマンドを使用すると、現在どの cpp ファイルにも関連付けられていないヘッダーのリストを取得できます（GNU sed が必要です）。

```sh
diff <(ls src/*.h | sed 's!.*/!!') <(for i in src/*.cpp; do echo $i; sed -n '/^#include/{p; :loop n; p; /^$/q; b loop}' $i; done | grep 'e "' | grep -o '"[^"]*"' | sort -u | tr -d '"')
```

- [clang のバグ](https://bugs.llvm.org/show_bug.cgi?id=20666)のため、明示的なインスタンス化のテンプレート引数での使用はカウントされず、`IWYU pragma: keep`が必要になる場合があります。

- IWYU の
  [これらの](https://github.com/include-what-you-use/include-what-you-use/blob/4909f206b46809775e9b5381f852eda62cbf4bf7/iwyu.cc#L1617)
  [欠落している](https://github.com/include-what-you-use/include-what-you-use/blob/4909f206b46809775e9b5381f852eda62cbf4bf7/iwyu.cc#L1629)
  機能のため、戻り値の型のテンプレート引数での使用はカウントされず、他の `IWYU pragma: keep`の要件につながります。

- IWYU は、マップで使用される型に特に問題を抱えているようです。詳細には調査していませんが、ここでもプラグマで回避しました。
