---
title: エンジン/クライアントプロトコル 0.3
---

外部クライアントは、`cataclysm-bn-tiles --client=mcp` の stdio 上で、改行区切りの JSON-RPC 2.0 を使ってゲームをプレイします。エンジンが各画面とその画面でできる操作を記述するため、クライアントにゲームルールは不要です。

## セッション

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["0.3"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"0.3","epoch":"epoch:e7f3",...}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90"},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

`parts` 個のスナップショットパートを集め、その後 `bn.events` を順に適用します。欠落、epoch の不一致、`bn.resync` のいずれかがあれば、`bn.subscribe` を再度呼び出します。

## メソッド

| メソッド                 | パラメータ                                                | 結果                                                                           |
| ------------------------ | --------------------------------------------------------- | ------------------------------------------------------------------------------ |
| `bn.hello`               | `versions`, `client`, `locale?`, `capabilities?`          | `version`, `epoch`, `engine`, `limits`, `delivery?`, `locale`, `engine_locale` |
| `bn.viewport`            | `cols`, `rows`                                            | なし                                                                           |
| `bn.interrupt`           | なし                                                      | なし                                                                           |
| `bn.hold`                | `epoch`, `screen_id`, `affordance_id`                     | なし                                                                           |
| `bn.release`             | なし                                                      | なし                                                                           |
| `bn.subscribe`           | なし                                                      | スナップショットヘッダー。続いてパートとイベント                               |
| `bn.unsubscribe`         | なし                                                      | なし                                                                           |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `component_id`, `offset`, `limit` | `boundary_id`, `component_id`, `total`, `rows` (読み取り専用)                  |
| `bn.world.describe`      | `epoch`, `boundary_id`, `pos`                             | `lines` (読み取り専用)                                                         |
| `bn.world.cells`         | `epoch`, `min`, `max`                                     | `at`, `cells`, `forgotten` (読み取り専用)                                      |
| `bn.overmap.cells`       | `epoch`, `min`, `max`                                     | オーバーマップ階層の `overmap` マス (読み取り専用)                             |
| `bn.command.submit`      | `epoch`, `expect`, `operation`                            | `command_id`, `stage: "received"`                                              |
| `bn.command.result`      | `epoch`, `command_id`                                     | 最新のステージ。失われた通知の復旧用                                           |
| `bn.credit`              | `frames`                                                  | 付与後に保持する `credits`                                                     |

エンジンからの通知: `bn.snapshot.part`、`bn.events`、`bn.command`、`bn.resync`、`bn.loading`、`bn.progress`。

エラーはコード `1000` と `error.data = {kind, action?, at?}` を使います。`action` は次に行う操作(`hello`、`subscribe`、`retry`)を示します。

## 画面

すべての境界は、開いている画面を外側から順に並べた `stack` を持ちます。affordance を持つのは最後の画面だけです。マップ(role `map`)が最後のときは、ほかの画面は入力を受け取りません。操作するには、`operation: {kind: "invoke", affordance_id, args?}` と、その affordance の `expect` スコープを付けて `bn.command.submit` を送ります。

- `screen`: `{screen_id}`。その画面が最前面にある間のみ有効で、それ以外は `stale_screen` になります。
- `boundary`: `{boundary_id, revision}`。コマンドの実行中は、submit が `command_busy` で失敗します。

独自の記述を持たない画面は、role が `screen`、kind が入力コンテキスト名の小文字形になり、端末のセルが `canvas` として入ります。`avatar.sidebar` は、ネイティブのサイドバーを同じセル形式で表したものです。

## コマンド

`operation.kind` には `action`(登録済みのアクション)と `travel`(マップのクリック)も指定でき、これらは `{boundary_id, revision}` を期待します。古い `expect` は拒否されるため、クライアントが見ていない画面を操作することはありません。

ステージ: `received`、`validated`、`executing`、`completed`。または `rejected`。または `interrupted` で、理由は `error` に入ります。

## 配信クレジット

`bn.hello` の `delivery = {credits, max_hz?}` で有効にします。プッシュされる `bn.events` または `bn.command` のフレームは、クレジットを 1 消費します。クレジットがなくなると、エンジンは `bn.credit` が来るまでフレームを保留します。保留の上限を超えると、reason が `backpressure` の `bn.resync` を送ります。

## フレーミング

フレームは 1 行の JSON 値 1 つで、長さは `limits.frame_bytes` 以下です。JSON でない行には `-32700`、形の誤った JSON には `-32600`、未知の `bn.` メソッドには `-32601`、不正な params には `-32602` で応答します。サイズ超過または途中で切れたフレームは、終了コード 1 でエンジンを終了させます。

## 単一のクロック

- `epoch` は、プロセスの開始時またはワールドの置き換え時に変わります。
- `sequence` は発行されたイベントの数です(10 進数の文字列、連続)。`revision` は状態を変えるイベントの数です。`at = {epoch, sequence, revision}` は「sequence までのすべてのイベントを含む」ことを意味します。

## 値

- `pos = {dim, x, y, z}` は絶対的なマップのマスです。`dim` はゲームのディメンションで、プライマリは `""` です。
- `look = {kind, id, glyph, color, name}` はゲームデータ由来の見た目を持つため、テキストクライアントにタイルセットは不要です。
- イベントの `changes` は `coverage`、`view`、`cells`、`forgotten`、`entities`、`gone` の順に適用され、続いて `avatar`、`environment`、`route`、`interaction` が置き換えられます。
