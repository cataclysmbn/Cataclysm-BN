# サウンドパック

サウンドパックは `data/sound` ディレクトリにインストールできます。少なくとも `soundpack.txt` という名前のファイルを含むサブディレクトリでなければなりません。`sound_effect` や `playlist` を追加する JSON ファイルをいくつでも含められます。

## soundpack.txt の形式

`soundpack.txt` には NAME と VIEW の 2 つの値が必要です。NAME は一意でなければなりません。VIEW はオプションメニューに表示されます。`#` で始まる行はすべてコメントです。

```
#Basic provided soundpack
#Name of the soundpack
NAME: basic
#Viewing name of the soundpack
VIEW: Basic
```

## JSON の形式

### サウンドエフェクト

サウンドエフェクトは次の形式で記述できます。

```json
[
  {
    "type": "sound_effect",
    "id": "menu_move",
    "volume": 100,
    "files": [
      "nenadsimic_menu_selection_click.wav"
    ]
  },
  {
    "type": "sound_effect",
    "id": "fire_gun",
    "volume": 90,
    "variant": "bio_laser_gun",
    "files": [
      "guns/energy_generic/weapon_fire_laser.ogg"
    ]
  }
]
```

種類を増やすには、ある `id` の `variant` に複数の `files` を定義します。`variant` の再生時にその中からランダムに選ばれます。

`volume` キーには 0～100 の値を指定できます。

Cataclysm にはサウンドに追加で影響する、ユーザーが設定できる音量が用意されています。範囲は 0～128、既定値は 100 です。つまり既定の音量では、Cataclysm が再生するサウンドは最大値の約 78% になります。外部のオーディオエディターでサウンドを編集する場合、Cataclysm の既定音量ではエディターより小さく再生されることに注意してください。

### SFX の先読み

サウンドエフェクトは次の形式で先読みできます。

```json
[
  {
    "type": "sound_effect_preload",
    "preload": [
      { "id": "fire_gun", "variant": "all" },
      { "id": "environment", "variant": "daytime" },
      { "id": "environment" }
    ]
  }
]
```

`"variant": "all"` は特別に扱われ、指定した ID のすべてのバリアントを読み込みます。

> [!WARNING]
>
> `"variant": "all"` は最適化されていないアルゴリズム（開発者が手抜きのハックを使ったため）を使用するので、ゲームの読み込みが遅くなります。

`"variant"` を省略すると `"default"` になります。

### プレイリスト

プレイリストは次の形式で記述できます。

```json
[
  {
    "type": "playlist",
    "playlists": [
      {
        "id": "title",
        "shuffle": false,
        "files": [
          {
            "file": "Dark_Days_Ahead_demo_2.wav",
            "volume": 100
          },
          {
            "file": "cataclysmthemeREV6.wav",
            "volume": 90
          }
        ]
      }
    ]
  }
]
```

各サウンドエフェクトは ID とバリアントで識別されます。JSON ファイルに存在しないバリアントで再生しようとしても、`default` バリアントが存在すれば代わりにそれが再生されます。サウンドエフェクトのファイル名はサウンドパックのディレクトリからの相対パスです。ファイル名が `sfx.wav` で、サウンドパックが `data/sound/mypack` にあるなら、ファイルは `data/sound/mypack/sfx.wav` に置きます。

## サウンドエフェクト一覧

以下にサウンドエフェクト ID とバリアントの完全な一覧を示します。各行の形式は次のとおりです。

`id variant1|variant2`

ID はサウンドエフェクトの ID を表し、その後に `|` で区切ったバリアント一覧が続きます。バリアントを省略した場合は `default` とみなされます。バリアントがリテラル文字列ではなく変数の場合は `<` `>` で囲みます。たとえば `<furniture>` は、有効な家具 ID（家具定義 JSON と同じもの）のプレースホルダーです。

    # ドアの開閉

- `open_door default|<furniture>|<terrain>`
- `close_door default|<furniture>|<terrain>`

  # 破壊の試行と結果。特殊なもの、家具・地形固有のものを含む
