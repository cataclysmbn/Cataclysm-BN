---
edit: false
---

# CLI オプション

> [!NOTE]
>
> 英語の原文は `scripts/gen_cli_docs.ts` から自動生成されます。翻訳を更新する際は、最新の英語の原文を参照してください。

ゲームの実行可能ファイルは、皆様お気に入りのローグライクを実行するだけでなく、MOD制作者や開発者を支援するための多数のコマンドラインオプションを提供します。

---

## 情報

### `--help`

このメッセージを出力して終了します。

### `--version`

バージョンを出力して終了します。

### `--paths`

ゲームが使用するパスを出力して終了します。

## コマンドライン パラメータ

### `--seed <string of letters and or numbers>`

乱数ジェネレーターのシード値を設定します。

### `--jsonverify`

BN の JSONファイル をチェックします。

### `--check-mods [mods…]`

既定または指定された BN MOD の JSON ファイルを検証します。

### `--check-all-mods`

廃止されていないすべての BN MOD の JSON ファイルを検証します。

### `--dump-stats <what> [mode = TSV] [opts…]`

アイテムの統計情報をダンプします。

### `--world <name>`

ワールドをロードします。

### `--basepath <path>`

すべてのゲームデータ サブディレクトリのベースパス。

### `--dont-debugmsg`

設定されている場合、デバッグメッセージは出力されません。

### `--lua-doc <output path>`

指定されたパスに Lua ドキュメントを生成して終了します。

### `--lua-types <output path>`

指定されたパスに Lua 型定義を生成して終了します。

### `--gpu-backend <driver>`

診断用に SDL_GPU のバックエンドドライバーを指定します（`vulkan` / `direct3d12` / `metal` / `software`）。

### `--datadir <directory name>`

ゲームデータがロードされるサブディレクトリ名。

### `--autopickupfile <filename>`

コンフィグディレクトリ内にあるオートピックアップ オプションファイル名。

### `--motdfile <filename>`

メッセージ オブ ザ デイ（MOTD）ディレクトリ内にある MOTD ファイル名。

## マップ共有

### `--shared`

マップ共有モードを有効にします。

### `--username <name>`

マップ共有コードに対し、キャラクターにこの名前を使用するように指示します。

### `--addadmin <username>`

マップ共有コードに対し、この名前をキャラクターに使用し、チート機能へのアクセス権を与えるように指示します。

### `--adddebugger <username>`

マップ共有コードに対し、デバッガー内で実行中であることを通知します。

### `--competitive`

マップ共有コードに対し、ゲーム内チート機能へのアクセスを無効にするように指示します。

### `--worldmenu`

マップ共有コードでワールドメニューを有効にします。

## ユーザーディレクトリ

### `--userdir <path>`

./data ディレクトリおよび以下に名前が指定されたファイルに対するユーザーオーバーライドのベースパス。

### `--savedir <directory name>`

ゲームのセーブデータ用のサブディレクトリ名。

### `--configdir <directory name>`

ゲームの設定ファイル用のサブディレクトリ名。

### `--memorialdir <directory name>`

メモリアル（記念碑）用のサブディレクトリ名。

### `--optionfile <filename>`

コンフィグディレクトリ内にあるオプションファイル名。
