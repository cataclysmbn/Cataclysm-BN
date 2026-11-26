# C++ Lua 統合

この文書では、Cataclysm: Bright Nights における Lua 統合の実装詳細を説明します。

BN はスクリプトの実行に Lua 5.3.6 を使用し、C++ 側のバインディングには sol2 v3.3.0 を使用しています。

## C++ の構成

### Lua ソースファイル

ビルド設定を簡単にし、移植性を高めるため、`Lua 5.3.6` のソースコードを `src/lua/` ディレクトリーに同梱しています。ビルドシステムがそれをコンパイルし、ゲーム実行ファイルとテスト用ライブラリーにリンクします。

### Sol2 ソースファイル

Sol2 は簡単に同梱できます。`src/sol/` に `sol2 v3.3.0` の単一ヘッダー amalgamated 版があり、必要な場所でインクルードします。ヘッダーはかなり大きいため、インクルードするソースファイルは少ないほどよいでしょう。

- `sol/config.hpp` - 設定ヘッダー。いくつかのオプションがここで定義されています。
- `sol/forward.hpp` - 前方宣言。`sol/sol.hpp` の代わりにゲームのヘッダーからインクルードすべき軽量なヘッダーです。
- `sol/sol.hpp` - sol2 のメインヘッダー。非常に大きいため、ゲームのヘッダーからのインクルードは避けてください。

### ゲームのソースファイル

Lua に関係するゲームのソースファイルにはすべて `catalua` 接頭辞が付いています。

新しいバインディングを追加する場合は、`src/catalua_bindings.cpp` にある既存の例を確認し、Sol2 ドキュメントの関連部分を読んでください。

- `catalua.h`（および `catalua.cpp`）- Lua のメインインターフェース。コードベースのほとんどがインクルードする唯一のヘッダーで、公開インターフェースを提供します。
- `catalua_sol.h` および `catalua_sol_fwd.h` - コンパイル用のカスタムプラグマを備えた `sol/sol.hpp` と `sol/forward.hpp` のラッパーです。
- `catalua_bindings*` - ゲームの Lua バインディングはここにあります。座標バインディング（`catalua_bindings_coords*.cpp`、`catalua_coord.h`）は特に複雑です。Lua 側の API と設計上の理由については [`coordinates.md`](coordinates.md) を参照してください。
- `catalua_console.h`（`.cpp`）- ゲーム内 Lua コンソール。
- `catalua_impl.h`（`.cpp`）- `catalua.h`（`.cpp`）の実装詳細。
- `catalua_iuse_actor.h`（`.cpp`）- Lua 駆動の `iuse_actor`。
- `catalua_log.h`（`.cpp`）- コンソール用のインメモリーロギング。
- `catalua_luna.h` - 自動ドキュメント生成機能を備えたユーザータイプ登録インターフェース、通称 `luna`。
- `catalua_luna_doc.h` - `luna` で登録されるか、ドキュメント生成器に公開される型の一覧。
- `catalua_readonly.h`（`.cpp`）- Lua テーブルを読み取り専用としてマークする関数。
- `catalua_serde.h`（`.cpp`）- Lua テーブルと JSON の間の（逆）シリアライズ。
- `catalua_type_operators.h` - `string_id` 型のバインディング実装を補助するマクロ。