- `bash default`
- `smash wall|door|door_boarded|glass|swing|web|paper_torn|metal`
- `smash_success hit_vehicle|smash_glass_contents|smash_cloth|<furniture>|<terrain>`
- `smash_fail default|<furniture>|<terrain>`

  # 近接攻撃のサウンド
- `melee_swing default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing`
- `melee_hit_flesh default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing|<weapon>`
- `melee_hit_metal default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing!<weapon>`
- `melee_hit <weapon>` # 注: 素手攻撃には武器 ID "null" を使用

  # 銃器・遠隔武器のサウンド
- `fire_gun <weapon>|brass_eject|empty`
- `fire_gun_distant <weapon>`
- `reload <weapon>`
- `bullet_hit hit_flesh|hit_wall|hit_metal|hit_glass|hit_water`

  # 環境 SFX（分かりやすいよう分類）
- `environment thunder_near|thunder_far`
- `environment daytime|nighttime`
- `environment indoors|indoors_rain|underground`
- `environment <weather_type>` # 例:
  `WEATHER_DRIZZLE|WEATHER_RAINY|WEATHER_THUNDER|WEATHER_FLURRIES|WEATHER_SNOW|WEATHER_SNOWSTORM`
- `environment alarm|church_bells|police_siren`
- `environment deafness_shock|deafness_tone_start|deafness_tone_light|deafness_tone_medium|deafness_tone_heavy`

  # その他の環境音
- `footstep default|light|clumsy|bionic`
- `explosion default|small|huge`

  # 大量のゾンビを見たときの周囲の危険テーマ
- `danger_low`
- `danger_medium`
- `danger_high`
- `danger_extreme`

  # チェーンソーパック
- `chainsaw_cord     chainsaw_on`
- `chainsaw_start    chainsaw_on`
- `chainsaw_start    chainsaw_on`
- `chainsaw_stop     chainsaw_on`
- `chainsaw_idle     chainsaw_on`
- `melee_swing_start chainsaw_on`
- `melee_swing_end   chainsaw_on`
- `melee_swing       chainsaw_on`
- `melee_hit_flesh   chainsaw_on`
- `melee_hit_metal   chainsaw_on`
- `weapon_theme      chainsaw`

  # モンスターの死亡と噛みつき攻撃
- `mon_death zombie_death|zombie_gibbed`
- `mon_bite bite_miss|bite_hit`

- `melee_attack monster_melee_hit`

- `player_laugh laugh_f|laugh_m`

  # プレイヤーの移動 SFX
  重要: `plmove <terrain>` has priority over default `plmove|walk_<what>` (excluding
  `|barefoot`) example: if `plmove|t_grass_long` is defined it will be played before default
  `plmove|walk_grass` default for all grassy terrains

- `plmove <terrain>|<vehicle_part>`
- `plmove walk_grass|walk_dirt|walk_metal|walk_water|walk_tarmac|walk_barefoot|clear_obstacle`

  # 疲労
- `plmove 疲労_m_low|疲労_m_med|疲労_m_high|疲労_f_low|疲労_f_med|疲労_f_high`

  # プレイヤーの負傷サウンド
- `deal_damage hurt_f|hurt_m`

  # プレイヤーの死亡とゲーム終了サウンド
- `clean_up_at_end game_over|death_m|death_f`

  # さまざまな生体工学サウンド
- `bionic elec_discharge|elec_crackle_low|elec_crackle_med|elec_crackle_high|elec_blast|elec_blast_muffled|acid_discharge|pixelated`
- `bionic bio_resonator|bio_hydraulics|`

  # さまざまな工具・罠の使用（関連する地形・家具を含む）
- `tool alarm_clock|jackhammer|pickaxe|oxytorch|hacksaw|axe|shovel|crowbar|boltcutters|compactor|gaspump|noise_emitter|repair_kit|camera_shutter|handcuffs`
- `tool geiger_low|geiger_medium|geiger_high`
- `trap bubble_wrap|bear_trap|snare|teleport|dissector|glass_caltrop|glass`

  # さまざまな活動
