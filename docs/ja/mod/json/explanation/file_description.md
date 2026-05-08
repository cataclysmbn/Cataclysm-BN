# ファイル記述

ここでは、各JSONファイルが含む内容をフォルダ別に概説します。このリストは網羅的ではありませんが、広範な内容をカバーしています。

## `data/json/`

| ファイル名                 | 説明                                                             |
| -------------------------- | ---------------------------------------------------------------- |
| achievements.json          | アチーブメント（実績）の定義。                                   |
| anatomy.json               | プレイヤーの身体部位のリスト。編集不可。                         |
| ascii_arts.json            | アイテムの説明に使用されるアスキーアート。                       |
| bionics.json               | バイオニック。ただし、バイオニックの効果は含まない。             |
| body_parts.json            | anatomy.jsonの拡張。編集不可。                                   |
| clothing_mods.json         | 衣類MODの定義。                                                  |
| construction.json          | 建設メニューのタスク定義。                                       |
| default_blacklist.json     | ジョークモンスターの標準的なブラックリスト。                     |
| doll_speech.json           | お喋り人形の会話メッセージ。                                     |
| dreams.json                | 夢のテキストとリンクされた変異カテゴリ。                         |
| disease.json               | 病気の定義。                                                     |
| effects.json               | 一般的なエフェクトとその効果。                                   |
| emit.json                  | 煙やガスの放出の定義。                                           |
| flags.json                 | 一般的なフラグとその記述。                                       |
| furniture.json             | 家具および備品として扱われる設置物。                             |
| game_balance.json          | ゲームバランスを微調整するための各種オプション。                 |
| gates.json                 | ゲート地形の定義。                                               |
| harvest.json               | 死体を解体した際のアイテムドロップ。                             |
| health_msgs.json           | プレイヤーが目覚めた際に表示されるメッセージ。                   |
| item_actions.json          | 標準的なアイテムアクションの記述。                               |
| item_category.json         | アイテムカテゴリとそれらのデフォルトのソート順。                 |
| item_groups.json           | アイテムの出現グループ。                                         |
| lab_notes.json             | 研究所のコンピュータのメッセージ。                               |
| martialarts.json           | 格闘術スタイルとバフ効果。                                       |
| materials.json             | 素材のタイプ。                                                   |
| monster_attacks.json       | モンスターの攻撃。                                               |
| monster_drops.json         | モンスターが死亡時にドロップするアイテム。                       |
| monster_factions.json      | モンスターの派閥。                                               |
| monstergroups.json         | モンスターの出現グループ。                                       |
| monstergroups_egg.json     | 卵から出現するモンスターグループ。                               |
| monsters.json              | モンスターの記述(主にゾンビ)。                                   |
| morale_types.json          | 士気に関する修正メッセージ。                                     |
| mutation_category.json     | 変異カテゴリに関するメッセージ。                                 |
| mutation_ordering.json     | タイルモードにおける変異およびCBMオーバーレイの描画順序。        |
| mutations.json             | 特性、変異                                                       |
| names.json                 | NPC、プレイヤー名の生成に使用される名前。                        |
| overmap_connections.json   | オーバーマップにおける道路やトンネルの接続。                     |
| overmap_terrain.json       | オーバーマップ地形の定義。                                       |
| player_activities.json     | プレイヤーの行動。                                               |
| professions.json           | 職業の定義。                                                     |
| recipes.json               | 製作および分解レシピ。                                           |
| regional_map_settings.json | マップ生成全体の設定。                                           |
| road_vehicles.json         | 道路での車両生成情報。                                           |
| rotatable_symbols.json     | 回転可能なシンボル。編集不可。                                   |
| scent_types.json           | 利用可能な匂いのタイプ。                                         |
| scores.json                | スコア。                                                         |
| skills.json                | スキルの記述とID。                                               |
| snippets.json              | チラシ/ポスターの記述。メッセージはここに定義。                  |
| species.json               | モンスターの種族                                                 |
| speech.json                | モンスターの音声表現                                             |
| statistics.json            | スコアとアチーブメントを定義するために使用される統計情報と変換。 |
| start_locations.json       | シナリオの開始場所。                                             |
| techniques.json            | アイテムと格闘術のための汎用テクニック。                         |
| terrain.json               | 地形のタイプと定義。                                             |
| test_regions.json          | テストリージョン。                                               |
| tips.json                  | 今日のヒント。                                                   |
| tool_qualities.json        | 標準的なツールの品質とそのアクション。                           |
| traps.json                 | 標準的な罠。                                                     |
| tutorial.json              | チュートリアル（古い）のメッセージ。                             |
| vehicle_groups.json        | 車両の出現グループ。                                             |
| vehicle_parts.json         | 車両部品。フラグ効果には影響しない。                             |
| vitamin.json               | ビタミンとその欠乏症。                                           |

