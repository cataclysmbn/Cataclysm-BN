# MOD制作ガイド

> [!CAUTION]
>
> #### 一部のドキュメントは未完成です
>
> ドキュメントの一部は不完全であるか、古いドキュメントから正しく更新されていません。修正への貢献を歓迎します。

ゲームの一部の機能は、ソースコードからゲームを再ビルドせずに変更できます。職業、モンスター、NPC などが対象です。該当するファイルを変更してゲームを実行し、変更を確認してください。

MOD制作の大部分は JSON ファイルの編集で行います。すべての JSON ファイルと対応するフィールドの詳しい説明は [JSON_INFO](./../reference/json_info.md) を参照してください。

## 基本

### 最小限のMODを作成する

Cataclysm の `data/mods` ディレクトリ内にフォルダを作成すると MOD になります。MOD の属性は、そのフォルダにある `modinfo.json` ファイルで設定します。Cataclysm に MOD と認識させるには、フォルダ内に `modinfo.json` ファイルが**必ず**必要です。

<!--厳密には正しくありません。MOD_INFO 構造を持つ JSON ファイルがあれば動作し、ファイル名が modinfo.json である必要はありません-->

### Modinfo.json

modinfo.json は MOD のメタデータを含むファイルです。Cataclysm が MOD を見つけるには、すべての MOD に `modinfo.json` ファイルが必要です。最小限の `modinfo.json` は次のようになります:

```json
[
  {
    "type": "MOD_INFO",
    "id": "Mod_ID",
    "name": "Mod's Display Name",
    "authors": ["Your name here", "Your friend's name if you want"],
    "description": "Your description here",
    "category": "content",
    "dependencies": ["bn"]
  }
]
```

`category` 属性は、MOD が MOD 選択メニューのどこに表示されるかを指定します。以下は選択可能なカテゴリと、この文書の作成時点で存在した MOD の例です。modinfo ファイルを書くときは、最も適切なものを選んでください。

- `content` - 多くの要素を追加する MOD。通常は大型 MOD やゲーム全体の改変用（例: Core game files、Aftershock）
- `items` - 新しいアイテムやレシピを追加する MOD（例: More survival tools）
- `creatures` - 新しい生物や NPC を追加する MOD（例: Modular turrets）
- `misc_additions` - その他のコンテンツを追加する MOD（例: Alternative map key、Crazy cataclysm）
- `buildings` - 新しいオーバーマップの場所や建物（例: Fuji's more buildings）。建物の生成を禁止する場合にも使用できます（例: No rail stations）。
- `vehicles` - 新しい車両や車両部品（例: Tanks and other vehicles）
- `rebalance` - ゲームのバランスを変更する MOD（例: Safe autodocs）
- `magical` - 魔法関連の要素を追加する MOD（例: Necromancy）
- `item_exclude` - アイテムが世界に生成されないようにする MOD（例: No survivor armor、No drugs）
- `monster_exclude` - 特定のモンスター種が生成されないようにする MOD（例: No fungal monsters、No ants）
- `graphical` - ゲームのグラフィックを変更する MOD（例: Graphical overmap）

`dependencies` 属性は、MOD が別の MOD に依存していることを Cataclysm に伝えます。コアゲーム以外に依存しない場合は、リストに `bn` を含めれば十分です。別の MOD が必要な場合、その MOD の `id` 属性を配列に追加すると、Cataclysm はそれを先に読み込みます。

`MOD_INFO` オブジェクトの詳細は [MOD_INFO](/mod/json/reference/mod_info/) を参照してください。

## MODに要素を追加する

基本的な MOD ができたら、実際に要素を追加できます。

### ファイル構造

追加する要素のカテゴリごとに別の JSON ファイルへ分けるとよいでしょう。MOD フォルダまたはそのサブフォルダにある JSON ファイルはすべて Cataclysm に検出・読み込みされますが、それ以外に配置の制限はありません。

### JSON_INFO.md

