# Lua で Mod を作る

## 便利なリンク

- [Lua 5.3 リファレンスマニュアル](https://www.lua.org/manual/5.3/)
- [Sol2 ドキュメント](https://sol2.readthedocs.io/en/latest/)
- [Programming in Lua（初版）](https://www.lua.org/pil/contents.html)

## サンプル Mod

`data/mods/` には、ここで説明する Lua API を利用したコメント付きのサンプル Mod がいくつかあります。
これらです:

- `smart_house_remotes` - ガレージの扉や窓のカーテンを操作するリモコンを追加します。
- `saveload_lua_test` - Lua のセーブ/ロード API をテストする Mod です。

## ゲーム内 Lua コンソール

ゲーム内 Lua コンソールは、デバッグメニューまたは `Lua Console` ホットキー（デフォルトでは未割り当て）から利用できます。

シンプルな機能ですが、入力履歴を保持し、Lua スクリプトの出力とエラーを表示し、Lua スニペットを実行して返された値を表示できます。

`gdebug.set_log_capacity( num )` を実行してコンソールログの容量（デフォルトは100件）を調整したり、`gdebug.clear_lua_log()` で消去したりできます。

## Lua ホットリロード

Mod 開発を速くするため、BN は Lua のホットリロード機能をサポートしています。

ファイルシステムウォッチャーはないため、ホットリロードは対応する
`Reload Lua Code` ホットキー（デフォルトでは未割り当て）で手動で起動する必要があります。コンソール
ウィンドウで対応するホットキーを押すか、`gdebug.reload_lua_code()` コマンドを実行して起動することもできます。
通常の Lua スクリプトから実行すると意図しない結果になる可能性があるため、自己責任で使用してください!

すべてのコードをホットリロードできるわけではありません。詳しくは後のセクションで説明します。

## ゲームデータの読み込み

ワールドを読み込むとき、ゲームはおおよそ次の手順を実行します:

1. ワールド関連の内部状態を初期化し、ワールドをアクティブにする
2. ワールドのアーティファクトアイテムタイプを読み込む（アーティファクトは暫定的な仕組みで、relic と Lua に置き換えられる可能性が高い）
3. ワールドで使用する Mod の一覧を取得する
4. 一覧に従って Mod を読み込む
5. アバター関連の内部状態を初期化する
6. セーブディレクトリから実際のオーバーマップデータ、アバターデータ、リアリティバブルデータを読み込む

ここで重要なのは Mod の読み込み段階です。いくつかのサブステップがあります:

1. ロード関数がワールドの Mod 一覧を受け取る
2. 存在しない Mod を破棄し、それぞれについてデバッグメッセージを表示する
3. 一覧に残った Mod を確認し、Lua が必要なのにゲームのビルドが Lua をサポートしていない場合はエラーを出す
4. ゲームの Lua API バージョンが Mod の使用するバージョンと異なる場合は警告を出す
5. 一覧にある Lua を使う各 Mod について、Mod の [`preload.lua`](#preloadlua) スクリプトを実行する（存在する場合）
6. 一覧と同じ順序ですべての Mod を巡回し、各 Mod フォルダーから JSON 定義を読み込む
7. 読み込んだデータを確定する（copy-from を解決し、複雑な状態を持つ型を使用できるよう準備する）
8. 一覧にある Lua を使う各 Mod について、Mod の [`finalize.lua`](#finalizelua) スクリプトを実行する（存在する場合）
9. 読み込んだデータの整合性を確認する（値を検証し、問題のある値の組み合わせなどを警告する）
10. (R) 一覧にある Lua を使う各 Mod について、Mod の [`main.lua`](#mainlua) スクリプトを実行する（存在する場合）

したがって、Mod の Lua コードを置けるスクリプトは [`preload.lua`](#preloadlua)、
[`finalize.lua`](#finalizelua)、[`main.lua`](#mainlua) の3つだけです。この3つの違いと
用途については以下で説明します。

必要に応じて、1つ、2つ、または3つすべてのスクリプトを使用できます。

ホットリロードを実行すると、ゲームは (R) の付いた手順を繰り返します。つまり、作業中のコードを
ホットリロード可能にしたい場合は、[`main.lua`](#mainlua) に置いてください。

<a id="preloadlua"></a>

### `preload.lua`

このスクリプトではイベントフックを登録し、ゲームの JSON 読み込みシステムが参照する定義（アイテム使用アクションなど）を設定します。ここで登録し、後の段階（例えば [`main.lua`](#mainlua)）で定義することで、ホットリロードをフックに反映させることもできます。

<a id="finalizelua"></a>

### `finalize.lua`

このスクリプトは copy-from の解決後に JSON から読み込まれた定義を Mod が変更できるようにするものですが、現在はまだ API がありません。

TODO: finalization の API

<a id="mainlua"></a>

### `main.lua`

このスクリプトでは Mod の主なロジックを実装します。これには次のものなどが含まれます:

1. Mod のランタイム状態
2. ゲーム開始時の Mod 初期化
3. 必要に応じた Mod の保存・読み込みコード
4. [`preload.lua`](#preloadlua) で設定したフックの実装

## Lua API の詳細

バニラ Lua では多くのことができますが、統合には潜在的なバグを防ぐための制限があります:

- パッケージ（または Lua モジュール）の読み込みは `data/lua/` と `data/mods/<mod_id>/` ディレクトリーに制限されます。
- 現在の Mod ID は `game.current_mod` 変数に保存されます。
- Mod のランタイム状態は `game.mod_runtime[ game.current_mod ]` テーブルに保存します。他の Mod の ID が分かる場合は、同じように
  `game.mod_runtime[ that_other_mod_id ]` でランタイム状態にアクセスして、他の Mod と連携できます。

- グローバル状態への変更はスクリプト間で利用できません。これは関数名と変数名の偶発的な衝突を防ぐためです。
  グローバル変数や関数は定義できますが、あなたの Mod からしか見えません。

### `require` によるモジュール読み込み

`require` 関数は、Mod のコードを整理するため複数のインポートパターンをサポートします:

#### 相対インポート

現在のファイルを基準にモジュールを読み込みます:

```lua
-- In data/mods/my_mod/main.lua
local utils = require("./lib/utils")          -- loads lib/utils.lua
local config = require("./config")            -- loads config.lua in same dir

-- In data/mods/my_mod/lib/foo.lua
local helper = require("./helper")            -- loads lib/helper.lua
local parent_mod = require("../parent")       -- loads parent.lua from mod root
```

#### 標準ライブラリーのインポート

`lib.` または `bn.lib.` 接頭辞を使い、`data/lua/lib/` から共有ライブラリーを読み込みます:

```lua
-- Assuming penlight is installed in data/lua/lib/pl/
local pl_utils = require("lib.pl.utils")      -- loads data/lua/lib/pl/utils.lua
local pl_path = require("bn.lib.pl.path")     -- same, bn.lib. prefix also works
```

#### Mod ローカルの絶対インポート

接頭辞なしのドット記法で Mod ディレクトリーからモジュールを読み込みます:

```lua
-- In data/mods/my_mod/main.lua
require("foo.bar.baz")  -- searches for:
                        -- 1. <mod>/foo/bar/baz.lua
                        -- 2. <mod>/foo/bar/baz/init.lua
```

#### 検索順序

- **相対（`./`、`../`）**: 現在のファイルのディレクトリー
- **標準ライブラリー（`lib.*`、`bn.lib.*`）**: `data/lua/lib/` のみ
- **Mod ローカル（接頭辞なし）**: 現在の Mod ディレクトリーのみ

> [!CAUTION]
> 名前空間の衝突を避けるため、予約済みの接頭辞（`lib`、`bn`）で Mod モジュールに名前を付けないでください。
> 代わりに、Mod 固有の説明的な名前（例: `mymod.core`、`utils`）を使用してください。

#### モジュール構造

モジュールはテーブルを返す必要があります:

```lua
-- lib/math_helper.lua
local M = {}

M.add = function(a, b)
  return a + b
end

return M
```

他のファイルから次のように使用します:

```lua
local math_helper = require("./lib/math_helper")
local result = math_helper.add(2, 3)
```

### Lua ライブラリーと関数

スクリプトが呼び出されると、いくつかの標準 Lua ライブラリーがあらかじめ読み込まれています:

| ライブラリー | 説明                             |
| ------------ | -------------------------------- |
| `base`       | print、assert、その他の基本関数  |
| `math`       | 数学関連のすべて                 |
| `string`     | 文字列ライブラリー               |
| `table`      | テーブルの操作・参照関数         |
| `package`    | `require` でモジュールを読み込む |

詳細は Lua マニュアルの `Standard Libraries` セクションを参照してください。

ここにある関数の一部は BN によってオーバーロードされています。詳しくは[グローバルオーバーライド](#global-overrides)を
参照してください。

### グローバル状態

必要なデータとゲームのランタイム状態のほとんどは、グローバルな `game` テーブルから利用できます。次のメンバーがあります:

| 変数                        | 説明                                                                   |
| --------------------------- | ---------------------------------------------------------------------- |
| `game.current_mod`          | 読み込み中の Mod の ID（スクリプト実行中のみ利用可能）                 |
| `game.active_mods`          | 読み込み順のアクティブなワールド Mod 一覧                              |
| `game.mod_runtime.<mod_id>` | Mod のランタイムデータ（各 Mod が ID 名の独自テーブルを持つ）          |
| `game.mod_storage.<mod_id>` | セーブ時に自動保存・読み込みされる Mod ごとのストレージ                |
| `game.cata_internal`        | ゲーム内部用。使用しないでください                                     |
| `game.hooks.<hook_id>`      | Lua スクリプトに公開され、対応するイベントで呼び出されるフック         |
| `game.iuse.<iuse_id>`       | アイテムファクトリーが認識し、アイテム使用時に呼び出すアイテム使用関数 |

### ゲームバインディング

ゲームはさまざまな関数、定数、型を Lua に公開します。関数と定数は整理のため「ライブラリー」に
まとめられています。型はグローバルに利用でき、メンバー関数やフィールドを持つ場合があります。

関数、定数、型の完全な一覧を見るには、`--lua-doc` コマンドライン引数でゲームを起動します。これにより `config`
フォルダーに `lua_doc.md` というドキュメントファイルが生成されます。

多くの API 関数はゲーム世界の位置を扱います。ゲームは各位置に原点（基準フレーム）とスケール（単位サイズ）を持たせた型付き座標システムを使用します。

型付き座標の型、算術規則、生の `Tripoint`/`Point` 値を使うべき場合の詳しい説明は、[`coordinates.md`](../explanation/coordinates.md) を
参照してください。

<a id="global-overrides"></a>

#### グローバルオーバーライド

ゲームとの統合を改善するため、一部の関数はグローバルにオーバーライドされています。

| 関数       | 説明                                                                                   |
| ---------- | -------------------------------------------------------------------------------------- |
| print      | `INFO LUA` として debug.log に出力（標準 Lua の print を上書き）                       |
| require    | 相対（`./foo`、`../bar`）と絶対（`pl.utils`）インポートをサポート                      |
| package    | セキュリティ検査付きのカスタム検索器で `data/lua/`、`data/mods/<mod_id>/` から読み込む |
| dofile     | 無効                                                                                   |
| loadfile   | 無効                                                                                   |
| load       | 無効                                                                                   |
| loadstring | 無効                                                                                   |

TODO: dofile などの代替

#### フック

フックの一覧を見るには、自動生成されたドキュメントファイルの `hooks_doc` セクションを確認してください。ここで
フック ID の一覧と、フックが要求する関数シグネチャを確認できます。

こちらも参照: [`hooks.md`](../hooks.md)（優先度順、新しい登録形式、連鎖）。

`game.add_hook` を使って新しいフックを登録できます:

```lua
-- In preload.lua
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_game_save", function(...)
  -- This is essentially a forward declaration.
  -- We declare that the hook exists, it should be called on game_save event,
  -- but we will forward all possible arguments (even if there is none) to,
  -- and return value from, the function that we'll declare later on.
  return mod.my_awesome_hook(...)
end)

-- In main.lua
local mod = game.mod_runtime[game.current_mod]

mod.my_awesome_hook = function()
  -- Do actual work herede
end
```

#### アイテム使用関数

アイテム使用関数は一意の ID を使ってアイテムファクトリーに登録されます。アイテムが有効化されると、
下の例で説明する複数の引数を受け取ります。

```lua
-- In preload.lua
local mod = game.mod_runtime[ game.current_mod ]
game.iuse_functions[ "SMART_HOUSE_REMOTE" ] = function(...)
  -- This is just a forward declaration,
  -- but it will allow us to use SMART_HOUSE_REMOTE iuse in JSONs.
  return mod.my_awesome_iuse_function(...)
end

-- In main.lua
local mod = game.mod_runtime[ game.current_mod ]
mod.my_awesome_iuse_function = function( who, item, pos )
  -- Do actual activation effect here.
  -- `who` is the character that activated the item
  -- `item` is the item itself
  -- `pos` is the position of the item (equal to character pos if character has it on them)
end
```

<a id="translation-functions"></a>

#### 翻訳関数

Mod を他の言語へ翻訳可能にするには、`locale` ライブラリーにバインドされた関数でテキストを取得します。C++ 側の対応について詳しくは
[翻訳 API](../explanation/lua_integration.md) を参照してください。

使用例を以下に示します:

```lua
-- Simple string.
--
-- The "Experimental Lab" text will be extracted from this code by a script,
-- and will be available for translators.
-- When your Lua script runs, this function will search for translation of
-- "Experimental Lab" string and return either translated string,
-- or the original string if there was no translation found.
local location_name_translated = locale.gettext( "Experimental Lab" )

-- ERROR: you must call `gettext` with a string literal.
-- Calling it like this will make it so "Experimental Lab" is NOT extracted,
-- and translators won't see it when they translate the text.
local location_name_original = "Experimental Lab"
local location_name_translated = locale.gettext( location_name_original )

-- ERROR: don't alias the function under different name.
-- Calling it like this will make it so "Experimental Lab" is NOT extracted,
-- and translators won't see it when they translate the text.
local gettext_alt = locale.gettext
local location_name_translated = gettext_alt( "Experimental Lab" )

-- This, however, is fine.
local gettext = locale.gettext
local location_name_translated = gettext( "Experimental Lab" )

-- String with possible plural form.
-- Many languages have more than 2 plural forms with complex rules related to which one to use.
local item_display_name = locale.vgettext( "X-37 Prototype", "X-37 Prototypes", num_of_prototypes )

-- String with context
local text_1 = locale.pgettext("the one made of metal", "Spring")
local text_2 = locale.pgettext("the one that makes water", "Spring")
local text_3 = locale.pgettext("time of the year", "Spring")

-- String with both context and plural forms.
local item_display_name = locale.vpgettext("the one made of metal", "Spring", "Springs", num_of_springs)

--[[
  When some text is tricky and requires explanation,
  it's common to place a special comment denoted with `~` to help translators.
  The comment MUST BE right above the function call.
]]

--~ This comment is good and will be visible for translators.
local ok = locale.gettext("Confusing text that needs explanation.")

--~ ERROR: This comment is too far from gettext call and won't be extracted!
local not_ok = locale.
                gettext("Confusing text that needs explanation.")

local not_ok = locale.gettext(
                  --~ ERROR: This comment is in wrong place and won't be extracted!
                  "Confusing text that needs explanation."
                )

--[[~
  ERROR: Multiline Lua comments can't be used as translator comments!
  This comment won't be extracted!
]]
local ok = locale.gettext("Confusing text that needs explanation.")

--~ If you need a multiline translator comment,
--~ just use 2 or more single-line comments.
--~ They'll be concatenated and shown as a single multi-line comment.
local ok = locale.gettext("Confusing text that needs explanation.")
```
