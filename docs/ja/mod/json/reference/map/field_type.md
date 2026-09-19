# フィールドの種類

```jsonc
{
  // 必要なデータ
  "type": "field_type", // これはフィールドタイプです
  "id": "fd_gum_web", // フィールドのID
  // フィールドの強度の配列。
  // 最初のものは強度 1 で、毎回 1 ずつ増加します。
  "intensity_levels": [
    {
      "name": "shadow", // この強度レベルの名前
      "sym": "a", // この電界強度レベルの記号
      "color": "light_red", // フィールドシンボルの呪いの色
      // 可視性
      "transparent": false, // フィールドが透けて見える天候
      "light_override": 3.7, // 使用する光の量は周囲光を無視します。 100 は日光、0 は 100% 暗いです。
      "light_color": "#ffd08a", // 発光する光の色
      "light_emitted": 20, // 発光する光の量
      // 動き
      "dangerous": true, // この強さの天気を通り抜けるのは危険です
      "move_cost": 50, // フィールド内を移動するための移動コスト
      // 放射線
      "extra_radiation_min": 10, // フィールドへの侵入による余分な放射線の最小量
      "extra_radiation_max": 100, // フィールドに入る際の追加放射線の最大量
      "radiation_hurt_damage_min": 1, // 体のすべての部分に最小限の痛みを与える
      "radiation_hurt_damage_max": 3, // 体のすべての部分に引き起こす最大の痛み
      "radiation_hurt_message": "The radioactive gas burns!", // 痛みが出たときに印刷するメッセージ
      // 強度の操作
      "intensity_upgrade_chance": 10, // x 分の 1 の確率で数ティックごとに強度がアップグレードされます
      "intensity_upgrade_duration": 1, // 強度をアップグレードする前に待機するティック数
      // モンスターグループの出現
      "monster_spawn_chance": 10, // モンスター出現のチャンス
      "monster_spawn_count": 10, // 出現するモンスターの数
      "monster_spawn_radius": 10, // モンスターを出現させる半径
      "monster_spawn_group": "GROUP_ZOMBIE", // スポーンするグループ
      // その他
      "convection_temperature_mod": 10, // 加える温度の量
      // 効果
      "effects": [{
        // 基本情報
        "effect_id": "sap", // 適用するエフェクトID
        "body_part": "torso", // 指定された ID を持つボディパーツ
        "intensity": 1, // 効果の強さ
        // 期間
        "max_duration": "2 seconds", // 効果の最大持続時間
        "min_duration": "2 seconds", // 効果の最小持続時間
        // 免疫力と応用の可能性
        "is_environmental": true, // 環境保護は守るだろうか
        "immune_in_vehicle": true, // 車両のどの部分にでも免疫あり
        "chance_in_vehicle": 10, // 上記の条件を満たした場合、x 分の 1 の確率で効果が得られます
        "immune_inside_vehicle": true, // 車内に該当する車両部品に乗っている場合は免疫が得られます
        "chance_inside_vehicle": 10, // 上記の条件を満たした場合、x 分の 1 の確率で効果が得られます
        "immune_outside_vehicle": true, // 車内に該当する車両部品以外の場合は免疫あり
        "chance_outside_vehicle": 10, // 上記条件を満たすと×確率で効果が得られる
        // メッセージ
        "message": "The sap is sticky!", // 効果を受けたときに出力するメッセージ
        "message_npc": "The sap sticks to <npcname>!", // NPC が有効になったときに出力するメッセージ
        "message_type": "bad", // これはどのような種類のメッセージですか?
      }],
    },
  ],
  // グラフィカル
  "display_items": true, // ファイル上の項目を表示するかどうか?
  "display_field": true, // フィールドを表示するかどうか?
  "priority": 1, // フィールドのシンボル/スプライトを表示する優先順位
  // 免疫
  "immunity_data": {
    "traits": ["M_IMMUNE"], // フィールドからの影響を防ぐ特性の配列
    "body_part_env_resistance": [["mouth", 15], ["eyes", 15]], // 野外から保護するために必要な身体部分の耐環境性
  },
  "immune_mtypes": ["mon_zombie"], // フィールドの影響を受けないモンスタータイプ
  "has_fire": true, // 火の免疫により、このフィールドに対する免疫が得られますか
  "has_acid": true, // 酸性免疫はありますか？
  "has_elec": true, // 電気はどうですか？
  "has_fume": true, // 呪いの背景色を白に変更しましょうか？
  // 朽ちて広がる
  "underwater_age_speedup": "1 minute", // さらに多くのターンが水中での熟成を経ます
  "outdoor_age_speedup": "1 minute", // 水中でさらに多くのターンが経つと老化が進みます
  "decay_amount_factor": 5, // フィールドの年齢がさらにもう 1 ターン増加します
  "percent_spread": 10, // 各ターンにスプレッドする確率のパーセント
  "half_life": "10 minutes", // 半分がなくなるまでのターン数
  "stacking_type": "duration", // さらにフィールドを追加しますか `intensity` または `duration`
  "wandering_field": "fd_toxic_gas", // このフィールドが生成するフィールド
  "accelerated_decay": true, // このフィールドは地形の変化とともに下に下がりますか?
  // その他
  "gas_absorbption_factor": 10, // ガスマスクから食べるチャージの数
  "dirty_transparency_cache": true, // 透明度キャッシュを強制的に再計算する天気。 transparentがfalseの場合は自動で設定されます
  "description_affix": "covered in", // これをすべての強度レベル名の前に置きます
  "phase": "solid", // 固体液体ガスですか、それともプラズマですか?主にガスが伝播に影響を与える
  "is_splattering": true, // これは車両の部品に血液を塗布しますか?
  "apply_slime_factor": 2, // 因子 * 強度の香りを適用します
  "scent_neutralization": 2, // この値だけ匂いを軽減します
  // NPC 苦情を言う
  "npc_complain": {
    "chance": 20, // 苦情を言うチャンスは X 回に 1 回
    "issue": "crack_smoke", // 愚痴を言うトークタグ
    "duration": "30 minutes", // 彼らはいつまで文句を言うつもりだろうか
    "speech": "<crack_smoke>", // もう一度話したトークタグ
  },
  // モンスターの出現
  "monster_spawn_chance": 2, // X 分の 1 の確率でモンスターが出現する
  "monster_spawn_count": 3, // モンスターを何体出現させるか
  "monster_spawn_group": "GROUP_ZOMBIE", // どのモンスターグループを出現させるか
  "monster_spawn_radius": 5, // モンスターの出現場所のタイル半径
  // バッシュデータ
  "bash": {
    "str_min": 1, // バッシュに必要なバッシングダメージの下側ブラケット
    "str_max": 3, // 上位ブラケット
    "sound_vol": 2, // フィールドを叩くのに成功したときに発生するノイズ
    "sound_fail_vol": 2, // フィールドを叩くのに失敗したときに発生する騒音
    "sound": "shwip", // 成功時のサウンド
    "sound_fail": "shwomp", // 失敗時に鳴る
    "msg_success": "You brush the gum web aside.", // 成功時のメッセージ
    "move_cost": 120, // そのフィールドを正常に攻撃するのに必要な手数 (デフォルト: 100)
    "items": [ // バッシング成功時にドロップされるアイテム
      { "item": "2x4", "count": [5, 8] },
      { "item": "nail", "charges": [6, 8] },
      {
        "item": "splinter",
        "count": [3, 6],
      },
      { "item": "rag", "count": [40, 55] },
      { "item": "scrap", "count": [10, 20] },
    ],
  },
}
```
