---
title: エンジンとクライアント
---

1 つの実行ファイルに有効なクライアントがすべてリンクされ、起動時に `--client=tiles`、`--client=curses`、`--client=mcp` のいずれかを選びます。`TILES`、`CURSES`、`MCP` がビルドするクライアントを決めます。デフォルトでは tiles、curses、MCP の順に優先されます。

すべてのクライアントは同じゲームループと `input_context` ロジックを実行し、同じ `catacurses` のセルウィンドウに描画します。ネイティブクライアントはこのセルをそのまま表示します。外部クライアントは入力境界ごとに[プロトコル](../reference/engine_client_protocol.md)でセルを受け取ります。

クライアントを追加するには、デバイスインターフェースを実装し、ファクトリを登録し、ネイティブソースを `CataClientSources.cmake` に追加します。