- `activity burrow`

  # 楽器。`_bad` は演奏に失敗したときに使用
- `musical_instrument <instrument>`
- `musical_instrument_bad <instrument>`

  # さまざまな叫び声と悲鳴
- `shout default|scream|scream_tortured|roar|squeak|shriek|wail|howl`

  # 発話。現在はアイテム ID またはモンスター ID、あるいは特殊な `NPC` / `NPC_loud` に紐づく
  # TODO: speech.json の完全な音声化
- `speech <item_id>` # 例: talking_doll, creepy_doll, Granade,
- `speech <monster_id>` # 例: eyebot, minitank, mi-go, many robots
- `speech NPC_m|NPC_f|NPC_m_loud|NPC_f_loud` # NPC 用の特殊形式
- `speech robot` # 機械などのロボット音声用の特殊形式

  # ラジオの会話
- `radio static|inaudible_chatter`

  # さまざまな発生源の唸り音
- `humming electric|machinery`

  # （燃えている）火に関するサウンド
- `fire ignition`

  # 車両のサウンド - エンジンなどの部品の動作
  # 注: 特定のオプションが未定義の場合は既定値が実行される
- `engine_start <vehicle_part>` # note: specific engine start (id of any
  engine/motor/steam_engine/paddle/oar/sail/etc. )
- `engine_start combustion|electric|muscle|wind` # default engine starts グループ
- `engine_stop <vehicle_part>` # note: specific engine stop (id of any
  engine/motor/steam_engine/paddle/oar/sail/etc. )
- `engine_stop combustion|electric|muscle|wind` # default engine stop グループ

  # 注: 車内エンジン音のピッチは車両速度に応じて動的に変化する
  # 専用チャンネルを使う環境ループサウンド
- `engine_working_internal <vehicle_part>` # 注: 車内で聞こえるエンジン動作音
- `engine_working_internal combustion|electric|muscle|wind` # default engine working (inside) グループ

  # 注: 車外エンジン音の音量とパンは車両までの距離と角度に応じて動的に変化する
  # 指定距離で聞こえる音量はエンジンの `noise_factor` とエンジンへの負荷に関連する（`vehicle::noise_and_smoke()` 参照）
  # 専用チャンネルを使う環境ループサウンド
  # 単一チャンネルの実装（TODO: 聞こえる車両ごとのマルチチャンネル）。最も大きく聞こえる車両を選ぶ
  # ここではピッチを変更しない（必要になれば導入する）
- `engine_working_external <vehicle_part>` # 注: 車外で聞こえるエンジン動作音
- `engine_working_external combustion|electric|muscle|wind` # default engine working (outside)
  グループ

  # 注: gear_up/gear_down はピッチ操作で自動的に行われる
  # ギアシフトは最高安全速度に依存し、次の前提で動作する:
  # 前進ギア 6 段、ギア 0 = ニュートラル、ギア -1 = リバース
- `vehicle gear_shift`

- `vehicle engine_backfire|engine_bangs_start|fault_immobiliser_beep|engine_single_click_fail|engine_multi_click_fail|engine_stutter_fail|engine_clanking_fail`
- `vehicle horn_loud|horn_medium|horn_low|rear_beeper|chimes|car_alarm`
- `vehicle reaper|scoop|scoop_thump`

- `vehicle_open <vehicle_part>` # note: ドア、トランク、ハッチなどの ID
- `vehicle_close <vehicle part>`

  # その他のサウンド
- `misc flashbang|flash|shockwave|earthquake|stairs_movement|stones_grinding|bomb_ticking|lit_fuse|cow_bell|bell|timber`
- `misc rc_car_hits_obstacle|rc_car_drives`
- `misc default|whistle|airhorn|horn_bicycle|servomotor`
- `misc beep|ding|`
- `misc rattling|spitting|coughing|heartbeat|puff|inhale|exhale|insect_wings|snake_hiss` # 主に
  生物の音
