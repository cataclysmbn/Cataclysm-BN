---
title: エンジン/クライアントプロトコル 1.0
---

外部クライアントは `cataclysm-bn-tiles --client=mcp` の stdio 上で、改行区切りの JSON-RPC 2.0 によりゲームをプレイします。値は [Draft 2020-12 スキーマ](../../../schema/engine-client/1.0.schema.json)で定義され、以下の例はすべて[例ファイル](../../../schema/engine-client/1.0.examples.json)でスキーマに照らして検証されています。従来の MCP ツール(`bn.observe`、`bn.press` など)は別の面であり変更されません。クライアントが `bn.hello` を呼ぶと、これらのツールは入力を拒否します。

## セッション

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["1.0"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"1.0","epoch":"epoch:e7f3","engine":{"build":"...","mods":["bn"]},"limits":{"frame_bytes":1048576,"cells_per_part":512,"cells_per_query":4096}}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90","schema_id":null},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"validated"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"executing"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

`parts` 個のスナップショットパートを集めてから、`bn.events` を順に適用します。欠落(gap)、epoch の不一致、`bn.resync` があれば `bn.subscribe` を再度呼びます。新しいスナップショットは状態のみを復元し、`lost_after` 以降の一時的なイベントは失われたものとして扱われ、再送されません。

## メソッド

| メソッド                 | パラメータ                                | 結果                                                   |
| ------------------------ | ----------------------------------------- | ------------------------------------------------------ |
| `bn.hello`               | `versions`, `client`, `viewport?`         | `version`, `epoch`, `engine`, `limits`                 |
| `bn.viewport`            | `cols`, `rows`                            | なし                                                   |
| `bn.interrupt`           | なし                                      | なし                                                   |
| `bn.subscribe`           | なし                                      | スナップショットヘッダー。続いてパートとイベントが届く |
| `bn.unsubscribe`         | なし                                      | なし                                                   |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `offset`, `limit` | `boundary_id`, `total`, `choices` (読み取り専用)       |
| `bn.world.describe`      | `epoch`, `boundary_id`, `pos`             | `lines`(読み取り専用)                                  |
| `bn.world.cells`         | `epoch`, `min`, `max`                     | `at`, `cells`, `forgotten` (読み取り専用)              |
| `bn.command.submit`      | `epoch`, `expect`, `operation`            | `command_id`, `stage: "received"`                      |
| `bn.command.result`      | `epoch`, `command_id`                     | 最新のステージ。失われた通知の回復に使う               |

エンジンからの通知は `bn.snapshot.part`、`bn.events`、`bn.command`、`bn.resync`、`bn.loading`、`bn.progress` です。`bn.loading` と `bn.progress` 以外は、購読中のクライアントへ入力境界ごとに送られます。`bn.progress`(`{epoch}`)はハートビートで、待機などの行動が入力境界に到達せずに続いている間、`bn.hello` を済ませたクライアントへ 2 秒ごとに送られます。通信が途絶えたなら、エンジンは動いていません。ワールドの読み込み中は `bn.loading` が同じ役割を果たします。

エラーはコード `1000` と `error.data = {kind, action?, at?}` を使い、`action` が次の対応(`hello`、`subscribe`、`retry`)を示します。

## 単一の時計

- `epoch` はプロセス開始時とワールド入れ替え時に変わり、読み取りでは変わりません。
- `sequence` は公開されたイベントの数です(10進文字列、連続、`"1"` から開始)。`revision` は状態を変えるイベントの数です。`at = {epoch, sequence, revision}` は「sequence までのすべてのイベントを含む」ことを意味します。
- コマンドは `expect = {revision, boundary_id, schema_id}` を持ちます。古い値は拒否されるため、クライアントは見ていない画面に対して操作しません。`schema_id` は、その境界にインタラクションがないときに限り `null` です。

## コマンド

`operation.kind` は `choose`、`fill`、`set_count`、`set_target`、`cancel`(意味ベースのメニュー)、`action`(移動キーなど登録済みの行動)、または `travel`(マップのクリック)のいずれかです。

```json
{ "kind": "choose", "choice_id": "tab:new_game" }
{ "kind": "set_target", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
{ "kind": "action", "action_id": "RIGHT" }
{ "kind": "travel", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
```

すべての境界アクションは、アクティブな入力コンテキストがそのアクションに割り当てた単一キーボードキーの可搬名(`ESC`、`SPACE`、`RETURN`、`UP`、`>` など)を `keys` に列挙します。キーのないアクションは `action` で実行できません。押されたキーで動作するクライアントは、キー設定ファイルから推測せず、`keys` にそのキーを含むアクションを探してください。

