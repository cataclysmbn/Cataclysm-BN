# アクティビティ (Activities)

アクティビティとは、中断や（任意での）再開が可能な長期アクションを指します。これにより、プレイヤーキャラクター（アバター）やNPCが、1ターン以上かかる作業の最中に発生した出来事（モンスターの出現や負傷など）に対して、適切に反応できるようになります。

## 新しいアクティビティの追加

1. `player_activities.json` 対象となるアクティビティの全インスタンスに適用されるプロパティを定義します。

2. `activity_actor.h` `activity_actor` (例: move_items_activity_actor) を作成し、新しいアクティビティの状態保持と各ター
   ンの処理を記述します。

3. `activity_actor.cpp` 新しいアクターに必要な`start`、`do_turn`、`finish` 関数と、必要なシリアル化(保存) 関数を定義し
   ます。`activity_actor.cpp`の下部にある`deserialize_functions`マップに、新しいアクティビティアクターのデシリアル化関数を追加するのを忘れないでください。中断・キャンセル時に復元すべき複雑な状態を変更するアクティビティの場合は、 `canceled` 関数を定義してください。

4. 再開可能なアクティビティにする場合は、`activity_actor::can_resume_with_internal`を`override`します。

5. アクティビティアクターを構築し、`player_activity`のコンストラクタに渡します。新しく構築されたアクティビティはキャラ
   クターに割り当てられ、`Character::assign_activity`を使用して開始できます。

## JSONプロパティ

- verb: アクティビティを停止するか確認する際のクエリや、状況説明に使用される記述的な用語です。 例: `"verb": "mining"`
  (採掘)、`"verb": { "ctxt": "instrument", "str": "playing" }`(演奏)。

- suspendable: (既定値: true): trueの場合、最初からやり直すことなくアクティビティを継続できます。これは
  `can_resume_with()` が true を返す場合にのみ可能です。

- rooted (既定値: false): trueの場合、アクティビティ中に反動が軽減され、植物系の変異体は地面に根を張ります。数分以上続
  き、かつ足を動かさずに完了できるアクティビティでは true にすべきです。

- special (既定値: false): 特殊なアクティビティとみなされ、他のアクティビティとは異なる変則的なロジックを持つことを想定
  します。

- complex_moves (既定値: false):
  - false の場合 - 速度計算を行わず、1ターンあたり100移動力(moves)を消費します。JSONで `complex_moves` ブロックがない場
    合はこれに該当します。
  - true の場合 - 以下の要素に基づいた複雑な速度/移動力の計算を行います:
    - max_assistants(0): 手伝うことができる他のクリーチャーの最大数 (範囲: 0-32);
    - bench (既定値: false): 作業台を使用して実行可能か;
    - light (既定値: false): 現在の明るさが作業速度に影響するか;
    - speed (既定値: false): キャラクターの移動速度が作業速度に影響するか;
    - skills: アクティビティの速度がスキルの影響を受けます。`skill_name: modifier`のペアで指定するか、あるいは製作や建
      築のように、その場の状況に応じて必要なスキルが動的に決定される場合は `"skills": true` と明示的に指定します:
      - `"skills": true`
      - `"skills": ["fabrication", 5]`
    - stats: アクティビティの速度が能力値の影響を受けます。`stat_name: modifier`、のペアで指定するか、あるいは製作や建
      築のように、その場の状況に応じて必要な能力値が動的に決定される場合は`"stats": true` と明示的に指定します。
      - `"stats": true`
      - `"stats": ["DEX", 5]`
    - qualities: アクティビティの速度が道具の性能の影響を受けます。`q_name: modifier`のペアで指定するか、あるいは製作
      や建築のように、その場の状況に応じて必要な性能が動的に決定される場合は `"qualities": true`と明示的に指定します。
      - `"qualities": true`
      - `"qualities": ["CUT_FINE", 5]`
    - morale (既定値: false): 現在の士気レベルが作業速度に影響するか。

Example for whole block:

```json
"complex_moves": {
"max_assistants": 2,
"bench": true,
"light": true,
"speed": true,
"stats": true,
"skills": [ ], - //same as `"skills": true`
"qualities": [ ["CUT_FINE", "5"] ],
"morale": true
}
```

- morale_blocked (既定値 false): 士気が一定レベルを下回っていると、その作業を行えなくなります。

