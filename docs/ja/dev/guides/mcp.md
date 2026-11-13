---
title: MCP でプレイする
---

`cataclysm-bn-mcp` 実行ファイルは、stdio で通信する MCP サーバーです。エージェントはゲームの画面と操作をそのまま使ってプレイします。

```sh
cmake --preset linux-full -B out/build/linux-full-mcp -DMCP=ON -DTILES=ON -DCURSES=OFF
cmake --build out/build/linux-full-mcp --target cataclysm-bn-mcp
```

MCP ホストにこのサーバーを登録します。`--userdir` と `--configdir` は必ず両方指定してください。指定しないと、XDG ビルドが普段使っている設定を読み書きすることがあります。`--world NAME` を付けると、開始時にそのワールドを読み込みます。`--replay-record PATH` を付けるとセッションを記録します。記録は[リプレイのガイド](replay.md)に従って再生します。

```json
{
  "mcpServers": {
    "bn": {
      "command": "/path/to/Cataclysm-BN/out/build/linux-full-mcp/src/cataclysm-bn-mcp",
      "args": [
        "--datadir",
        "/path/to/Cataclysm-BN/data/",
        "--userdir",
        "/path/to/bn-agent-profile/",
        "--configdir",
        "/path/to/bn-agent-profile/config/"
      ]
    }
  }
}
```

ホストが `notifications/initialized` を送るまで、`ping` 以外の呼び出しはすべて失敗します。サーバーはリソース `bn://screen` と `bn://state` も提供します。

| ツール           | 結果                                                                            |
| ---------------- | ------------------------------------------------------------------------------- |
| `bn.observe`     | 合成された画面: `width`、`height`、`cursor`、`rows`、`styles`。                 |
| `bn.state`       | ワールドヘッダー。`part` を指定するとマップの part を 1 つ読みます。            |
| `bn.press`       | `keys` を送り、次の入力地点の画面を返します。                                   |
| `bn.actions`     | `input_id`、`category`、`timeout_ms`、バインディング付きの `actions`。          |
| `bn.interaction` | コントラクトスタックの最上位の画面: `screen_id` と affordance の一覧。          |
| `bn.interact`    | affordance を 1 つ(`affordance_id`、`args`)実行し、次の最上位の画面を返します。 |

`bn.interact` は `bn.command.submit` と同じ経路を通ります。入力は 1 つずつ送ってください。結果を読んでから次の入力を決めます。1 つの入力で別のメニューが開くことがあります。

```json
{ "name": "bn.press", "arguments": { "keys": [{ "action": "inventory" }] } }
```

`bn.actions` で得た `input_id` をイベントに入れると、その後に画面が変わった場合、サーバーがイベントを拒否します。

## 制限

- `bn.hello` を送ったクライアントが入力を所有します。このとき `bn.press` と `bn.interact` は失敗します。
- 1 MiB を超える結果はツールエラーになります。
- キーイベント 1 つには、`action`、`key`(`text` は任意)、`text`、`mouse` のうち、ちょうど 1 つだけを使います。
- `modifiers` は `key` と一緒に使います。
- `keys` には 1 個から 256 個までのイベントを入れられます。
