# JSONの継承

JSONデータの重複を減らすため、一部の「タイプ(type)」では既存の定義からプロパティを継承することが可能です。

## プロパティ

### `abstract`

抽象アイテムを作成します。これはゲーム内には登場せず、JSON内でのみ「コピー元(テンプレート)」として存在するアイテムです。この場合、`"id"` の代わりにこれを使用します。

### `copy-from`

プロパティのコピー元にしたいアイテムの識別子(ID)を指定します。これにより、同じタイプ(type)のアイテムの完全なコピーを作成した上で、コピー元から変更したい箇所だけを記述することが可能になります。

新しく定義するアイテムに、`copy-from` で指定した対象と同じ `"id"` を与えた場合、元のアイテムを上書き(置換)します。

### `extends`

Mod制作者は、定義に `"extend"` フィールドを追加することで、リスト全体を上書きするのではなく、既存のリストに新しい項目を追記することができます。

### `delete`

Mod制作者は `"delete"` フィールドを追加することもできます。これを使用すると、リスト全体を上書きするのではなく、既存のリストから特定の要素を削除することができます。

## 例

以下の簡略化された例では、`copy-from` を使用して、`556` 弾(5.56mm NATO弾)を `223` 弾(.223レミントン弾)から派生させています:

```json
"id": "556",
"copy-from": "223",
"type": "AMMO",
"name": "5.56 NATO M855A1",
"description": "5.56x45mm ammunition with a 62gr FMJ bullet...",
"price": 3500,
"relative": {
    "damage": -2,
    "pierce": 4,
},
"extend": { "effects": [ "NEVER_MISFIRES" ] }
```

上記の例には以下のルールが適用されます:

- 記述のないフィールド: 親と同じ値を保持します。

- 明示的に指定されたフィールド: 親タイプの値を上書きします。上記の例では、`name`(名称)、`description`(説明)、`price`(価格)が置き換えられています。

- 数値データは、親に対する『相対値』で指定することができます。例えば、例にある `556` 弾は `223` 弾よりも『`ダメージ`(damage)』は低いですが、『`貫通力`(pierce)』は高くなっています。もし親である `223` 弾の定義が変更されたとしても、この(数値の差の)関係性は維持されます。

- フラグは `extend` を通じて追加できます。 例えば、`556` 弾は軍用弾薬であるため、`NEVER_MISFIRES`(不発なし)という弾薬効果が付与されます。この際、`223` 弾から継承された既存のフラグはすべて保持されます。

- コピー元のエントリは、追加または変更しようとしているアイテムと同じ「`タイプ`(type)」でなければなりません。 (すべてのタイプがサポートされているわけではありません。後述の「サポート」セクションを参照してください)

再装填弾は、工場生産の同等品から派生していますが、「ダメージ」と「精度誤差」に10%のペナルティがあり、さらに不発の可能性があります。:

```json
"id": "reloaded_556",
"copy-from": "556",
"type": "AMMO",
"name": "reloaded 5.56 NATO",
"proportional": {
    "damage": 0.9,
    "dispersion": 1.1
},
"extend": { "effects": [ "RECYCLED" ] },
"delete": { "effects": [ "NEVER_MISFIRES" ] }
```

上記の(再装填弾の)例には、以下の追加ルールが適用されます:

連鎖継承が可能です。 例えば、reloaded_556 は `556` を継承しており、その `556` 自体も `223` から派生しています。

数値データは、親に対する「`比例値`」で指定できます。 小数点を用いた係数で指定し、`0.5` は 50%、`2.0` は 200% を意味します。

フラグは `delete` を通じて削除できます。 削除しようとしたフラグが親に存在していなくても、エラーにはなりません。

ゲーム内には登場せず、他のタイプが継承するためだけに存在する「`抽象`(abstract)」タイプを定義することが可能です。 以下の簡略化された例では、`magazine_belt`(マガジンベルト)が、実装されているすべての給弾ベルトに共通する値を定義しています。:

```json
"abstract": "magazine_belt",
"type": "MAGAZINE",
"name": "Ammo belt",
"description": "An ammo belt consisting of metal linkages which disintegrate upon firing.",
"rigid": false,
"armor_data": {
    "covers": [ "TORSO" ],
    ...
},
"flags": [ "MAG_BELT", "MAG_DESTROY" ]
```

上記の(抽象タイプの)例には、以下の追加ルールが適用されます:

必須フィールドが記述されていなくても、エラーにはなりません。 これは、JSONの読み込み完了後に `abstract` タイプ自体は破棄されるためです。

記述のない任意フィールドについては、そのタイプの通常の既定値が設定されます。

## サポート

現在、継承をサポートしているタイプは以下の通りです:

- GENERIC (一般アイテム：衣類以外の多くの雑貨など)
- AMMO (弾薬)
- GUN (銃器)
- GUNMOD (銃器改造パーツ)
- MAGAZINE (マガジン)
- TOOL (道具。ただし TOOL_ARMOR は除く)
- COMESTIBLE (食料・薬品などの消耗品)
- BOOK (本・レシピ本)
- ENGINE (エンジンパーツ)

あるタイプが copy-from をサポートしているかを知るには、そのタイプが generic_factory を実装しているかどうかを確認する必要があります。これを確認するには、以下の手順を行ってください:

- [init.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/init.cpp)を開きます。
- 調べたいタイプが記述されている行を探します。gate(門)タイプなら、
  例えば、`add( "gate", &gates::load );`のような行が見つかります。
- そのロード関数名(この例では gates::load)をコピーします。
- [Github検索バー](https://github.com/cataclysmbn/Cataclysm-BN/search?q=%22gates%3A%3Aload%22&unscoped_q=%22gates%3A%3Aload%22&type=Code)
  にその関数名を入力し、_gates::load_が定義されているソースファイルを検索します。
- 検索結果から
  [gates.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/gates.cpp)を見つけて開きます。
- gates.cpp内で generic_factory という記述がある行を探します。以下のよう
  な形式で見つかるはずです:
  `generic_factory<gate_data> gates_data( "gate type", "handle", "other_handles" );`
- generic_factory の行が存在すれば、そのタイプは copy-fromをサポートして
  いると結論付けられます。
- もし generic_factoy が見つからなければ、copy-fromには対応していませ
  ん。例えば
  vitamin (ビタミン)タイプで同じ手順を踏むと、
  [vitamin.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/vitamin.cpp) には generic_factoyが存在しないことがわかります。