- verbose_tooltip(既定値 true): 進行状況ウィンドウを拡張し、詳細な情報を表示します。

- no_resume (既定値 false): 再開を許可せず、常に最初からやり直す必要があります。

- multi_activity(既定値 false): 作業ができなくなるまで自動で繰り返されます。NPCやアバターの「ゾーン指定」による活動に使用されます。

- refuel_fires(既定値 false): trueの場合、長期作業中に自動で火の補充（燃料投下）を行います。

- auto_needs(既定値 false) : trueの場合、長期作業中に特定の「自動消費ゾーン」から自動で飲食を行います。

## 終了処理 (Termination)

アクティビティを終了させる方法はいくつかあります:

1. `player_activity::set_to_null()`を呼び出す

   予定より早く完了した場合や、データの破損、アイテムの消失などの問題が発生した場合に呼び出します。この場合、バックログには保存されません。

2. `moves_left` <= 0

   この条件を満たすと、終了関数(ある場合) が呼び出されます。終了関数は `set_to_null()`を呼び出す必要があります。終了関数がない場合は、自動的に呼び出されます。

3. `progress.complete()`

   基本的には`moves_left` <= 0と同じですが、進行状況システムを用いた追加のチェックが行われます。

4. `Character::cancel_activity`

   アクティビティをキャンセルすると、`activity_actor::finish` 関数は実行されず、成果も得られません。代わりに `activity_actor::canceled` が呼び出されます。再開可能なアクティビティであれば、コピーが `Character::backlog`に保存されます。

## 進行状況 (Progress)

`progress_counter`クラスはアクティビティの進捗管理に特化しています。

- targets: 処理待ちターゲットのキュー。ターゲット名、総移動力、残り移動力を保持します;

- moves_total (0): アクティビティ (全タスク)の完了に必要な総移動力;

- moves_left (): アクティビティ完了までの残り移動力;

- idx (1): 現在処理中のタスクのインデックス (1から開始);

- total_tasks (0): 完了済みおよび待機中のタスクの総数。

## 注意事項

キャラクターがアクティビティを実行している間、毎ターン `activity_actor::do_turn`が呼び出されます。ここではJSONプロパティの設定に基づき、いくつかの共通処理が自動的に実行されます。それと同時に、アクティビティ固有の `do_turn` 関数も呼び出され、もし `moves_left`が0以下であれば、終了関数が実行されます。

一部のアクティビティ (MP3プレイヤーでの音楽再生など) は、最終的な成果物を持たず、毎ターン特定の効果 (電池を消費して士気ボーナスを得るなど) を発生させます。

アクティビティの実行中や終了時に特定の情報 (「どこで」作業を行うか、「どの」アイテムを使用して作業するか等) が必要な場合は、作成したアクティビティアクターにデータメンバを追加してください。あわせて、そのデータのシリアル化関数も実装する必要があります。これにより、セーブやロードを挟んでもアクティビティの状態を正確に維持できるようになります。

座標を保存する際は注意してください。 NPCがアクティビティを行う可能性がある場合、座標はローカル（アバター相対）ではなく、絶対座標で保持する必要があります。

### `activity_actor::start`

この関数は、アクティビティがキャラクターに割り当てられた際に一度だけ呼び出されます。時間や速度に基づいて進行するアクティビティにおいて `player_activity::moves_left`や`player_activity::moves_total` の初期値を設定するのに便利です。

### `activity_actor::do_turn`

無限ループの発生を防ぐため、必ず以下のいずれかの条件を満たすように実装してください:

- `do_turn` 内で`player_activity::progress.moves_left`を減少させる。

- `do_turn` 内でアクティビティを停止させる（前述の『終了処理』を参照）。

例えば、`move_items_activity_actor::do_turn` の場合、キャラクターの現在の移動力(moves)が許す限りのアイテムを移動させるか、対象のアイテムが残っていない場合はアクティビティを終了させます。

### `activity_actor::finish`

この関数は、アクティビティが完了した際 (`moves_left` <= 0)に呼び出されます。この中では必ず
`player_activity::set_to_null()` を呼び出すか、あるいは新しいアクティビティを割り当ててください。 完了したアクティビティに対して
`Character::cancel_activity`を呼び出してはいけません。これを呼んでしまうと、完了済みのアクティビティのコピーが
`Character::backlog`に作成され、**すでに終わったはずの作業を再開できてしまう**という不具合の原因になります。
