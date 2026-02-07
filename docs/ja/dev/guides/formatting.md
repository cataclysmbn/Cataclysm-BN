---
title: Formatting & Linting
---

このガイドでは、Cataclysm: Bright Nightsにおけるコードの整形と静的解析の方法について説明します。

## クイックリファレンス

| ファイル形式    | ツール         | コマンド                                           |
| --------------- | -------------- | -------------------------------------------------- |
| C++ (`.cpp/.h`) | astyle         | `cmake --build build --target astyle`              |
| JSON            | json_formatter | `cmake --build build --target style-json-parallel` |
| Markdown        | deno fmt       | `deno fmt`                                         |
| TypeScript      | deno fmt       | `deno fmt`                                         |
| Lua             | dprint         | `deno task dprint fmt`                             |

## 自動フォーマット

プルリクエスト(PR)は、[autofix.ci](https://autofix.ci)によって自動的にフォーマットされます。コードにスタイル違反がある場合、修正のためのコミットが自動的にプッシュされます。

> [!TIP]
> autofixによるコミット後のマージコンフリクトを避けるには、以下のいずれかを行ってください:
>
> 1. `git pull` を実行して autofix のコミットを取り込んでから作業を継続する。
> 2. プッシュする前にローカルでフォーマットを実行する(必要に応じて `git push --force` を行う)。

## C++ のフォーマット

C++ ファイルは [astyle](http://astyle.sourceforge.net/)を使用してフォーマットします。

```sh
# astyle のインストール (Ubuntu/Debian)
sudo apt install astyle

# astyle のインストール (Fedora)
sudo dnf install astyle

# astyle のインストール (macOS)
brew install astyle
```

### CMake を使用する場合

```sh
# 設定(一度だけ実行、または既存のビルドを使用)
cmake --preset lint

# すべてのC++ファイルをフォーマット
cmake --build build --target astyle
```

スタイルの設定は、リポジトリのルートにある `.astylerc` に記述されています。

## JSON のフォーマット

JSONファイルは、プロジェクトのソースからビルドされたカスタムツール `json_formatter`を使用してフォーマットします。

### CMake を使用する場合

```sh
# 設定(一度だけ実行、または既存のビルドを使用)
cmake --preset lint

# すべてのJSONファイルを並列でフォーマット
cmake --build build --target style-json-parallel

# すべてのJSONファイルを順次フォーマット (低速ですがデバッグに便利です)
cmake --build build --target style-json
```

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

コミットする前に、以下のチェックを実行してください:

```sh
# 設定の実行 (フォーマットツールを含むビルドディレクトリを作成)
cmake --preset lint

# すべてのコードをフォーマット
cmake --build build --target astyle           # C++
cmake --build build --target style-json-parallel  # JSON
deno fmt                                       # Markdown/TypeScript
deno task dprint fmt                           # Lua
```

## CI 連携

CI パイプラインでは、以下のチェックが自動的に実行されます:

1. **JSON 構文の検証** - `build-scripts/lint-json.sh`
2. **JSON フォーマット** - `cmake --build build --target style-json-parallel`
3. **ダイアログのバリデーション** - `tools/dialogue_validator.py`

いずれかのチェックに失敗すると、ビルドは失敗します。プッシュする前に、上記のコマンドを使用してローカルで問題を修正してください。

## エディタの統合

### VS Code

自動フォーマットのために以下の拡張機能をインストールしてください:

- **C++**: [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) astyle 連携を
  有効にする
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno)
  Markdown/TypeScript用

### Vim/Neovim

設定ファイルに以下を追加してください:

```vim
" Format C++ with astyle on save
autocmd BufWritePre *.cpp,*.h !astyle --options=.astylerc %

" Format with deno
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## トラブルシューティング

### "json_formatter not found" または "style-json-parallel target not found"

CMakeの設定を `lint` プリセット、または `-DJSON_FORMAT=ON`を指定して行っているか確認してください:

```sh
cmake --preset lint
cmake --build build --target json_formatter
```

### "astyle target not found"

`astyle` がインストールされており、PATH が通っているか確認してください:

```sh
# astyle が利用可能か確認
which astyle

# インストールされていない場合 (Ubuntu/Debian)
sudo apt install astyle
```

その後、CMake を再構成します:

```sh
cmake --preset lint
```

### astyle の実行結果が異なる

リポジトリのルートにある`.astylerc` を使用しているか確認してください:

```sh
astyle --options=.astylerc src/*.cpp
```
