---
title: Formatting & Linting
---

このガイドでは、Cataclysm: Bright Nightsにおけるコードの整形と静的解析の方法について説明します。

## クイックリファレンス

| 対象             | ツール               | コマンド         |
| ---------------- | -------------------- | ---------------- |
| staged ファイル  | すべてのフォーマッタ | `just fmt`       |
| すべてのファイル | すべてのフォーマッタ | `just fmt --all` |
| C++ (`.cpp/.h`)  | astyle/clang-format  | `just fmt-cpp`   |
| JSON             | json_formatter       | `just fmt-json`  |
| Markdown/TS      | deno fmt             | `just fmt-docs`  |
| Lua              | dprint               | `just fmt-lua`   |

## 自動フォーマット

プルリクエスト(PR)は、[autofix.ci](https://autofix.ci)によって自動的にフォーマットされます。コードにスタイル違反がある場合、修正のためのコミットが自動的にプッシュされます。

> [!TIP]
> autofixによるコミット後のマージコンフリクトを避けるには、以下のいずれかを行ってください:
>
> 1. `git pull` を実行して autofix のコミットを取り込んでから作業を継続する。
> 2. プッシュする前にローカルでフォーマットを実行する(必要に応じて `git push --force` を行う)。

## C++ のフォーマット

トップレベルの C++ ファイルは [astyle](http://astyle.sourceforge.net/)を使用してフォーマットします。ソースのサブディレクトリにある C++ ファイルは [clang-format](https://clang.llvm.org/docs/ClangFormat.html)を使用してフォーマットします。

```sh
# フォーマッタのインストール (Ubuntu/Debian)
sudo apt install astyle clang-format

# フォーマッタのインストール (Fedora)
sudo dnf install astyle clang-tools-extra

# フォーマッタのインストール (macOS)
brew install astyle clang-format
```

### スクリプトを使用する場合

```sh
just fmt-cpp
# または
build-scripts/format-cpp.sh
```

スタイルの設定は、リポジトリのルートにある `.astylerc` と `.clang-format` に記述されています。

## JSON のフォーマット

JSONファイルは、プロジェクトのソースからビルドされたカスタムツール `json_formatter`を使用してフォーマットします。

### スクリプトを使用する場合

```sh
just fmt-json
# または
build-scripts/format-json.sh
```

このスクリプトは `out/build/json-format` に `json_formatter` をビルドし、JSONファイルをフォーマットします。設定済みのゲームビルドや CMake プリセットは不要です。必要に応じて `CATA_JSON_FORMAT_BUILD_DIR` で補助ビルドディレクトリを変更できます。

> [!NOTE]
> `data/names/` ディレクトリは、名前ファイルに特殊なフォーマット要件があるた
> め、フォーマットの対象から除外されています。

### JSONの構文チェック (Validation)

フォーマットを行う前に、JSON の構文を検証できます:

```sh
build-scripts/lint-json.sh
```

これはすべてのJSONファイルに対してPythonの `json.tool` を実行し、構文エラーを検出します。

## Markdown & TypeScript のフォーマット

MarkdownおよびTypeScriptファイルは[Deno](https://deno.land/)を使用してフォーマットします。

```sh
# Deno のインストール
curl -fsSL https://deno.land/install.sh | sh

# Markdown と TypeScript のフォーマット
deno fmt
```

## Lua のフォーマット

Luaファイルは、Deno 経由で[dprint](https://dprint.dev/)を使用してフォーマットします。

```sh
# Lua ファイルのフォーマット
deno task dprint fmt
```

## ダイアログのバリデーション

NPCのダイアログファイルには、追加の検証項目があります:

```sh
tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*
```

## コミット前のワークフロー

[`prek`](https://github.com/j178/prek) を使った任意の pre-commit フックをインストールできます:

```sh
prek install
# または
just hooks-setup
```

コミット時、フックは `just fmt` を実行します。Deno と dprint は通常どおり実行され、C++ と JSON のフォーマッタは staged の作成/更新ファイルだけを処理します。フォーマットされた staged ファイルは、同じコミットに含まれるよう再度 `git add` されます。

同じフォーマットを staged ファイルに対して手動で実行するには:

```sh
just fmt
```

フックを使わずにコミットする前に実行するコマンド:

```sh
# staged ファイルをフォーマット
just fmt

# フォーマット可能なすべてのファイルをフォーマット
just fmt --all
```

## CI 連携

CI パイプラインでは、以下のチェックが自動的に実行されます:

1. **JSON 構文の検証** - `build-scripts/lint-json.sh`
2. **JSON フォーマット** - `build-scripts/format-json.sh`
3. **ダイアログのバリデーション** - `tools/dialogue_validator.py`

いずれかのチェックに失敗すると、ビルドは失敗します。プッシュする前に、上記のコマンドを使用してローカルで問題を修正してください。

## エディタの統合

### VS Code

自動フォーマットのために以下の拡張機能をインストールしてください:

- **C++**: [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) astyle 連携を
  有効にする
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno)
  Markdown/TypeScript用

### Visual Studio

PowerShell でフォーマッタを一度インストールします。LLVM には `clang-format` と `clang-tidy` が含まれます。

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex
scoop install llvm astyle
clang-format --version
clang-tidy --version
astyle --version
```

Scoop をすでにインストールしている場合は、`scoop install llvm astyle` だけを実行してください。バージョン確認コマンドが
見つからない場合は、PowerShell と Visual Studio を閉じて開き直し、`PATH` を再読み込みしてください。

ツールをインストールした後:

1. リポジトリのフォルダを Visual Studio で CMake プロジェクトとして開きます。
2. CMake を構成します。フォーマッタをインストールする前に Visual Studio がプロジェクトを構成していた場合は、
   CMake キャッシュを再構成します。
3. **View > Other Windows > CMake Targets View** を開きます。
4. コミット前に `format` ターゲットをビルドします。

Visual Studio の通常の **Format Document** コマンドは、このターゲットを実行しません。
リポジトリのスタイルを適用するには `format` ターゲットを使用してください。このターゲットはトップレベルの
C++ ファイルには `astyle` を、`src/` サブディレクトリ内の C++ ファイルには `clang-format` を実行します。

### Vim/Neovim

設定ファイルに以下を追加してください:

```vim
" Format C++ with astyle on save
autocmd BufWritePre *.cpp,*.h !astyle --options=.astylerc %

" Format with deno
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## トラブルシューティング

### "json_formatter not found"

JSON フォーマッタのスクリプトを実行してください。補助フォーマッタを自動的に設定してビルドします:

```sh
build-scripts/format-json.sh
```

### C++ フォーマッタが見つからない

`astyle` と `clang-format` がインストールされており、PATH が通っているか確認してください:

```sh
# フォーマッタが利用可能か確認
which astyle
which clang-format

# インストールされていない場合 (Ubuntu/Debian)
sudo apt install astyle clang-format
```

その後、再実行してください:

```sh
build-scripts/format-cpp.sh
```

### C++ フォーマッタの実行結果が異なる

リポジトリのルートにあるフォーマッタ設定を使用しているか確認してください:

```sh
astyle --options=.astylerc src/*.cpp
clang-format -i src/utils/*.cpp src/utils/*.h
```
