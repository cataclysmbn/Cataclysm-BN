# 外部タイルセット

`data/json/external_tileset`は、Bright Nights独自のコンテンツのうち、UDPやUlticaといった主要タイルセットに未実装のものを扱うために使用されます。これは、既存の `looks_like` (指定した別のアイテムの外見を流用する機能) では、内容を十分に表現できない場合に利用されます。機能としては `mod_tileset` の仕組みを利用しており、指定したタイルセットに対して新しいスプライトの追加や上書き適用を行います。

この手法の主な利点は、スプライトを切り分けて管理することで、主要タイルセット自体のアップデート(タイル作者のリポジトリに基づく更新)との干渉を防げる点にあります。これにより、タイルセットの更新を取り込んだ際に、BN独自の修正や追加スプライトが誤って上書き・削除されることを防げます。また、ゲームバランスの都合上、BNでは別のスプライトを適用した方が適切であるといった「コンテンツの乖離」が生じた際にも対応可能です。

以下に記載されている各コンテンツの導入・変更に関連するプルリクエストへのリンクです:

- DDAからのパッチワーク・スキンの移植: [#1298](https://github.com/cataclysmbn/Cataclysm-BN/pull/1298)
- 弾丸のアニメーション演出: [#1861](https://github.com/cataclysmbn/Cataclysm-BN/pull/1681)
- 蒸気タービン: [#2815](https://github.com/cataclysmbn/Cataclysm-BN/pull/2815)
- 木製シールド、ライオットシールド、バリスティックシールド:
  [#2851](https://github.com/cataclysmbn/Cataclysm-BN/pull/2851)
- 鉛の投石弾: [#2709](https://github.com/cataclysmbn/Cataclysm-BN/pull/2709)
- レザーシールド、バンドシールド: [#2856](https://github.com/cataclysmbn/Cataclysm-BN/pull/2856)
- ブルーベリー/イチゴの茂み、チェリーの木の調整:
  [#2861](https://github.com/cataclysmbn/Cataclysm-BN/pull/2861)
- 木製大弓の再実装、コンパウンド/コンポジットグレートボウのフレーバー変更:
  [#2862](https://github.com/cataclysmbn/Cataclysm-BN/pull/2862)
- カタール、サイ: [#2715](https://github.com/cataclysmbn/Cataclysm-BN/pull/2715)
- ユーティリティライトの消灯機能:
  [#1003](https://github.com/cataclysmbn/Cataclysm-BN/pull/1003)
- ミ＝ゴの神経クラスター: [#1962](https://github.com/cataclysmbn/Cataclysm-BN/pull/1962)
- ウロコグマ: [#1371](https://github.com/cataclysmbn/Cataclysm-BN/pull/1371)
- バックラー、溶接シールド: [#2878](https://github.com/cataclysmbn/Cataclysm-BN/pull/2878)
- バトルマスク、ブロンズアームガード:
  [#3221](https://github.com/cataclysmbn/Cataclysm-BN/pull/3221)
- 配線し直された街灯: [#3273](https://github.com/cataclysmbn/Cataclysm-BN/pull/3273)
- 耳/尻尾の変異用代替スプライト: [#3340](https://github.com/cataclysmbn/Cataclysm-BN/pull/3340)
- 沼鉄鉱からの鉄鉱石精錬: [#3506](https://github.com/cataclysmbn/Cataclysm-BN/pull/3506)
- 新しい樹木: [#3626](https://github.com/cataclysmbn/Cataclysm-BN/pull/3626)
- 看板の代替スプライト: [#3670](https://github.com/cataclysmbn/Cataclysm-BN/pull/3670)
- M1874 ガトリング砲: [#3815](https://github.com/cataclysmbn/Cataclysm-BN/pull/3815)
- 新しいトラップ: [#3939](https://github.com/cataclysmbn/Cataclysm-BN/pull/3939)
- 新しいモンスター: [#4182](https://github.com/cataclysmbn/Cataclysm-BN/pull/4182)
- 家具版ユーティリティライト:
  [#4780](https://github.com/cataclysmbn/Cataclysm-BN/pull/4780)
- 倒れた状態のスチールターゲット:
  [#5361](https://github.com/cataclysmbn/Cataclysm-BN/pull/5361)
- フラッグポール: [#5363](https://github.com/cataclysmbn/Cataclysm-BN/pull/5363)
- 車両搭載用フラッグ: [#5372](https://github.com/cataclysmbn/Cataclysm-BN/pull/5372)
- 海賊旗: [#5375](https://github.com/cataclysmbn/Cataclysm-BN/pull/5375)
- 簡易大砲、散弾筒:
  [#5398](https://github.com/cataclysmbn/Cataclysm-BN/pull/5398)
- 収穫済みのガマ: [#5445](https://github.com/cataclysmbn/Cataclysm-BN/pull/5445)
- 硝石床: [#5446](https://github.com/cataclysmbn/Cataclysm-BN/pull/5446)
- スケールスキンアーマー、牙や骨の武器:
  [#5466](https://github.com/cataclysmbn/Cataclysm-BN/pull/5466)
- ボーンアーマーの再実装: [#5646](https://github.com/cataclysmbn/Cataclysm-BN/pull/5646)
- 骨/牙の武器の追加、トリフィドの武器:
  [#5712](https://github.com/cataclysmbn/Cataclysm-BN/pull/5712)
- DDAからの樹冠の移植: [#5167](https://github.com/cataclysmbn/Cataclysm-BN/pull/5167)
- DDAからのスケートボードの移植: [#5849](hhttps://github.com/cataclysmbn/Cataclysm-BN/pull/5849)
- ドアを溶接して封鎖する機能: [#6182](https://github.com/cataclysmbn/Cataclysm-BN/pull/6182)
- スカウトバイザー: [#6687](https://github.com/cataclysmbn/Cataclysm-BN/pull/6687)
- DDAからの骸骨リッチと骸骨マスターの移植: [#6831](https://github.com/cataclysmbn/Cataclysm-BN/pull/6831)
- 新しい上位ゾンビ: [#6854](https://github.com/cataclysmbn/Cataclysm-BN/pull/6854)
- テーブル/ベンチを倒して遮蔽物にする機能: [#6857](https://github.com/cataclysmbn/Cataclysm-BN/pull/6857)
- 銃眼/要塞化地形: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6864)
- アドビの床: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6885)
- 湖底コンテンツ: [#6903](https://github.com/cataclysmbn/Cataclysm-BN/pull/6903)

## Undead People

以下は、本フォルダによってUDPタイルセットに追加されるスプライトの最新リストです。どのファイルに、どのような目的で追加されたかを記載しています。現在、外部タイルセット(external_tileset)が用意されているのはUDPのみですが、将来的にはUltica用スプライトの追加も計画されています。

### External_Tileset_DP_Normal.png

- 蒸気タービン(設置物版):「高性能蒸気エンジン」へ分解するための設置物。BN独自のコンテンツ。
- 弾丸の飛行エフェクト: 飛行中の弾丸のアニメーション効果。BN独自の機能。
- 木製シールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- ライオットシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- バリスティックシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- 鉛の投石弾: BN独自のアイテム。looks_likeに石を指定するのは不適切と判断したため追加。
- レザーシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- 大型レザーシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- ブロンズシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- 大型バンドシールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- `木製グレートボウ`の装備中スプライト修正: UDP本家版にスプライトエラーがあったため。
  `注意: この修正は本家リポジトリでも公開済みのため、BN側のUDPが更新されれば削除可能です。`
- `コンポジットグレートボウ`の上書きスプライト: BN版はコンパウンドボウではなく「コンポジットボウ」
  をモデルとしているため。
- `コンポジットグレートボウ`の新規装備中スプライト: DDAのコンパウンドグレートボウは装備(背負うこ
  と)ができないため、UDPには装備中スプライトが存在しませんでした(いずれにせよコンポジットボウ風に編集する必要がありました)。
- カタール: 手持ち状態のスプライトを含む。BN独自のアイテム。
- サイ:(通信施設のSAIではなく武器) 手持ち状態のスプライトを含む。BN独自のアイテム。
- 溶接シールド: 装備中および手持ちの状態を含む。BN独自のアイテム。
- バックラー: 装備中および手持ちの状態を含む。BN独自のアイテム。
- バトルマスク(鉄・青銅): 装備中スプライトを含む。BN独自のアイテム。
- ブロンズアームガード: 装備中スプライトを含む。BN独自のアイテム。
- カカオのポッド: BN独自のアイテム。
- 看板スプライトの上書き: 前面にあるDDA固有の文字表記を削除。
- 急造監視アラーム: BN独自のトラップ。
- レピドプテリッド: BN独自の新モンスター。
- 設置物版ユーティリティライト: BN独自の設置物
- 倒れたスチールターゲット: BN独自の設置物。
- ジョリー・ロジャー: アイテムおよび装備中スプライト。BN独自のアイテム。
- 簡易キャノン: アイテムおよび車両パーツ用スプライト。BN独自のアイテム。
- 炸裂弾および装填済みキャノン砲弾のスプライト: BN独自のアイテム。
- ガマ(収穫状態): 冬季バリエーションを含む。BN独自の設置物。
- 爬虫類のスケイルアーマー、重い骨・牙・虫の刺で作られた武器: BN独自のアイテム。
- ボーンキュイラスおよびボーングリーブ: 過去に削除されたボーンアーマーをBNで再実装。
- 牙や刺の武器(追加分)、トリフィドの刺の武器: BN独自のアイテム。
- スカウトバイザー: BN独自のアイテム。
- 死体収集ゾンビ、ゾンビ軍曹、バンシー、ストームブリンガー、帳織り、ショックボルテックス: BN独自の
  新モンスター。
- ベンチとテーブルの反転バリエーション: BN独自の設置物。
- 鉄鉱石: DDAから移植されたアイテムですが、適切なスプライトがなかったため追加。
- M1874 ガトリング砲: 手持ちスプライトおよび車両銃座を含む。BN独自のアイテム。
- パッチワーク・スキン: DDAからの移植。UDP本家版にスプライトがなかったため追加。
- スケートボード: アイテムおよび車両パーツ。UDPにスプライトが欠けていたDDAからの移植。
- シースクーター: アイテムおよび車両パーツ。BN独自のアイテム。
- 骸骨リッチおよび骸骨マスター: UDPにスプライトがなかったDDAからの移植。

### External_Tileset_DP_terrain_normal.png

- 水田: BN独自の地形。
- 硝石床: BN独自の地形。
- ブルーベリーの茂み: BNでは収穫時期が変更されたため、春と夏のスプライトを入れ替え(夏のスプライト
  に未熟な実が描かれていなかったため)。
- 溶接された金属ドア、覗き穴付きドア、隔壁、研究所用ドア、鉄格子ドア。 BN独自の地形。
- 銃眼(壁への加工): 木製、丸太、石造り、レンガ造りの壁に刻まれた防衛用の隙間。BN独自の地形。
- アドビの床: BN独自の地形。
- 湖底の苔と倒木: 湖底の表面に生成される地形。BN独自の地形。

### External_Tileset_DP_Tall.png

- ユーティリティライト(オフ状態): ライトのON/OFF切り替え機能。BN独自の仕様。
- エイリアンの神経クラスター: ミ＝ゴの拠点に追加された設置物。
  `注意: この修正は本家リポジトリでも公開済みのため、BN側のUDPが更新されれば削除可能です。`
- ウロコグマ: 死体スプライトを含む。BN独自のモンスター。
- チェリー: BNでの収穫時期変更に伴い、実のない夏スプライト(および桜色の色調)を適用。
- 配線し直された街灯: 稼働状態のスプライトを含む。BN独自の家具。
- ココアの木: BN独自の地形。
- コカの植物: BN独自の地形。
- エリア照明灯: 分解可能になったユーティリティライトをベースにした設置物。BNへの追加要素。
- フラッグポール(金属・木製): 旗を掲げた状態を含む。BN独自の設置物。
- 車両用星条旗: 車両パーツとして表示される旗。BN独自の仕様。
- ジョリー・ロジャー: 家具としての旗竿および車両パーツ用スプライト。BN独自のコンテンツ。
- 樹冠: DDAから移植されましたが、現時点のUDPにはスプライトが存在しなかった地形。

### alternative_mutation_tileset.png

![](../../../../../../../assets/img/external_tileset/mutation/after.png)

<details><summary>Before</summary>

![](../../../../../../../assets/img/external_tileset/mutation/before.png)

</details>

以下の部位に対する代替スプライトが含まれています:

- `FELINE_EARS`
- `LUPINE_EARS`
- `MOUSE_EARS`
- `CANINE_EARS`
- `TAIL_FLUFFY`
- `TAIL_STUB`