この MOD で可能なことを網羅した一覧として、[JSON_INFO](./../reference/json_info.md) を読む価値があります。この文書の残りではコピーして使える例をいくつか示しますが、網羅的ではありません。コアゲームのデータも MOD と同じ方法で定義されているため、ゲームの JSON ファイル（`data/json` 内）を読むことも大きな助けになります。ゲームワールドを読み込むとき JSON 構文の問題が見つかるとエラーメッセージが表示され、修正するまでそのワールドを読み込めません。

### シナリオを追加する

シナリオは、キャラクター作成時の大まかな状況を決めます。キャラクターが世界のどこにいつ出現できるか、どの職業を使えるかを決定し、開始時に職業が変異を持てるかどうかも決定します。以下はゲーム内蔵の `Large Building` シナリオの JSON 定義です。

```json
[
  {
    "type": "scenario",
    "id": "largebuilding",
    "name": "Large Building",
    "points": -2,
    "description": "Whether due to stubbornness, ignorance, or just plain bad luck, you missed the evacuation, and are stuck in a large building full of the risen dead.",
    "allowed_locs": [
      "mall_a_12",
      "mall_a_30",
      "apartments_con_tower_114",
      "apartments_con_tower_014",
      "apartments_con_tower_104",
      "apartments_con_tower_004",
      "hospital_1",
      "hospital_2",
      "hospital_3",
      "hospital_4",
      "hospital_5",
      "hospital_6",
      "hospital_7",
      "hospital_8",
      "hospital_9"
    ],
    "start_name": "In Large Building",
    "surround_groups": [["GROUP_BLACK_ROAD", 70.0]],
    "flags": ["CITY_START", "LONE_START"]
  }
]
```

### 職業を追加する

職業はゲーム開始時に選択できるキャラクタークラスです。職業には特性、スキル、アイテム、ペットまで設定できます。以下は Police Officer 職業の定義です。

```json
[
  {
    "type": "profession",
    "id": "cop",
    "name": "Police Officer",
    "description": "Just a small-town deputy when you got the call, you were still ready to come to the rescue.  Except that soon it was you who needed rescuing - you were lucky to escape with your life.  Who's going to respect your authority when the government this badge represents might not even exist anymore?",
    "points": 2,
    "skills": [{ "level": 3, "name": "gun" }, { "level": 3, "name": "pistol" }],
    "traits": ["PROF_POLICE"],
    "items": {
      "both": {
        "items": [
          "pants_army",
          "socks",
          "badge_deputy",
          "sheriffshirt",
          "police_belt",
          "smart_phone",
          "boots",
          "whistle",
          "wristwatch"
        ],
        "entries": [
          { "group": "charged_two_way_radio" },
          { "item": "ear_plugs", "custom-flags": ["no_auto_equip"] },
          { "item": "usp_45", "ammo-item": "45_acp", "charges": 12, "container-item": "holster" },
          { "item": "legpouch_large", "contents-group": "army_mags_usp45" }
        ]
      },
      "male": ["boxer_shorts"],
      "female": ["bra", "boy_shorts"]
    }
  }
]
```

### アイテムを追加する

アイテムについては [JSON_INFO](./../reference/json_info.md) を読むことを強く勧めます。できることが非常に多く、アイテムのカテゴリごとに少しずつ異なるためです。

<!--できるだけ基本的なアイテムを選びました。他のアイテムは何らかの機能を持っています。-->

```json
[
  {
    "id": "family_photo",
    "type": "GENERIC",
    "//": "Unique mission item for the CITY_COP.",
    "category": "other",
    "name": "family photo",
    "description": "A photo of a smiling family on a camping trip.  One of the parents looks like a cleaner, happier version of the person you know.",
    "weight": "1 g",
    "volume": 0,
    "price": 800,
    "material": ["paper"],
    "symbol": "*",
    "color": "light_gray"
  }
]
```

### モンスターの生成を防ぐ

これは比較的簡単ですが、とても便利な種類の MOD です。世界に特定のモンスターを出現させたくない場合に使います。モンスターグループ全体をブラックリストにする方法と、個別のモンスターをブラックリストにする方法があります。どちらにもモンスターの ID が必要です。コアゲームでは `data/json/monsters` ディレクトリにあります。以下の例は `No Ants` MOD のもので、ゲーム内にあらゆる種類のアリが生成されないようにします。