`travel` は、インタラクションのない境界で、画面内の絶対マップマスに対する Tiles と curses の左クリックです。最初のクリックはエンジン自身のルートを `route` として公開し、同じマスをもう一度クリックするとネイティブの自動移動が始まります。その後アバターは 1 歩ずつ歩き、各歩でそのコマンドに紐づく通常のイベントが公開され、エンジンが止まる場所(到着、モンスターの視界入りなど)で止まります。歩行が終わって再び入力を待つとコマンドは `completed` になります。他のコマンドや別のマスのクリックは計画を置き換えるか消します。クライアントがルートを自分で計算することはありません。

`context` は同じマスへの右クリック(SEC_SELECT)です。ネイティブでは隣のマスを調べる、隣のドアを閉じる、足元のアイテムを拾う、見えているモンスターを撃つ、あるいは計画中のルートをまず取り消します。`bn.world.describe` はターミナルウィンドウ内のマスにホバーしたときにネイティブのマウスビューが表示するテキストを返します(範囲外は `validation_failed`)。何も選択も計画もしません。メニューでのホイール 1 ステップは登録済みアクション `SCROLL_UP` または `SCROLL_DOWN` で、エンジンがネイティブのカーソルを 3 行動かします。

ステージは `received`、`validated`、`executing`、`completed`、または `rejected`、または `interrupted` で、`interrupted` の `error` が理由を示します(ワールドが入れ替わると `stale_epoch`、ゲームが入力を拒否すると `validation_failed`、それ以外は `not_ready`)。`completed` は入力が再び待たれていることを意味します。待機などの行動は、終わるか、ゲームが何かを尋ねる(ポップアップや中断)まで先に実行されます。その `at` はその境界の終点です。同時に実行できるコマンドは 1 つです(`command_busy`)。 待機などの行動の実行中もリクエストには応答します。`bn.interrupt` はネイティブの中断キーを押し、行動は Tiles と同様に中止するか尋ね、その質問が次の境界になります。行動中でなければ `not_ready` で失敗します。

## 読み込み画面

ワールドの読み込み中はゲームスレッドが処理中で入力境界に到達しないため、`bn.hello` を済ませたクライアントには、ネイティブの読み込み画面の各ステップで `bn.loading` が送られます(購読の有無は問いません)。進行状況はネイティブクライアントが表示する画面と同じです。`title` は現在のコンテキスト(例: "Loading files")、`entries` はその一覧、`index` は処理中の項目(それ以前は完了)、`image` はネイティブクライアントと同じ方法で選ばれた読み込み画像です。`image.path` はゲームのベースパスからの相対パスで、ファイル名に作者が含まれない場合 `author` は省略されます。読み込み画面が終わると `done` が一度だけ送られ、その後に通常どおり `bn.resync` や `bn.events` が続きます。`epoch` はそのステップを送った時点の epoch です。

```text
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","title":"Loading files","entries":["Terrain","Items"],"index":1,"image":{"path":"data/json/loading/Ada_dawn.webp","author":"Ada"}}}
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","done":true}}
```

## 値

