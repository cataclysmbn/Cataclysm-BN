---
title: Traps
---

### trap

```json
{
  "type": "trap",                // これを罠として定義
  "id": "tr_caltrops_glass",     // 一意な識別子
  "name": "glass caltrops",      // 表示名
  "color": "dark_gray",          // タイルを無効にした場合、またはスプライトが未定義の場合に表示する記号の色
  "symbol": "_",                 // タイルを無効にした場合、またはスプライトが未定義の場合に表示する記号
  "looks_like": "tr_caltrops",   // タイルがない場合のタイルセットへのヒント。looks_like のタイルを使用
  "visibility": 6,               // プレイヤーが設置していない罠を見つける難易度。0ならプレイヤーに常に見える
  "avoidance": 6,                // 踏み込んだときに作動する確率。0なら踏んでも作動しない
  "difficulty": 0,               // 解除の難易度。大きいほど難しい
  "action": "caltrops_glass",    // 作動時の動作。trapfunc.cpp または下記を参照
  "spell_data": { "id": "spell_trap_can_alarm_trigger" },   // spell アクションで罠が唱える呪文の `id`
  "map_regen": "microlab_shifting_hall",    // map_regen アクションで使用する mapgen_update のエントリ
  "remove_on_trigger": true,     // 作動後に罠を除去するか。繰り返し作動する罠では省略（地形生成では非推奨）
  "trigger_items": [ "tripwire", "shotgun_d", { "item": "shot_hull", "quantity": 2, "charges": 1 } ],   // 作動時に生成するアイテム。quantity 倍で、charges は既定スタックを持つアイテムに使用
  "drops": [ "tripwire", "shotgun_d", { "item": "shot_00", "quantity": 2, "charges": 1 } ],   // 解除成功時に落とすアイテム。quantity/charges は trigger_items と同じ
  "vehicle_data": {              // 車両の車輪がこの罠を踏んだときの動作。通常の作動とは別
    "do_explosion": true,        // trueなら直接ダメージではなく爆発を作る
    "damage": 1000,              // 車両タイルへの直接ダメージ、または爆発の中心ダメージ
    "shrapnel": 8,               // 爆発時の破片ダメージ
    "sound_volume": 10,          // 作動時に再生する音の音量
    "sound": "Boom!",            // メッセージログの音の説明。「You hear %s」の形式
    "sound_type": "explosion",  // 種類: background, weather, music, movement, speech, activity, destructive_activity, alarm, combat, alert, order
    "sound_variant": "default"   // 使用する音のバリエーション
  },
  "benign": true,                // 作動させる罠ではなく、ロールマットや漏斗などの無害なものを表す。踏む前の確認をせず、`PATH_AVOID_DANGER_2` フラグのモンスターも避けない。実際に害を与える `action` と併用してはいけない
  "funnel_radius": 200           // 漏斗用。雨が降るとこの値に基づく割合で水を集め、同じタイルの空容器を満たす
  "floor_bedding_warmth": -1000, // ロールマットなどが睡眠時に与える追加の暖かさ
},
```

#### action

trapfunc.cpp で定義されている使用可能なアクション:

- `none` - 漏斗など、罠の動作が不要なものに使用します。
- `bubble` - 踏むと気泡緩衝材の弾ける音がします。
- `glass` - 踏んだものに軽い切断ダメージを与え、ガラスを踏む音がします。
- `cot` - プレイヤーや NPC には安全で快適ですが、モンスターはつまずいて1ターン失います。
- `beartrap` - ダメージを与え、脱出するまでそのタイルに拘束します。脱出後に罠のアイテムが落ちます。
- `board` - 軽いダメージを与え、移動数を減らします。
- `caltrops` - ダメージを与え、移動数を減らします。
- `caltrops_glass` - `caltrops` と同じ基本効果ですが、ガラス音が増えます。
- `tripwire` - 作動に成功すると軽いダメージを与え、対象を後退させて移動数を減らします。
- `crossbow` - 外れる可能性のある刺突ダメージを与えます。後で落ちる弾薬の生成はハードコードされています。
- `shotgun` - 作動させた対象に大きな弾道ダメージを与えます。ID が `tr_shotgun_2` なら2回命中します。
- `blade` - 作動させた対象を直接打撃・切断ダメージで攻撃します。
- `snare_light` - ダメージを与え、脱出まで拘束し、効果が切れると関連アイテムを生成します。
- `snare_heavy` - `snare_light` に似ていますが、ダメージと脱出時のアイテムが異なります。
- `landmine` - 中程度のダメージと大きな破片ダメージを伴う爆発です。
- `boobytrap` - `landmine` と同じ効果ですが、作動時のメッセージが異なります。
- `telepad` - 8タイル以内のランダムな場所へテレポートさせ、壁へ移動する危険とテレグローを与えます。
- `goo` - プレイヤーと NPC はダメージを受けて粘液で遅くなる可能性があります。非粘液モンスターには速度低下、非ロボットには粘液化が起きます。
- `dissector` - 切断ダメージを与えます。`ROBOT` 種の生物は無効です。
- `pit` - 落下ダメージを受け、登り出るまで拘束されます。
- `pit_spikes` - `pit` と同じですが、トゲの追加ダメージがあります。トゲが壊れるとアイテムを生成し、地形を `t_pit` に変えます。
- `pit_glass` - `pit_spikes` に似ていますが、壊れたとき木の槍ではなくガラス片を生成します。
- `lava` - 高い熱ダメージを与えます。
- `portal` - 現在は `telepad` の別名で、同じ効果を発生させます。
- `sinkhole` - 下の地形を `t_pit` に変え、落とし穴の効果を発生させます。グラップリングフック、牛追い鞭、長いロープ、または Web Diver 変異で防げます。
- `ledge` - 下の階へ落下し、落下ダメージを受ける可能性があります。
- `temple_flood` - strange temple で床を深い水に変えるハードコードイベントを起動します。プレイヤーだけが作動できます。
- `temple_toggle` - strange temple で赤・緑・青のパズルタイルを床と壁の間で切り替えます。プレイヤーだけが作動できます。
- `glow` - プレイヤーと NPC を放射線照射・閃光させる可能性があります。モンスターには酸ダメージと速度低下が起こる可能性があります。
- `hum` - かすかな音から耳をつんざく音まで、ランダムな音量のハミングを発生させます。
- `shadow` - 近くに影のモンスターを召喚します。プレイヤーだけが作動できます。
- `map_regen` - 罠の `map_regen` で指定した mapgen update をタイルに適用します。使用時は `remove_on_trigger` と `trigger_items` は処理されません。
- `drain` - 防具と免疫を無視するごく小さなダメージを与えます。
- `spell` - 罠の `spell_data` で指定した呪文を、作動させたものを中心に唱えます。通常は `min_aoe` が必要です。
- `snake` - `shadow` に似ていますが、影の蛇を召喚します。NPC とモンスターも作動させられます。