サブフォルダ

## `data/json/items/`

さまざまなアイテムの詳細については以下を参照してください。

| ファイル名                   | 説明                                                               |
| ---------------------------- | ------------------------------------------------------------------ |
| ammo.json                    | バッテリーやビー玉などの一般的な基本コンポーネント。               |
| ammo_types.json              | 銃器別の標準的な弾薬タイプ。                                       |
| archery.json                 | 弓と矢。                                                           |
| armor.json                   | アーマーと衣類。                                                   |
| bionics.json                 | コンパクト・バイオニック・モジュール(CBM)。                        |
| biosignatures.json           | 動物の排泄物。                                                     |
| books.json                   | 書籍。                                                             |
| chemicals_and_resources.json | 化学物質と資源。                                                   |
| comestibles.json             | 食料/飲料 (消耗品)。                                               |
| containers.json              | コンテナ (容器)。                                                  |
| crossbows.json               | クロスボウとボルト。                                               |
| fake.json                    | バイオニックや変異のための偽のアイテム。                           |
| fuel.json                    | 液体燃料。                                                         |
| grenades.json                | 手榴弾と投擲可能な爆発物。                                         |
| handloaded_bullets.json      | ランダムな弾薬。                                                   |
| melee.json                   | 他のアイテムJSONに含まれないすべてのもの、主に近接武器。           |
| migration.json               | 存在しないアイテムをセーブゲームから現在のアイテムへ変換するもの。 |
| newspaper.json               | チラシ、新聞、サバイバーのメモ。メッセージはsnippets.jsonを参照。  |
| obsolete.json                | ゲームから削除されつつあるアイテム。                               |
| ranged.json                  | 銃器                                                               |
| software.json                | SDカードおよびUSBスティック用のソフトウェア。                      |
| tool_armor.json              | 使用できる衣類とアーマー。                                         |
| toolmod.json                 | 道具の改造。                                                       |
| tools.json                   | 使用できる道具およびアイテム。                                     |
| vehicle_parts.json           | 車両に取り付けられていない場合の車両の部品。                       |

### `data/json/items/comestibles`

## `data/json/requirements/`

製作のための標準的なコンポーネントと道具:

| ファイル名                | 説明                             |
| ------------------------- | -------------------------------- |
| ammo.json                 | 弾薬コンポーネント。             |
| cooking_components.json   | 一般的な材料セット。             |
| cooking_requirements.json | 調理器具と熱源。                 |
| materials.json            | 糸、布地、その他の基本的な素材。 |
| toolsets.json             | 一緒に使用される道具類のセット。 |
| uncraft.json              | アイテムの分解結果。             |
| vehicle.json              | 車両作業用のツール。             |

## `data/json/vehicles/`

自己説明的なファイル名を持つ車両定義のグループ:

| ファイル名           |
| -------------------- |
| bikes.json           |
| boats.json           |
| cars.json            |
| carts.json           |
| custom_vehicles.json |
| emergency.json       |
| farm.json            |
| helicopters.json     |
| military.json        |
| trains.json          |
| trucks.json          |
| utility.json         |
| vans_busses.json     |
| vehicles.json        |