- `pos = {dim, x, y, z}` は絶対マップマスで、`dim` はゲームのディメンション、プライマリは `""` です。リアリティバブル座標はワイヤ上に現れません。
- `look = {kind, id, glyph, color}` はゲームデータ由来の見た目を持つため、テキストクライアントにタイルセットは不要です。自動壁の地形はアバターが知っている接続に応じた線文字を持ち、記憶されたマスは `memory.terrain` の上に `memory.overlay`（家具、罠、乗り物部品）を保持します。
- タイルクライアントは同じ `look` をゲームのタイルセットで描画します。エンジンだけが知り得る情報（データとネイティブの選択ロジックを持つのはエンジンのため）を公開し、クライアントは再実装しません。`tile`（`id` と異なるスプライト ID。`corpse_<monster>`、`player_male`、`vp_<part>`）、`looks_like`（データのフォールバック連鎖、近い順）、`subtile` と `rotation`（アバターが知っている隣接マスからネイティブの選択が計算したマルチタイルキーと 90 度回転数。接続ロジックは一箇所のみ）、`stack`（山の表示アイテム）、`attitude` と `aware`（モンスターと NPC。ネイティブ表示は重ねて印を付ける）、`facing`（生物）、マスの `light`（ネイティブの `lit_level`：0 暗い、1 低い、2 明るいのみ、3 照明あり、4 明るい）、`environment.season`、アバターの `look`、キャラクターの `overlays`（装備・手持ちアイテム・ミューテーション・バイオニクスを、ネイティブの順に試すスプライト ID を `tile` と `looks_like` に並べた種別 `overlay` の `look` として）です。ファイル、重み付きバリアント、レイヤー、暗転はタイルセットが定めるものでクライアントが担当します。描画順は地形、家具、罠、フィールド、アイテム、乗り物部品、生物で、`fields` と `items` の最後の要素がネイティブ表示で描かれるものです。記憶されたマスは暗く、`light` が 1 のマスは陰影付きで描かれ、スプライトのない ID は `glyph` にフォールバックします。
- `interaction` はネイティブのメニューやダイアログです。`choices` は最大 200 行ですがサイズにより少なくなることがあるため、残りは `bn.interaction.choices` を `offset = choices.length` から読みます。全体の行数は `choice_total` です。`compat.focus` と `compat.panes` は 1:1 移植のためにネイティブのリスト状態を保持します。クライアントは無視して構いません。メインメニューはネイティブと同じタブ構成です。各タブは `pane_id` を持たない選択肢(`tab:new_game`、`tab:load` など。選択中のタブは `selected`)で、その後に選択中のタブの項目だけが、そのタブの `pane_id` 付きで続きます(`new_game:tutorial`、`settings:options`、`load:<world>`)。別のタブを選ぶと、そのタブの項目を持つ新しい interaction が公開されます。ID は位置にも言語にも依存しません。クラフトメニュー(`context` が `CRAFTING`)も同じタブ構成です。カテゴリタブ(`tab:CC_ELECTRONIC`)、選択中カテゴリのサブタブ(`subtab:CSC_ELECTRONIC_TOOLS`、検索中は無し)、レシピ(`recipe:<hash>`)の順で、後の 2 つはカテゴリを `pane_id` に持ちます。タブまたはサブタブを選ぶとそのグループが一覧され、レシピを選ぶとクラフトし、レシピへの `set_count` は一括クラフトに入ります。各レシピはネイティブの詳細テキスト(スキル、時間、道具、材料)を色タグなしで `description` に持ちます。`filter` フィールドがネイティブの検索で、`submit: true` で `fill` すると検索し、空の値で解除します。
- ワールドはアバターが知っているものです。`cells`(`remembered`、`visible`、`sensed`)、`entities`、`avatar`(能力値、所持品 `inventory`、足元の `ground`)、`environment`、読み込み済みの `coverage`、`view`、保留中の `route`(計画がなければ空または省略)で構成されます。`view` はアバターのいる階層でネイティブの地形ウィンドウが映すマス `{min, max}` です(アバターに追従します)。`travel` クリックはこの内側だけ受理され、外側は `validation_failed` で拒否されます。
- `avatar.inventory` は手に持つアイテム、装備中のアイテム、持ち運ぶスタックの順に並び、各要素は `{look, name, count?, slot}` で、`slot` は `wielded`、`worn`、`carried` のいずれかです。`avatar.ground` はアバター自身のマスにあるスタックを `slot` なしの同じ形で並べます。`name` はネイティブの一覧と同じ表示名で、色マークアップを含みません。どちらの配列も空のときは省略されます。
- `interaction` はネイティブのメニューやダイアログです。`choices` は最大 200 行ですがサイズにより少なくなることがあるため、残りは `bn.interaction.choices` を `offset = choices.length` から読みます。全体の行数は `choice_total` です。`compat.focus` と `compat.panes` は 1:1 移植のためにネイティブのリスト状態を保持します。クライアントは無視して構いません。メインメニューはネイティブと同じタブ構成です。各タブは `pane_id` を持たない選択肢(`tab:new_game`、`tab:load` など。選択中のタブは `selected`)で、その後に選択中のタブの項目だけが、そのタブの `pane_id` 付きで続きます(`new_game:tutorial`、`settings:options`、`load:<world>`)。別のタブを選ぶと、そのタブの項目を持つ新しい interaction が公開されます。ID は位置にも言語にも依存しません。
- 設定画面はネイティブのものです。`settings:options`、`settings:keybindings`、`settings:autopickup`、`settings:safemode`、`settings:distractions`、`settings:colors` から開きます(オプションとキー割り当てはゲーム中も開けます)。`editor` を持つ行は直接設定できます。`fill` の `field_id` に行の `choice_id`、`value` にエディタの形式の値、`submit: true` を指定します。`editor.type` は `bool`(`true`/`false`)、`select`(`values[].id` のいずれか)、`integer`/`float`(`minimum`〜`maximum`)、`text`(`max_length` 以下)で、ネイティブメニューが受け付けない値はエンジンが拒否します。行への `choose` はネイティブの Enter と同じです。オプション: タブ行 `page:<id>` の後に、選択中のページの `option:<NAME>` と `group:<id>` の行が続きます。`cancel` で抜けるとネイティブの「変更を保存しますか?」が出て、保存すると適用され書き込まれます。キー割り当て: `mode:add_local`、`mode:add_global`、`mode:remove`(ネイティブが提供する場合は `mode:execute`)が次の `action:<ID>` 行の動作を決め、フィールドが名前で絞り込み、新しいキーの入力は `key` 型のフィールドで、キー割り当てファイルと同じ綴り(`z`、`=`、`F9`、`CTRL+A`)のキー名を受け取ります。自動拾いとセーフモード: タブ行 `page:<n>`、列行 `column:<n>`、`rule:<n>` 行。ルール操作(追加・削除・移動など)は画面の登録済みアクションです。注意散漫: `bool` エディタ付きの `distraction:<name>` 行(true で中断)。色: `column:<n>` と `color:<name>` 行。色を選ぶとネイティブの色リストが開きます。
- ワールドはアバターが知っているものです。`cells`(`remembered`、`visible`、`sensed`)、`entities`、`avatar`(能力値と所持品 `inventory`)、`environment`、読み込み済みの `coverage`、`view`、保留中の `route`(計画がなければ空または省略)で構成されます。`view` はアバターのいる階層でネイティブの地形ウィンドウが映すマス `{min, max}` です(アバターに追従します)。`travel` クリックはこの内側だけ受理され、外側は `validation_failed` で拒否されます。
- `avatar.sidebar` は同じ getter から取ったネイティブのクラシックサイドバーの値です。`limbs`(`id`、`label` と状態色、`hp`、`hp_max`、バーの `color`、`broken`)、`pain`、`hunger`、`thirst`、`fatigue`、`temperature`、`power`、`location`、`weather`(ネイティブ色名の `{text, color}`)、`focus`、`morale`(`level`、ネイティブの `face`)、`stamina`、`speed`、`movement`(`counter`、`mode` は `walk`、`run`、`crouch`、`prone`)、`safe_mode`、`time`(`season`、`day`、`clock`: 時計の時刻、なければおおよその時間帯、地下では空)、任意の `ambient_temperature`(温度計がある場合のみ)、`weapon`、任意の `style`(武術スタイル)で構成されます。文字列はエンジンの表示言語で、それ以外の値は言語中立です。`stats` の項目はネイティブの `color` を持つことがあります。
- 地形ウィンドウはネイティブのウィンドウサイズ変更と同じ方法で決まります。`bn.hello` の `viewport = {cols, rows}`(マップのマス 1〜512)、または後の `bn.viewport` は、端末をそのマス数にサイドパネルを足した大きさへ変え、エンジンがアバター周辺をその分だけ映すようにします。ネイティブの最小端末サイズが引き続き適用されるため、実際の `view` が正です。何も送らなければ既定の端末が使われます。
- すべてのイベントは汎用の `changes` ブロックを持ち、`coverage`、`view`、`cells`、`forgotten`、`entities`、`gone` の順に適用した後、`avatar`、`environment`、`route`、`interaction` を置き換えます。
- `message.logged` はネイティブのメッセージログです。`data = {id, text, kind, color, count}` で、1 行ごとに 1 イベントがログの順に、原因となったコマンドに紐づいて送られます。`text` は色タグと「x N」を除いた翻訳済みの行、`kind` はネイティブの種別(`good`、`bad`、`mixed`、`warning`、`info`、`neutral`、`debug`)、`color` はその種別のネイティブの色名です。最後の行が繰り返されると、同じ `id` が高い `count` で再度送られ、その行を置き換えます。状態は変えず(`revision` は進みません)、他の一時イベントと同様 `bn.resync` をまたぐと失われます。ネイティブのログがクールダウンで隠すメッセージは送られません。
- `projectile.moved`、`explosion.started`、`explosion.blast`、`explosion.ended`、`combat_text.shown` はネイティブのアニメーション事実です。ゲームがアニメーションを始める箇所で公開されるため、どのクライアントもレンダラーなしで同じ事実を得ます。状態は変えず、原因のコマンドに紐づき、そのコマンドの状態イベントより前に届きます。`display.duration_ms` はネイティブのアニメーションがその事実を保持する時間です(1 ステップあたり `ANIMATION_DELAY` オプション)。`projectile.moved` は `path`(描画順のマス。弾が飛ぶ間はイベントごとに 1 マス)と、グリフとカスタムスプライト ID を持つ種別 `projectile` の `look` を持ちます。`explosion.started` は `at`、`radius`、`color` と、ネイティブのタイルが使うタイル ID `tile` を持ちます。事実はネイティブのオプションに従い、`ANIMATIONS` が無効なら何も公開されず、戦闘テキストには `ANIMATION_SCT` も必要です。`explosion.blast` は地形で形が変わる爆発の 1 リング分の `cells` を並べます。`combat_text.shown` は `at` に出る 1〜2 色の `segments` の浮遊テキストです。

イベントは `interaction.changed`、`coverage.moved`、`turn.passed`、`cells.seen`、`message.logged` と上記のアニメーション事実で、今後スキーマに挙げた残りの演出タイプが加わります。1.0 の公開前に追加したイベントタイプは 1.0 に含まれ、公開後は新しいイベントタイプに新しい正確なバージョンが必要です。