```json
[
  {
    "type": "MONSTER_BLACKLIST",
    "categories": ["GROUP_ANT", "GROUP_ANT_ACID"]
  },
  {
    "type": "MONSTER_BLACKLIST",
    "monsters": [
      "mon_ant_acid_larva",
      "mon_ant_acid_soldier",
      "mon_ant_acid_queen",
      "mon_ant_larva",
      "mon_ant_soldier",
      "mon_ant_queen",
      "mon_ant_acid",
      "mon_ant"
    ]
  }
]
```

### ロケーションの生成を防ぐ

ゲーム内で特定のロケーションの生成を防ぐ方法は、対象の種類によって少し複雑です。オーバーマップ建物には通常の建物とオーバーマップスペシャルがあります。特定のフラグを持つものを禁止したい場合は、モンスターと同じような方法でブラックリストにできます。以下も `No Ants` MOD の例で、アリ塚の生成を止めます。

```json
[
  {
    "type": "region_overlay",
    "regions": ["all"],
    "overmap_feature_flag_settings": { "blacklist": ["ANT"] }
  }
]
```

禁止したい場所がオーバーマップスペシャルの場合は、その定義をコピーして `occurrences` 属性を `[ 0, 0 ]` に手動で設定する必要があるでしょう。

都市内に生成されるものを禁止したい場合は、リージョンオーバーレイを使います。以下は `No rail stations` MOD の例で、都市内に鉄道駅が生成されないようにします。ただし、鉄道駅のオーバーマップスペシャルは生成されます。

```json
[
  {
    "type": "region_overlay",
    "regions": ["all"],
    "city": { "houses": { "railroad_city": 0 } }
  }
]
```

### 特定のシナリオを無効化する

`SCENARIO_BLACKLIST` はブラックリストにもホワイトリストにもできます。ホワイトリストにすると、指定したもの以外のすべてのシナリオがブラックリストになります。1つのゲームで（一つの MOD だけでなく、すべての MOD と基本ゲームで）同時に指定できるブラックリストは1つだけです。形式は次のとおりです。

```json
[
  {
    "type": "SCENARIO_BLACKLIST",
    "subtype": "whitelist",
    "scenarios": ["largebuilding"]
  }
]
```

`subtype` の有効な値は `whitelist` と `blacklist` です。`scenarios` はブラックリストまたはホワイトリストにするシナリオ ID の配列です。

### モンスターを調整する

```json
[
  {
    "type": "monster_adjustment",
    "species": "ZOMBIE",
    "flag": { "name": "REVIVES", "value": false },
    // ここでは任意のフラグを使えます。
    "stat": { "name": "speed", "modifier": 10 },
    // stat で指定できる名前は speed と HP だけです。
    "special": "nightvision",
    // nightvision はこの種類のすべてのモンスターに暗視を与えます。
    // no_zombify はモンスターの "zombify_into" エントリを削除します。
  }
```

## JSONファイルに関する重要な注意

JSON ファイルを追加・変更するとき、`[ { , } ] : "` という文字は非常に重要です。`,` や `[`、`}` が1つ欠けるだけで、正常に動作するファイルが起動時にゲームを停止させるファイルになり得ます。アイテムの説明に引用符を入れるなど、これらの文字を含めたい場合は、該当する文字の前にバックスラッシュを置いてエスケープします。

```json
...
"description": "This is a shirt that says \"I wanna kill ALL the zombies\" on the front.",
...
```

ゲーム中には次のように表示されます:
`This is a shirt that says "I wanna kill ALL the zombies" on the front.`

多くのエディタには `{ [` と `] }` の対応を確認する機能があります。この機能はエスケープされた文字も正しく扱います。[Notepad++](https://notepad-plus-plus.org/) はこの機能を備えた Windows 向けの無料エディタです。Linux には多数の選択肢があります。

オンラインまたはオフラインの JSON バリデータも使用できます。例として <https://dev.narc.ro/cataclysm/format.html> や、リリースに同梱される `json_formatter`（JSON ファイルをドラッグ＆ドロップします）があります。

## 追記

<!-- ここに置くべきかよく分かりません。意見を聞かせてください。 -->
