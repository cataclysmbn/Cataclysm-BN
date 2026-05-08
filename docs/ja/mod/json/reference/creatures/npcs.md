---
title: NPCs
tableOfContents:
  maxHeadingLevel: 4
---

## NPCとの会話の作成

TODO: NPCテンプレートの読み込みに使用される "npc" 構造体について記述する。

会話は状態遷移図のように機能します。 まずNPCが何かを話し (トピックの開始)、それに対してプレイヤーキャラクターが応答 (複数の選択肢から一つを選択)します。その応答によって次の会話トピックが決定されます。このやり取りは、会話が終了するか、あるいはNPCが敵対状態になるまで繰り返されます。

Note 特定のトピックに対して、自分自身(同じトピック)にループする応答を設定しても全く問題ありません。

NPCのミッション(クエスト)については、関連していますが別のJSON構造で管理されています。詳細は
[ミッションのドキュメント](./missions_json.md)を参照してください。

会話において、以下の2つのトピックは特殊な意味を持ちます:

- `TALK_DONE` 直ちに会話を終了します。
- `TALK_NONE` 直前の会話トピックに戻ります。

### 会話の妥当性確認

会話トピックを把握し、応答内で参照されているすべてのトピックが定義済みであるか、また定義されたトピックがNPCのチャットや応答で正しく参照されているかを確認するのは非常に困難な作業です。 これを補助するため、`tools/dialogue_validator.py` にPythonスクリプトが用意されています。これを使用すると、すべてのトピックと応答の対応関係をマッピングできます。

```sh
python tools/dialogue_validator.py data/json/npcs/* data/json/npcs/Backgrounds/* data/json/npcs/refugee_center/*
```

Modで独自の会話を追加する場合は、スクリプト実行時にそのModの会話ファイルへのパスを追加することでチェックが可能です。

## 会話トピック

各トピックは、以下の要素で構成されています:

1. トピックID (例: `TALK_ARSONIST`)
2. ダイナミックライン (dynamic_line): NPCが発するセリフ。
3. スピーカーエフェクト (speaker_effect): (任意) NPCがセリフを発した際に発生する効果のリスト。
4. 応答 (responses): プレイヤーキャラクターが選択できる回答のリスト。
5. 反復応答 (repeated responses) (任意) プレイヤーまたはNPCが特定のアイテムを所持している場合に自動生成される応答
   のリスト。

JSONを使用して新しいトピックを定義できます。現時点では、会話の「開始トピック」を自由に定義することはできません。そのため、既存のデフォルトトピック (`TALK_STRANGER_FRIENDLY` や `TALK_STRANGER_NEUTRAL`など)、あるいはすでに到達可能な他のトピックに応答を追加して、そこから新しい会話へ繋げる必要があります。

書式:

```json
{
  "type": "talk_topic",
  "id": "TALK_ARSONIST",
  "dynamic_line": "What now?",
  "responses": [
    {
      "text": "I don't know either",
      "topic": "TALK_DONE"
    }
  ],
  "replace_built_in_responses": true
}
```

### type

必須項目です。値は常に `"talk_topic"`である必要があります。

### id

トピックIDには、ゲームに組み込まれた既存のID、または新しいIDを指定できます。ただし、JSON内で同じIDを持つ複数の会話トピックが定義されている場合、最後に読み込まれた定義がそれ以前のものを上書きします。

また、IDには文字列の配列を指定することも可能です。この場合、配列に含まれるすべてのIDに対して、全く同じ内容のトピックが定義されたものとして読み込まれます。JSONから読み込む際、応答は既存のリストに追加されますが、`dynamic_line`と `replace_built_in_responses` の設定については、JSONで定義されている場合、元の設定を上書きします。この仕組みを利用することで、複数のトピックに対して一度に応答を追加することができます。

以下の例では、指定したすべてのトピックに「もう行くよ。(I'm going now!)」という応答を追加しています。

```cpp
{
    "type": "talk_topic",
    "id": [ "TALK_ARSONIST", "TALK_STRANGER_FRIENDLY", "TALK_STRANGER_NEUTRAL" ],
    "dynamic_line": "What now?",
    "responses": [
        {
            "text": "I'm going now.",
            "topic": "TALK_DONE"
        }
    ]
}
```

### セリフ (dynamic_line)

`dynamic_line` はNPCが話すセリフを定義します。これは任意項目です。定義されていない場合、そのIDが組み込み `dynamic_line` トピックと同じであれば、元の組み込みセリフが使用されます。それ以外の場合は、NPCは何も話しません。詳細は後述の`dynamic_line`の詳細セクションを参照してください。

### 発言時効果 (speaker_effect)

The `speaker_effect`は、プレイヤーがどの応答を選択したかに関わらず、NPCが
`dynamic_line`を話した直後に発生する効果のオブジェクト、またはその配列です。詳細は後述のSpeaker Effectsの詳細セクションを参照してください。

### プレイヤーの応答 (response)

`responses` エントリは、選択可能な応答の配列です。空にすることはできません。各エントリは応答オブジェクトの必要があります。詳細は後述の「Responses の詳細」セクションを参照してください。

### 組み込み応答の置換 (replace_built_in_responses)

`replace_built_in_responses` は、そのトピックに対する「組み込みの応答」を無効化するかどうかを決定する、任意指定のbool値です(既定値は `false`)。

- 組み込みの応答が存在しない場合: この設定は何の影響も与えません。
- `true`に設定した場合: 組み込みの応答は無視され、現在のJSONファイルで定義された応答のみが使用されます。
- `false`に設定した場合 (既定値): 現在のJSONで定義された応答が、組み込みの応答 (存在する場合)と組み合わせて使用されます。

---

## dynamic_line

`dynamic_line`は、単純な文字列、より詳細な複合オブジェクト、`dynamic_line` のエントリを複数格納した配列のいずれかで定義されます。

- 配列形式の場合: NPCが応答を必要とするたびに、リストの中から1つのエントリがランダムに選択されます。
- 選択確率: 各エントリが選ばれる確率はすべて均等です。

例:

```json
"dynamic_line": [
    "generic text",
    {
        "npc_female": [ "text1", "text2", "text3" ],
        "npc_male": { "u_female": "text a", "u_male": "text b" }
    }
]
```

複合オブジェクト形式の`dynamic_line`には、通常、複数の`dynamic_line`エントリと、それらのどれを使用するかを決定する条件が含まれます。

- 処理の優先順位: dynamic_line がネストされていない場合、リストの上から記述順に
  処理されます。
- 代名詞の定義: すべてのケースにおいて、`npc_` は NPC を、`u_` は プレイヤー を
  指します。
- 任意項目の扱い: 必須ではない項目は定義しなくても構いませんが、NPCが常に何かし
  らの応答を返せるよう配慮してください。
- ネスト構造: 各エントリは常に`dynamic_line`として解析されるため、さらにその中に
  `dynamic_line` を含めることが可能です。

#### 複数のラインの結合

この`dynamic_line`は、複数の`dynamic_line`をまとめたリストであり、リスト内のすべての内容が結合されて表示されます。 リストに含まれる各 `dynamic_line`は、通常どおり個別に処理されます。

```json
{
  "and": [
    {
      "npc_male": true,
      "yes": "I'm a man.",
      "no": "I'm a woman."
    },
    "  ",
    {
      "u_female": true,
      "no": "You're a man.",
      "yes": "You're a woman."
    }
  ]
}
```

#### 性別コンテキストに応じた翻訳

性別によって言葉遣いや語尾が変化する言語の翻訳をサポートするために、NPCやプレイヤー、あるいはその両方の性別情報を指定できます:

```json
{
  "gendered_line": "Thank you.",
  "relevant_genders": ["npc"]
}
```

(例えばポルトガル語などでは、話し手が男性か女性かによって"Thank you."の表現が異なります。).

`"relevant_genders"` リストに指定可能な値は`"npc"` と `"u"`です。

#### ヒントのランダム選択

ヒント用スニペット(短いテキスト群)から、セリフがランダムに選択されます。

```json
{
  "give_hint": true
}
```

#### 直前に生成された理由に基づくセリフ

以前のエフェクトによって生成された理由からセリフが選択されます。使用後、その理由は消去されます。この使用は `"has_reason"` 条件で制限する必要があります。

```json
{
  "has_reason": { "use_reason": true },
  "no": "What is it, boss?"
}
```

#### 会話条件に基づくセリフ

セリフ(dynamic_line)は、単一の会話条件の(true/false)に基づいて選択されます。会話条件を `"and"`、`"or"`、`"not"` で繋げることはできません。条件がtrueであれば `"yes"` の応答が選ばれ、そうでなければ `"no"` の応答が選ばれます。`"yes"` と `"no"` の応答はいずれも省略可能です。単純な文字列指定による条件の場合、後ろに `"true"` を続けて辞書のフィールドとして扱うか、条件がtrueの時に選ばれる応答を後ろに記述することで `"yes"` オブジェクト自体を省略することも可能です。

```json
{
    "npc_need": "fatigue",
    "level": "TIRED",
    "no": "Just few minutes more...",
    "yes": "Make it quick, I want to go back to sleep."
}
{
    "npc_aim_rule": "AIM_PRECISE",
    "no": "*will not bother to aim at all.",
    "yes": "*will take time and aim carefully."
}
{
    "u_has_item": "india_pale_ale",
    "yes": "<noticedbooze>",
    "no": "<neutralchitchat>"
}
{
    "days_since_cataclysm": 30,
    "yes": "Now, we've got a moment, I was just thinking it's been a month or so since... since all this, how are you coping with it all?",
    "no": "<neutralchitchat>"
}
{
    "is_day": "Sure is bright out.",
    "no": {
        "u_male": true,
        "yes": "Want a beer?",
        "no": "Want a cocktail?"
    }
}
```

---

## スピーカーエフェクト

`speaker_effect` エントリには、NPCが `dynamic_line` を話した後、プレイヤーが応答する前に発生する会話効果(effect)を記述します。プレイヤーがどの応答を選択したかに関わらず実行されます。各エフェクトには任意で条件(condition)を設定でき、その条件がtrueである場合のみ適用されます。また、各 `speaker_effect` には任意の識別子(`sentinel`)を設定することも可能です。これを使用することで、そのエフェクトが一度しか実行されないことを保証できます。

訳注 処理の順番のフロー図:

1. NPCがセリフ(dynamic_line) を話す
2. speaker_effect が実行される (条件チェックと一度きりの判定を含む)
3. プレイヤーの応答選択肢が表示される
4. プレイヤーが選択し、次のトピックへ移動

書式:

```json
"speaker_effect": {
  "sentinel": "...",
  "condition": "...",
  "effect": "..."
}
```

or:

```json
"speaker_effect": [
  {
    "sentinel": "...",
    "condition": "...",
    "effect": "..."
  },
  {
    "sentinel": "...",
    "condition": "...",
    "effect": "..."
  }
]
```

`sentinel`には任意の文字列を使用できますが、各`TALK_TOPIC`内で固有の必要があります。 一つの `TALK_TOPIC` 内に複数の `speaker_effect` が存在する場合は、それぞれに異なる `sentinel` を設定してください。`sentinel` は必須ではありませんが、会話がその`TALK_TOPIC` に戻るたびに `speaker_effect` が実行されてしまうため、意図しない効果の重複を避けるために設定が強く推奨されます。

`effect`には、後述する有効なエフェクトを何でも指定できます。エフェクトは、通常のオブジェクトと同様に、単純な文字列、オブジェクト、あるいは文字列とオブジェクトの配列のいずれの形式でも記述可能です。

任意指定の `condition`(条件)には、後述する有効な条件を何でも指定できます。`condition` が設定されている場合、その条件が true であるときにのみ `effect` が発生します。

スピーカーエフェクトは、「プレイヤーがNPCと話したこと」を示すステータス変数を設定するのに便利です。これにより、個々の「応答」に複数の変数操作を記述して複雑化させるのを避けることができます。また、`sentinel` と併用することで、プレイヤーがNPCから特定のセリフを初めて聞いた時に一度だけ `mapgen_update`(マップ生成の更新)を実行するといった使い方も可能です。

---

## 応答

応答には、少なくともひとつのテキスト (プレイヤーキャラクターが話す内容として表示されますが、ゲーム的な意味はありません)と、会話が次に遷移するトピックIDが含まれます。また、「試行」オブジェクトを持たせることもでき、これを使用してNPCに対し「嘘をつく(lie)」、「説得する(persuade)」、「威圧する(intimidate)」といったアクションが行えます (詳細は後述)。試行には成功時と失敗時のそれぞれに対して、異なる結果を設定することが可能です。

書式:

```json
{
  "text": "I, the player, say to you...",
  "condition": "...something...",
  "trial": {
    "type": "PERSUADE",
    "difficulty": 10
  },
  "success": {
    "topic": "TALK_DONE",
    "effect": "...",
    "opinion": {
      "trust": 0,
      "fear": 0,
      "value": 0,
      "anger": 0,
      "owed": 0,
      "favors": 0
    }
  },
  "failure": {
    "topic": "TALK_DONE"
  }
}
```

あるいは、以下のような短縮形式も使用できます:

```json
{
  "text": "I, the player, say to you...",
  "effect": "...",
  "topic": "TALK_WHATEVER"
}
```

この短縮形式は、以下の定義 (無条件のトピック遷移。`effect` は任意)と同等です:

```json
{
  "text": "I, the player, say to you...",
  "trial": {
    "type": "NONE"
  },
  "success": {
    "effect": "...",
    "topic": "TALK_WHATEVER"
  }
}
```

任意指定のブール値(true/false)キーである "switch" と "default" は、既定では false に設定されています。

`"switch": true` かつ `"default": false` に設定された応答のうち、条件(condition) を満たした最初のひとつだけが表示され、それ以外の `"switch": true` な応答は表示されません。もし `"switch": true` かつ `"default": false` の応答がひとつも表示されなかった場合に限り、`"switch": true` かつ `"default": true` に設定された応答がすべて表示されます。

どちらの場合においても、`"switch": false` に設定された応答は(`"default": true` が設定されているかに関わらず)、条件を満たしている限りすべて表示されます。

#### switchとdefaultの例

```json
"responses": [
  { "text": "You know what, never mind.", "topic": "TALK_NONE" },
  { "text": "How does 5 Ben Franklins sound?",
    "topic": "TALK_BIG_BRIBE", "condition": { "u_has_items": { "item": "100_usd", "count": 5 } }, "switch": true },
   { "text": "I could give you a big Grant.",
    "topic": "TALK_BRIBE", "condition": { "u_has_item": "50_usd" }, "switch": true },
  { "text": "Lincoln liberated the slaves, what can he do for me?",
    "topic": "TALK_TINY_BRIBE", "condition": { "u_has_item": "5_usd" }, "switch": true, "default": true },
  { "text": "Maybe we can work something else out?", "topic": "TALK_BRIBE_OTHER",
    "switch": true, "default": true },
  { "text": "Gotta go!", "topic": "TALK_DONE" }
]
```

プレイヤーには常に「前のトピックに戻る」か「会話を終了する」という選択肢が表示されます。それ以外については、資金がある場合に限り、500ドル、50ドル、または5ドルの賄賂を贈る選択肢が表示されます。もしプレイヤーが少なくとも50ドル持っていない場合は、別の賄賂を提案する選択肢も表示されます。

### truefalsetext

条件(condition)がtrueの場合と偽の場合で、プレイヤーの応答テキストを切り替えます。どちらのテキストを選んだ場合でも、設定された同一の試行が実行されます。`condition`、`true`、`false` のすべてが必須項目です。

```json
{
  "truefalsetext": {
    "condition": { "u_has_item": "FMCNote" },
    "true": "I may have the money, I'm not giving you any.",
    "false": "I don't have that money."
  },
  "topic": "TALK_WONT_PAY"
}
```

### text

ユーザーに表示されるテキストです。それ以上の(システム的な)意味はありません。

### trial

任意指定の項目であり、定義されていない場合は `"NONE"` が使用されます。指定する場合は `"NONE"`、`"LIE"`(嘘)、`"PERSUADE"`(説得)、`"INTIMIDATE"`(威圧)、`"CONDITION"`(条件判定)のいずれか一つを指定します。`"NONE"` を使用する場合、`failure`(失敗)オブジェクトは読み込まれませんが、それ以外の場合は必須となります。

`difficulty`(難易度)は、タイプが `"NONE"` または `"CONDITION"` 以外の場合にのみ必要で、成功率をパーセント単位で指定します(ただし、変異などの様々な要因によって修正されます)。難易度の数値が高いほど、成功しやすくなります。

任意で設定できる `mod`(修正)配列には以下の修正子を指定でき、その修正子に対応する「プレイヤーに対するNPCの感情」や「NPCの性格的特徴」に、指定した値を掛け合わせた分だけ難易度が上昇します： `"ANGER"`(怒り)、`"FEAR"`(恐怖)、`"TRUST"`(信頼)、`"VALUE"`(価値)、`"AGRESSION"`(攻撃性)、`"ALTRUISM"`(利他主義)、`"BRAVERY"`(勇敢さ)、`"COLLECTOR"`(収集癖)。 特殊な "POS_FEAR" 修正子は、NPCのプレイヤーに対する恐怖心が0未満の場合、それを0として扱います。 特殊な `"TOTAL"` 修正子は、それまでのすべての修正子の合計に指定した値を掛け合わせるもので、貸し(owed value)を設定する際などに使用されます。

`"CONDITION"` による試行では、`difficulty` の代わりに `condition`(条件)の記述が必須となります。`condition` がtrueであれば `success` オブジェクトが、falseであれば `failure` オブジェクトが選択されます。

### 成功と失敗

どちらのオブジェクトも同じ構造を持ちます。`topic` は会話が次に遷移するトピックを定義します。`opinion`(感情)は任意項目で、NPCの感情がどのように変化するかを定義します。指定された値はNPCの既存の感情値に加算されます。すべての値は任意指定で、既定値は0です。`effect` は、その応答が選択された後に実行される関数です(詳細は後述)。

NPCの感情値は、NPCとの相互作用において以下のようないくつかの側面に影響を与えます:

- 信頼 (trust) が高いほど、「嘘」や「説得」が成功しやすくなります。通常、これはポジティブな要素です。
- 恐怖 (fear) が高いほど、「威圧」が成功しやすくなりますが、NPCがプレイヤーから逃げ出す(会話ができなくなる)可能性もあります。
- 価値 (value) が高いほど、「説得」や「命令」を聞き入れやすくなります。これは一種の友好度の指標です。
- 怒り (anger) が高い場合(恐怖より20ポイント以上高い場合など。ただしNPCの性格にも依存します)、NPCは敵対的になります。通常、これはネガティブな要素です。 「恐怖」と「信頼」の組み合わせは、NPCの性格と相まって、最初の会話トピック(`"TALK_MUG"`(強盗)、`"TALK_STRANGER_AGGRESSIVE"`(攻撃的な見知らぬ人)、`"TALK_STRANGER_SCARED"`(怯えた見知らぬ人)、`"TALK_STRANGER_WARY"`(警戒する見知らぬ人)、`"TALK_STRANGER_FRIENDLY"`(友好的な見知らぬ人)、または `"TALK_STRANGER_NEUTRAL"`(中立的な見知らぬ人))を決定します。

これらのデータの実際の使用方法については、ソースコード内で `"op_of_u"`を検索してください。

試行が失敗した場合は `failure` オブジェクトが使用され、それ以外 (成功時や試行なし) の場合は `success` オブジェクトが使用されます。

### 試行のサンプル

```json
"trial": { "type": "PERSUADE", "difficulty": 0, "mod": [ [ "TRUST", 3 ], [ "VALUE", 3 ], [ "ANGER", -3 ] ] }
"trial": { "type": "INTIMIDATE", "difficulty": 20, "mod": [ [ "FEAR", 8 ], [ "VALUE", 2 ], [ "TRUST", 2 ], [ "BRAVERY", -2 ] ] }
"trial": { "type": "CONDITION", "condition": { "npc_has_trait": "FARMER" } }
```

`topic` には、(文字列のIDだけでなく)単一のトピックオブジェクトを指定することも可能です (この場合、type メンバーの記述は不要です):

```json
"success": {
    "topic": {
        "id": "TALK_NEXT",
        "dynamic_line": "...",
        "responses": [
        ]
    }
}
```

### 条件

これは任意指定の条件で、特定の状況下において応答が表示されないようにするために使用されます。定義されていない場合、既定値は常に `true`(表示)となります。条件を満たさない場合、その応答は選択肢のリストに表示されません。指定可能な内容については、後述の「会話の条件(Dialogue Conditions)」を参照してください。

---

## 反復応答

反復応答とは、アイテムのインスタンスごとに一度ずつ、応答リストに複数回追加される応答のことです。

反復応答は以下の書式で定義します:

```json
{
  "for_item": [
    "jerky",
    "meat_smoked",
    "fish_smoked",
    "cooking_oil",
    "cooking_oil2",
    "cornmeal",
    "flour",
    "fruit_wine",
    "beer",
    "sugar"
  ],
  "response": { "text": "Delivering <topic_item>.", "topic": "TALK_DELIVER_ASK" }
}
```

`"response"` は必須項目であり、前述した標準的な会話応答の形式で記述する必要があります。反復応答内でも `"switch"` を使用することができ、通常通り機能します。

`"for_item"` または `"for_category"` のいずれかの指定が必要です。これらはそれぞれ単一の文字列、あるいはアイテムやアイテムカテゴリのリストとして記述できます。プレイヤー(またはNPC)のインベントリ内にある、リストに該当するアイテムごとに `response` が生成されます。

`"is_npc"` は任意指定のブール値です。これが指定されている場合、NPCのインベントリがチェック対象となります。指定がない場合は、デフォルトでプレイヤーのインベントリがチェックされます。

`"include_containers"` は任意指定のブール値です。これが指定されている場合、中身が入っている容器自体も、その中身とは別の独立した応答として生成されます。

---

## 会話エフェクト

`speaker_effect` または `response` 内の `effect` フィールドには、以下のいずれかのエフェクトを指定できます。複数のエフェクトを設定する場合はリスト形式 (配列)で記述し、リストに並んでいる順序で処理されます。

### ミッション

| エフェクト                                                       | 説明                                                                                                                       |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `assign_mission`                                                 | 以前に選択されたミッションをプレイヤーキャラクターに割り当てます。                                                         |
| `mission_success`                                                | 現在のミッションを成功として解決します。                                                                                   |
| `mission_failure`                                                | 現在のミッションを失敗として解決します。                                                                                   |
| `clear_mission`                                                  | プレイヤーキャラクターに割り当てられたミッション一覧から、そのミッションを削除します。                                     |
| `mission_reward`                                                 | ミッションの報酬をプレイヤーに与えます。                                                                                   |
| `assign_mission: mission_type_id string`                         | 指定したミッション `mission_type_id` をプレイヤーに割り当てます。                                                          |
| `finish_mission: mission_type_id string`,`success: success_bool` | 指定したミッションを完了させます。 `mission_type_id` が `success` が true なら成功、そうでなければ失敗として処理されます。 |

### ステータス / 士気

| エフェクト           | 説明                                                                                                                                                                         |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `give_aid`           | プレイヤーの全身から噛み傷、感染、出血を除去し、各部位のダメージを 10-25 HP 回復させます。これには30分を要し、開始時にNPCは30分間の現在多忙`currently_busy` 状態になります。 |
| `give_all_aid`       | プレイヤーおよびクラフト可能範囲内にいる味方NPC全員に対して `give_aid` を行います。これには1時間を要し、開始時にNPCは1時間の現在多忙`currently_busy`状態になります。         |
| `buy_haircut`        | プレイヤーに「散髪」による意欲ボーナスを12時間与えます。                                                                                                                     |
| `buy_shave`          | プレイヤーに「髭剃り」による意欲ボーナスを6時間与えます。                                                                                                                    |
| `morale_chat`        | プレイヤーに「楽しい会話」による意欲ボーナスを6時間与えます。                                                                                                                |
| `player_weapon_away` | プレイヤーに武器を収納させます。                                                                                                                                             |
| `player_weapon_drop` | プレイヤーに武器をその場に落とさせます。                                                                                                                                     |

### キャラクターエフェクト / 変異

| エフェクト                                                                                                                                                                                             | 説明                                                                                                                                                                                                                                                               |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `u_add_effect: effect_string`, (_one of_ `duration: duration_string`, `duration: duration_int`)<br/> `npc_add_effect: effect_string`, (_one of_ `duration: duration_string`, `duration: duration_int`) | プレイヤーまたはNPCに、指定した期間(ターン数)だけ「ステータス効果」を付与します。期間に "PERMANENT" を指定すると、その効果は永続的に付与されます。                                                                                                                 |
| `u_add_trait: trait_string`<br/> `npc_add_trait: trait_string`                                                                                                                                         | プレイヤーまたはNPCに、指定した「特性」を付与します。                                                                                                                                                                                                              |
|                                                                                                                                                                                                        |                                                                                                                                                                                                                                                                    |
| `u_lose_effect: effect_string`<br/> `npc_lose_effect: effect_string`                                                                                                                                   | プレイヤーまたはNPCがその効果を持っている場合、それを解除します。                                                                                                                                                                                                  |
| `u_lose_trait: trait_string`<br/> `npc_lose_trait: trait_string`                                                                                                                                       | プレイヤーまたはNPCから指定した特性を削除します。                                                                                                                                                                                                                  |
| `u_add_var, npc_add_var`: `var_name, type: type_str`, `context: context_str`, `value: value_str`                                                                                                       | プレイヤーまたはNPCに、後で `u_has_var` 等で参照可能な変数を保存します。`npc_add_var` は任意の「ローカル変数」の保存に適しており、`u_add_var` は任意の「グローバル変数」の保存に使用できます。ステータス効果を設定するよりも、こちらを利用することが推奨されます。 |
| `u_lose_var`, `npc_lose_var`: `var_name`, `type: type_str`, `context: context_str`                                                                                                                     | 保存されている変数のうち、`var_name`、`type_str`、`context_str`が一致するものを削除します。                                                                                                                                                                        |
| `u_adjust_var, npc_adjust_var`: `var_name, type: type_str`, `context: context_str`, `adjustment: adjustment_num`                                                                                       | 保存されている数値変数を、指定した値 `adjustment_num`だけ増減させます。                                                                                                                                                                                            |
| `barber_hair`                                                                                                                                                                                          | プレイヤーが新しい髪型を選択できるメニューを開きます。                                                                                                                                                                                                             |
| `barber_beard`                                                                                                                                                                                         | プレイヤーが新しい髭のスタイルを選択できるメニューを開きます。                                                                                                                                                                                                     |
| `u_learn_recipe: recipe_string`                                                                                                                                                                        | プレイヤーが指定したレシピを学習し、記憶します。 `recipe_string`.                                                                                                                                                                                                  |
| `npc_first_topic: topic_string`                                                                                                                                                                        | NPCが最初に話しかけてくる際のトピックを、恒久的に指定のものへ変更します。                                                                                                                                                                                          |

### 取引 / アイテム

| エフェクト                                                                                                                                                               | 説明                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `start_trade`                                                                                                                                                            | 取引画面を開き、NPCと取引を行えるようにします。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| `buy_10_logs`                                                                                                                                                            | 牧場のガレージに丸太を10本配置し、そのNPCを1日の間利用不可にします。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| `buy_100_logs`                                                                                                                                                           | 牧場のガレージに丸太を100本配置し、そのNPCを7日の間利用不可にします。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| `give_equipment`                                                                                                                                                         | プレイヤーがNPCのインベントリからアイテムを選択し、自分のインベントリへ移動させることができます。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| `npc_gets_item`                                                                                                                                                          | プレイヤーが自分のインベントリからアイテムを選択し、NPCに渡します。NPCの所持容量 (重量・体積)に空きがない場合は拒否され、その理由は変数 `"use_reason"` に保存され、後の会話で参照できます。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| `npc_gets_item_to_use`                                                                                                                                                   | プレイヤーがアイテムを選択してNPCに渡します。NPCはそれを装備しようと試みますが、重すぎる場合や現在の武器より弱い場合は拒否されます。拒否理由は `"use_reason"` に保存されます。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| `u_buy_item: item_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`, _optional_ `container: container_string`)                                        | NPCがプレイヤーに指定のアイテム(または count 個)を渡します。cost が指定されている場合、NPCの貸し(op_of_u.owed)からその分を差し引きます。貸しが不足している場合は取引画面が開き、差額を支払うまでアイテムは渡されません。cost 未指定なら無料です。.                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| `u_sell_item: item_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`)                                                                                 | プレイヤーがNPCに指定のアイテム(または count 個)を渡します。cost 指定時はその分がNPCの貸しに加算されます。未指定なら無料です。アイテムが不足していると失敗するため、事前に `u_has_items` で所持確認を行う必要があります。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| `u_bulk_trade_accept`<br/> `npc_bulk_trade_accept`                                                                                                                       | `repeat_response`の直後でのみ有効なエフェクトです。プレイヤーは、反復応答によってリストアップされた対象アイテムのすべてのインスタンスをNPCと取引します。`u_bulk_trade_accept`の場合、プレイヤーはインベントリからそれらのアイテムを失い、代わりに同等の価値を持つ「NPCの勢力通貨」を受け取ります。; `npc_bulk_trade_accept`の場合、プレイヤーはNPCのインベントリからアイテムを受け取り、同等の価値の「NPCの勢力通貨」を支払います。 取引額に端数(残りの価値)が出た場合、あるいはNPCが勢力通貨を持っていない場合は、その差額分がNPCの「プレイヤーに対する貸し `op_of_u.owed`に加算・減算されます。                                                                                                                |
| `u_bulk_donate`<br/> `npc_bulk_donate`                                                                                                                                   | `repeat_response`の直後でのみ有効なエフェクトです。プレイヤーまたはNPCは、反復応答によってリストアップされた対象アイテムのすべてのインスタンスを相手に譲渡します。`u_bulk_donate`の場合、プレイヤーはインベントリからアイテムを失い、NPCがアイテムを受け取ります。`npc_bulk_donate`の場合、プレイヤーがNPCのインベントリからアイテムを受け取り、NPCはアイテムを失います。                                                                                                                                                                                                                                                                                                                                        |
| `u_spend_ecash: amount`                                                                                                                                                  | プレイヤーキャラクターの「大災厄前」の銀行口座から、指定された`amount` (金額)を差し引きます。負の値を指定した場合、プレイヤーは電子マネー(e-cash)を獲得します。 NPCは電子マネーを取り扱うべき **ではなく** 個人間の貸し借りや現物アイテム(勢力通貨を含む)のみで取引を行うように設計してください。                                                                                                                                                                                                                                                                                                                                                                                                                |
| `add_debt: mod_list`                                                                                                                                                     | `mod_list`に指定された値に基づいて、NPCからプレイヤーへの「貸し(債務)」を増やします。<br/>例えば以下の記述は、NPCの「利他主義(altruism)」の1500倍と、NPCがプレイヤーに対して抱いている「価値(value)」の1000倍を合計した額だけ、NPCの借金を増やします: `{ "effect": { "add_debt": [ [ "ALTRUISM", 3 ], [ "VALUE", 2 ], [ "TOTAL", 500 ] ] } }`                                                                                                                                                                                                                                                                                                                                                                    |
| `u_consume_item`, `npc_consume_item: item_string`, (_optional_ `count: count_num`)                                                                                       | プレイヤーまたはNPCのインベントリから、指定されたアイテム(またはそのアイテム `count_num` 個分)を削除(消費)します。<br/>このエフェクトは、プレイヤーまたはNPCが少なくとも `count_num` 個以上のアイテムを持っていない場合は失敗します。そのため、事前に `u_has_items` または `npc_has_items`を使用して所持条件をチェックしておく必要があります。                                                                                                                                                                                                                                                                                                                                                                   |
| `u_remove_item_with`, `npc_remove_item_with: item_string`                                                                                                                | プレイヤーまたはNPCのインベントリ内にある、該当するアイテムのインスタンスをすべて削除します。<br/>これは無条件の削除であり、プレイヤーまたはNPCが対象のアイテムを所持していない場合でも、エラーや失敗になることはありません。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| `u_buy_monster: monster_type_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`, _optional_ `name: name_string`, _optional_ `pacified: pacified_bool`) | NPCはプレイヤーキャラクターに対し、指定されたモンスターをペットとして `count_num` (未指定の場合は1)体譲渡します。`cost_num` が指定されている場合、NPCの「プレイヤーに対する貸し `op_of_u.owed` からその分を差し引きます。 もし「貸し」の残高が`op_o_u.owed` より少ない場合は取引画面が開き、プレイヤーはその差額を埋めるために別のアイテムなどを差し出す必要があります。この`cost_num`が満たされない限り、NPCはモンスターを譲渡しません。<br/>`cost_num`(費用)の指定がない場合、NPCは無償でモンスターを譲渡します。<br/>`name_string` が指定されている場合、譲渡されるモンスターにはその名前が付けられます。 `pacified_bool`に設定した場合、そのモンスターには「鎮静(pacified)」状態のエフェクトが適用されます。 |

#### 振る舞い / AI

| エフェクト                             | 説明                                                                                                                                 |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `assign_guard`                         | NPCを「守衛」にします。味方NPCかつ拠点にいる場合、その拠点に配属されます。                                                           |
| `stop_guard`                           | NPCを`assign_guard`(守衛任務)から解放します。友好的なNPCであれば、再びプレイヤーへの追従を開始します。                               |
| `wake_up`                              | 眠っているNPCを起こします(ただし、鎮静剤などで動けない状態のNPCには無効です)。                                                       |
| `reveal_stats`                         | プレイヤーの評価スキルに基づき、NPCのステータス(能力値)を開示させます。                                                              |
| `end_conversation`                     | 会話を終了し、以降そのNPCがプレイヤーを無視するようにします。                                                                        |
| `insult_combat`                        | 会話を終了し、NPCを敵対化させます。プレイヤーがNPCに戦いを挑んだというメッセージが表示されます。                                     |
| `hostile`                              | NPCを敵対化させ、会話を終了します。                                                                                                  |
| `flee`                                 | NPCをプレイヤーから逃走させます。                                                                                                    |
| `follow`                               | NPCを「プレイヤーのフォロワー」勢力に加入させ、追従させます。                                                                        |
| `leave`                                | NPCを「プレイヤーのフォロワー」勢力から脱退させ、追従を停止させます。                                                                |
| `follow_only`                          | 勢力を変更せずに、NPCをプレイヤーに追従させます。                                                                                    |
| `stop_following`                       | 勢力を変更せずに、NPCの追従を停止させます。                                                                                          |
| `npc_thankful`                         | NPCのプレイヤーに対する態度を好意的なものにします。                                                                                  |
| `drop_weapon`                          | NPCに持っている武器を落とさせます。                                                                                                  |
| `stranger_neutral`                     | NPCの態度を「中立」に変更します。                                                                                                    |
| `start_mugging`                        | NPCがプレイヤーに接近し、持ち物を強奪しようとします。プレイヤーが抵抗すれば攻撃してきます。                                          |
| `lead_to_safety`                       | NPCの態度が「先導」に変化し、プレイヤーに「安全な場所へ到達する」ミッションを与えます。                                              |
| `start_training`                       | NPCがプレイヤーに対し、スキルや武術の訓練を開始します。                                                                              |
| `companion_mission: role_string`       | NPCの役割に応じた、味方NPC用のミッションリストを提示します。                                                                         |
| `basecamp_mission`                     | 現在地の拠点に応じた、味方NPC用のミッションリストを提示します。                                                                      |
| `bionic_install`                       | NPCがプレイヤーのインベントリにある義体をプレイヤーに移植します。NPCは非常に高いスキルで行い、手術の難易度に応じた費用を請求します。 |
| `bionic_remove`                        | NPCがプレイヤーから義体を除去します。NPCは非常に高いスキルで行い、手術の難易度に応じた費用を請求します。                             |
| `npc_class_change: class_string`       | NPCのクラスを、指定した文字列に変更します。                                                                                          |
| `npc_faction_change: faction_string`   | NPCが所属する勢力を、指定したものに変更します。                                                                                      |
| `u_faction_rep: rep_num`               | NPCが現在所属している勢力内でのプレイヤーの評判を増減させます(負の値で低下)。                                                        |
| `toggle_npc_rule: rule_string`         | 味方NPCのAIルール `"use_silent"`(静音武器使)、`"allow_bash"`(破壊許可)のON/OFFを切り替えます。                                       |
| `set_npc_rule: rule_string`            | 味方NPCのAIルールをtrueにします。`"use_silent"` または`"allow_bash"`                                                                 |
| `clear_npc_rule: rule_string`          | 味方NPCのAIルールをfalseにします。`"use_silent"` または `"allow_bash"`                                                               |
| `set_npc_engagement_rule: rule_string` | NPCが敵と交戦を開始する距離 `交戦規定`を、指定したルールに変更します。                                                               |
| `set_npc_aim_rule: rule_string`        | NPCが射撃時にどれくらい慎重に狙うか`照準速度`のルールを設定します。                                                                  |
| `npc_die`                              | 会話の終了時に、NPCが死亡します。                                                                                                    |

### マップの更新

| パラメータ                                                                                                                    | 説明                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| :---------------------------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mapgen_update:mapgen_update_id_string`<br>`mapgen_update:[list_of_strings]`<br>(optional `assign_mission_target` parameters) | 他のパラメータを指定しない場合、プレイヤーが現在立っている場所のオーバーマップタイルに対し、mapgen_update_id（またはそのリスト）で定義された変更を適用し、マップを更新します。<br>assign_mission_target パラメータを使用することで、更新の対象となるオーバーマップタイルの場所を変更できます。<br>assign_mission_targetパラメータについては[ミッションに関するドキュメント](missions_json)を、を、マップ生成については[マップ生成に関するドキュメント](../map/mapgen)を参照してください。 |

### 非推奨

| エフェクト                                                                 | 説明                                                                                                                                                                          |
| -------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `deny_follow`<br/> `deny_lead`<br/> `deny_train`<br/> `deny_personal_info` | NPCに対して、適切なステータス効果を数時間分設定します。<br/>これらのエフェクトは、前述したより柔軟な`npc_add_effect` の使用が推奨されており、現在は**非推奨**となっています。 |

### Sample effects

```json
{ "topic": "TALK_EVAC_GUARD3_HOSTILE", "effect": [ { "u_faction_rep": -15 }, { "npc_change_faction": "hells_raiders" } ] }
{ "text": "Let's trade then.", "effect": "start_trade", "topic": "TALK_EVAC_MERCHANT" },
{ "text": "What needs to be done?", "topic": "TALK_CAMP_OVERSEER", "effect": { "companion_mission": "FACTION_CAMP" } }
{ "text": "Do you like it?", "topic": "TALK_CAMP_OVERSEER", "effect": [ { "u_add_effect": "concerned", "duration": 600 }, { "npc_add_effect": "touched", "duration": "3600" }, { "u_add_effect": "empathetic", "duration": "PERMANENT" } ] }
```

---

### 感情の変化

特殊なエフェクトとして、プレイヤーキャラクターに対するNPCの感情値を変化させることができます。以下の形式を使用してください:

#### opinion: { }

trust(信頼)、value(価値)、fear(恐怖)、anger(怒り)は、opinion オブジェクト内で使用できる任意指定のキーワードです。
各キーワードの後には数値を記述します。NPCの現在の感情値に、その数値が加算(または減算)されます。

#### 感情変化のサンプル

```json
{ "effect": "follow", "opinion": { "trust": 1, "value": 1 }, "topic": "TALK_DONE" }
{ "topic": "TALK_DENY_FOLLOW", "effect": "deny_follow", "opinion": { "fear": -1, "value": -1, "anger": 1 } }
```

#### mission_opinion: { }

trust、value、fear、anger は、`mission_opinion` オブジェクト内でも任意で使用できるキーワードです。
各キーワードの後には数値を記述します。NPCの感情値がその数値によって修正されます。
(※通常、ミッションの成否に関連した感情変化に使用されます)

---

## 会話の条件

条件(Conditions)の記述形式には、制限付き文字列(simple string)のみのもの、キーと整数値(int)、キーと文字列(string)、キーと配列(array)、またはキーとオブジェクト(object)の組み合わせがあります。
配列とオブジェクトは互いに入れ子にすることができ、その中に他のあらゆる条件を含めることが可能です。

使用可能なキーおよび単純な文字列は以下の通りです:

#### ブール論理

| 条件    | 型           | 説明                                                                                                                                                                                                                                                                                                                          |
| ------- | ------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"and"` | 配列型       | リスト内のすべての条件がtrueである場合に`true`となります。<br/>`"[INTELLIGENCE 10+][PERCEPTION 12+] Your jacket is torn. Did you leave that scrap of fabric behind? "` <br/> "[知力 10以上][知覚 12以上] 上着が破れているな。あの布切れをどこかに残してきたのか？"                                                            |
| `"or"`  | 配列型       | 配列内の条件のうち、いずれか一つでも`true`であれば`true`となります。これを利用して、次のような複雑な条件判定を作成することができます。 <br/> `"[STRENGTH 9+] or [DEXTERITY 9+] I'm sure I can handle one zombie."` <br/>"[筋力 9以上] または [敏捷 9以上] ゾンビの一体くらい、私なら何とかできるはずだ。"                     |
| `"not"` | オブジェクト | オブジェクトまたは文字列で指定された条件が`false`である場合に、`true`となります。他の条件を否定することで、複雑な判定を作成するために使用されます。<br/> `"[INTELLIGENCE 7-] Hitting the reactor with a hammer should shut it off safely, right?"` <br/>"[知力 7未満] 原子炉をハンマーで叩けば、安全に停止するはずだ。だろ？" |

#### プレイヤーまたはNPCの判定条件

プレイヤーに対して判定を行う場合は `"u_"`形式 を使用し、NPCに対して判定を行う場合は
`"npc_"` 形式を使用することで、それぞれ個別にテストすることができます。

| 条件                                                   | 型             | 説明                                                                                                                                                                                                                                                                                                                                                                   |
| ------------------------------------------------------ | -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"u_male"`<br/> `"npc_male"`                           | 制限付き文字列 | プレイヤーまたはNPCが男性であれば`true`。                                                                                                                                                                                                                                                                                                                              |
| `"u_female"`<br/> `"npc_female"`                       | 制限付き文字列 | プレイヤーまたはNPCが女性であれば`true`                                                                                                                                                                                                                                                                                                                                |
| `"u_at_om_location"`<br/> `"npc_at_om_location"`       | 文字列         | プレイヤーまたはNPCが、指定されたIDの`広域マップ`タイル上にいればtrueとなります。 特殊文字列 `"FACTION_CAMP_ANY"` を指定すると、プレイヤーまたはNPCが「何らかの勢力拠点」のタイル上にいる場合に`true`を返します。 特殊文字列 `"FACTION_CAMP_START"` を指定すると、現在立っている広域マップタイルが「勢力拠点として設営可能なタイル」である場合にtrueを返します。       |
| `"u_has_trait"`<br/> `"npc_has_trait"`                 | 文字列         | プレイヤーまたはNPCが、指定された特定の特性フラグを持つ特性を一つでも持っていれば`true`となります。これは、個別の特性を指定する`u_has_any_trait` などよりも汎用性の高い判定方法です。特殊な特性フラグである`"MUTATION_THRESHOLD"` を指定すると、プレイヤーまたはNPCが(特定系統の)変異の閾値を超えているかどうかを判定できます。                                        |
| `"u_has_trait_flag"`<br/> `"npc_has_trait_flag"`       | 文字列         | プレイヤーまたはNPCが、指定された特定の特性フラグを持つ特性を一つでも持っていればtrueとなります。これは`u_has_any_trait`や`npc_has_any_trait`よりも堅牢(汎用的)な判定方法です。 特殊な特性フラグである `"MUTATION_THRESHOLD"`は、プレイヤーまたはNPCが(特定系統の)変異の閾値を超えているかどうかを判定するために使用されます。                                         |
| `"u_has_any_trait"`<br/> `"npc_has_any_trait"`         | 配列           | 配列内に指定された特性または変異のうち、プレイヤーあるいはNPCがいずれか一つでも持っていれば`true`となります。特定の複数の特性をまとめてチェックしたい場合に使用されます。                                                                                                                                                                                              |
| `"u_has_var"`<br/> `"npc_has_var"`                     | 文字列         | `"u_has_var"` または `"npc_has_var"` を使用する場合、それと同じディクショナリ(波括弧 { } 内)に、`"type"`、`"context"`、`"value"` の各フィールドを必須項目として記述する必要があります。<br/> `"u_add_var"` または `"npc_add_var"` によって設定された特定の文字列(`type_str`, `context_str`, `value_str`)を持つ変数が存在する場合、この条件は `true`となります。        |
| `"u_compare_var"`<br/> `"npc_compare_var"`             | 辞書           | `"u_add_var"` や `"npc_add_var"` と同様に変数を参照するため、`"type": type_str`、`"context": context_str`、`"op": op_str`、および `"value": value_num` はすべて必須項目となります。<br/> 指定された演算子`op_str` (`==`, `!=`, `<`, `>`, `<=`, `>=`のいずれか) と値に対し、プレイヤーキャラクターまたはNPCが保持している変数がその条件を満たす場合に`true`となります。 |
| `"u_has_strength"`<br/> `"npc_has_strength"`           | 整数           | プレイヤーキャラクターまたはNPCの筋力が、`u_has_strength` または `npc_has_strength` で指定した値以上であれば`true`となります。                                                                                                                                                                                                                                         |
| `"u_has_dexterity"`<br/> `"npc_has_dexterity"`         | 整数           | プレイヤーキャラクターまたはNPCの器用が、`u_has_dexterity` または `npc_has_dexterity` で指定した値以上であれば`true`となります。                                                                                                                                                                                                                                       |
| `"u_has_intelligence"`<br/> `"npc_has_intelligence"`   | 整数           | プレイヤーキャラクターまたはNPCの知力が、`u_has_intelligence` または `npc_has_intelligence` で指定した値以上であれば`true`となります。                                                                                                                                                                                                                                 |
| `"u_has_perception"`<br/> `"npc_has_perception"`       | 整数           | プレイヤーキャラクターまたはNPCの感覚が、`u_has_perception` または `npc_has_perception` で指定した値以上であれば`true`となります。                                                                                                                                                                                                                                     |
| `"u_has_item"`<br/> `"npc_has_item"`                   | 文字列         | プレイヤーキャラクターまたはNPCが、`u_has_item` または `npc_has_item` で指定した `item_id` と一致するアイテムをインベントリに所持していれば`true`となります。                                                                                                                                                                                                          |
| `"u_has_items"`<br/> `"npc_has_item"`                  | 辞書           | `u_has_items` または `npc_has_items` は、文字列の `item` と整数の `count` を持つ辞書型でなければなりません。<br/>プレイヤーキャラクターまたはNPCが、インベントリ内に該当する `item` を `count` で指定した数量(チャージ数または個数)以上所持していれば`true`となります。                                                                                                |
| `"u_has_item_category"`<br/> `"npc_has_item_category"` | 文字列         | `"count": item_count"` は任意指定のフィールドです。この項目は判定用の辞書内に記述する必要があり、指定されない場合の既定値は1となります。プレイヤーまたはNPCが、`u_has_item_category` もしくは `npc_has_item_category` で指定されたカテゴリと同じカテゴリのアイテムを `item_count`(指定した個数)分だけ所持していれば `true` となります。                                |
| `"u_has_bionics"`<br/> `"npc_has_bionics"`             | 文字列         | プレイヤーまたはNPCが、`"u_has_bionics"` もしくは `"npc_has_bionics"` で指定した `bionic_id`と一致する義体をインストールしている場合に `true` を返します。また、特殊な文字列として `"ANY"` を指定した場合、プレイヤーまたはNPCが何らかの義体を一つでもインストールしていれば `true` を返します。                                                                       |
| `"u_has_effect"`<br/> `"npc_has_effect"`               | 文字列         | プレイヤーキャラクターまたはNPCが、`u_has_effect` もしくは `npc_has_effect` で指定された `effect_id`の状態にあれば `true` となります。                                                                                                                                                                                                                                 |
| `"u_can_stow_weapon"`<br/> `"npc_can_stow_weapon"`     | 制限付き文字列 | プレイヤーキャラクターまたはNPCが武器を手にしており、かつそれを収納するための十分な空き容量がある場合に `true` となります。                                                                                                                                                                                                                                            |
| `"u_has_weapon"`<br/> `"npc_has_weapon"`               | 制限付き文字列 | プレイヤーキャラクターまたはNPCが、武器を装備していれば `true` となります。                                                                                                                                                                                                                                                                                            |
| `"u_driving"`<br/> `"npc_driving"`                     | 制限付き文字列 | プレイヤーキャラクターまたはNPCが車両を操作していれば `true` となります。注意： 現時点では、NPCは車両を操作することができません。                                                                                                                                                                                                                                      |
| `"u_has_skill"`<br/> `"npc_has_skill"`                 | 辞書           | `u_has_skill` または `npc_has_skill` は、文字列の `skill`と整数の `level`を持つ辞書形式で指定する必要があります。プレイヤーキャラクターまたはNPCが、指定された `skill` において `level` 以上の値を持っていれば `true` となります。                                                                                                                                     |
| `"u_know_recipe"`                                      | 文字列         | `u_know_recipe` で指定したレシピをプレイヤーキャラクターが知っていれば `true` となります。ただし、ここで「知っている」と見なされるのは、そのレシピを実際に暗記している場合のみです。そのレシピが掲載されている本を所持しているだけでは、条件を満たしたことにはなりません。                                                                                             |

#### プレイヤー専用の条件判定

| 条件              | 型             | 説明                                                                                                                                                                                                                                                                                                          |
| ----------------- | -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"u_has_mission"` | 文字列         | 指定したミッションIDがプレイヤーキャラクターに割り当てられていれば `true` となります。                                                                                                                                                                                                                        |
| `"u_has_ecash"`   | 整数           | プレイヤーキャラクターが、大災厄前の銀行口座に、指定された額 `u_has_ecash` 以上の電子マネー(e-cash)を保有していれば `true` を返します。注意点: NPCは電子マネーでの取引を行いません。NPCとのやり取りに使用できるのは、個人の「貸し(借り)」や「現物(アイテム)」、および「勢力通貨(faction currency)」のみです。 |
| `"u_are_owed"`    | 整数           | NPCに対するプレイヤーの「貸し」の額`op_of_u.owed`が、指定された値`u_are_owed`以上であれば `true` を返します。この条件は、プレイヤーがバーター取引(物々交換)による支払いを追加で行うことなく、NPCからアイテム等を購入できる状態にあるかどうかを確認するために使用されます。                                    |
| `"u_has_camp"`    | 制限付き文字列 | プレイヤーが1つ以上の有効な拠点を保有していれば `true` を返します。                                                                                                                                                                                                                                           |

#### プレイヤーとNPCの相互作用の条件

| 条件                            | 型             | 説明                                                                                                                                                                                                                                                                                                                                                            |
| ------------------------------- | -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"at_safe_space"`               | 制限付き文字列 | NPCの現在位置(広域マップ上)が安全な場所(`is_safe()` テストをパスする場所)であれば `true` を返します。                                                                                                                                                                                                                                                           |
| `"has_assigned_mission"`        | 制限付き文字列 | プレイヤーがそのNPCから受けているミッションが1つだけあれば true。「あの仕事についてだが……」といった選択肢に使用します。                                                                                                                                                                                                                                         |
| `"has_many_assigned_missions"`  | 制限付き文字列 | プレイヤーがそのNPCから**複数のミッション(2つ以上)**を受注していれば `true` を返します。 この条件は、「例の仕事のついてだが……」といったセリフの表示や、受注済みミッションの一覧画面(`"TALK_MISSION_LIST_ASSIGNED"`)へ遷移させる際の判定として使用されます。                                                                                                     |
| `"has_no_available_mission"`    | 制限付き文字列 | そのNPCが、プレイヤーに対して依頼できるミッションを1つも持っていなければ `true` を返します。                                                                                                                                                                                                                                                                    |
| `"has_available_mission"`       | 制限付き文字列 | そのNPCが、プレイヤーに対して依頼できるミッションを1つだけ持っていれば `true` を返します。                                                                                                                                                                                                                                                                      |
| `"has_many_available_missions"` | 制限付き文字列 | そのNPCが、プレイヤーに対して依頼できるミッションを複数持っていれば `true` を返します。                                                                                                                                                                                                                                                                         |
| `"mission_goal"`                | 文字列         | そのNPCが現在提示(または進行)しているミッションの目的が、指定した `mission_goal` と一致すれば `true` を返します。                                                                                                                                                                                                                                               |
| `"mission_complete"`            | 制限付き文字列 | プレイヤーが、そのNPCから受けた現在のミッションを完了していれば `true` を返します。                                                                                                                                                                                                                                                                             |
| `"mission_incomplete"`          | 制限付き文字列 | プレイヤーが、そのNPCから受けた現在のミッションをまだ完了していなければ `true` を返します。                                                                                                                                                                                                                                                                     |
| `"mission_has_generic_rewards"` | 制限付き文字列 | そのNPCが現在提示しているミッションに、汎用報酬のフラグが設定されていれば `true` を返します。                                                                                                                                                                                                                                                                   |
| `"npc_service"`                 | 制限付き文字列 | そのNPCに `currently_busy`(現在多忙)のエフェクトが付与されていなければ true を返します。 この判定は、完了までに時間を要するタスク(拠点の拡張、大規模なクラフト、長期間の派遣など)をNPCに依頼できるかどうかを確認する際に有用です。 機能的には `not": { "npc_has_effect": "currently_busy" }` と全く同じであり、`npc_available` という表記でも同様に動作します。 |
| `"npc_allies"`                  | 整数           | プレイヤーに同行している味方NPCの数が、指定した数以上であれば `true` を返します                                                                                                                                                                                                                                                                                 |
| `"npc_following"`               | 制限付き文字列 | そのNPCがプレイヤーに追従している状態であれば `true` を返します。                                                                                                                                                                                                                                                                                               |
| `"is_by_radio"`                 | 制限付き文字列 | プレイヤーがそのNPCと無線機越しに会話していれば `true` を返します。                                                                                                                                                                                                                                                                                             |

#### NPC専用条件 一覧

| 条件                 | 型             | 説明                                                                                                                         |
| -------------------- | -------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `"npc_available"`    | 制限付き文字列 | NPCに `currently_busy`(現在多忙)のエフェクトが付与されていなければ `true` を返します。                                       |
| `"npc_following"`    | 制限付き文字列 | NPCがプレイヤーに追従している状態であれば `true` を返します。                                                                |
| `"npc_friend"`       | 制限付き文字列 | NPCがプレイヤーに対して友好的であれば `true` を返します。                                                                    |
| `"npc_hostile"`      | 制限付き文字列 | NPCがプレイヤーに対して敵対的であれば `true` を返します。                                                                    |
| `"npc_train_skills"` | 制限付き文字列 | NPCのスキルの中に、プレイヤーのレベルを上回るものが1つ以上あれば `true` を返します。                                         |
| `"npc_train_styles"` | 制限付き文字列 | NPCが、プレイヤーが習得していない格闘術を1つ以上知っていれば `true` を返します。                                             |
| `"npc_has_class"`    | 配列           | NPCが、指定された特定のNPCクラス(職業/属性)に属していれば `true` を返します。                                                |
| `"npc_role_nearby"`  | 文字列         | 100タイル以内に、`npc_role_nearby` で指定した「随行任務ロール(companion mission role)」を持つNPCがいれば `true` を返します。 |
| `"has_reason"`       | 制限付き文字列 | 直前のエフェクトによって、処理を完了できなかった「理由(reason)」が設定されていれば `true` を返します。                       |

#### NPC同行者 AIルール条件

| 条件                    | 型     | 説明                                                                              |
| ----------------------- | ------ | --------------------------------------------------------------------------------- |
| `"npc_aim_rule"`        | 文字列 | NPC同行者の「照準ルール」の設定が、指定した文字列と一致すれば `true` を返します。 |
| `"npc_engagement_rule"` | 文字列 | NPC同行者の「交戦ルール」の設定が、指定した文字列と一致すれば `true` を返します。 |
| `"npc_rule"`            | 文字列 | 指定した名前のNPC同行者AIルールが設定されていれば `true` を返します。             |

#### 環境条件

| 条件                     | 型             | 説明                                                                                                                                                                             |
| ------------------------ | -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"days_since_cataclysm"` | 整数           | 大災厄が発生してから、指定した日数以上が経過していれば true を返します。                                                                                                         |
| `"is_season"`            | 文字列         | 現在の季節が指定した `is_season` と一致すれば `true` を返します。 指定できる値は、`"spring"`(春)、`"summer"`(夏)、`"autumn"`(秋)、`"winter"`(冬)のいずれかである必要があります。 |
| `"is_day"`               | 制限付き文字列 | 現在が昼間(日の出から日没までの間)であれば `true` を返します。                                                                                                                   |
| `"is_outside"`           | 制限付き文字列 | NPCが現在立っているタイルに屋根がなければ `true` を返します。                                                                                                                    |

#### ミッション報告と報酬の受け取り

```json
{
  "text": "Understood.  I'll get those antibiotics.",
  "topic": "TALK_NONE",
  "condition": { "npc_has_effect": "infected" }
},
{
  "text": "I'm sorry for offending you.  I predict you will feel better in exactly one hour.",
  "topic": "TALK_NONE",
  "effect": { "npc_add_effect": "deeply_offended", "duration": 600 }
},
{
  "text": "Nice to meet you too.",
  "topic": "TALK_NONE",
  "effect": { "u_add_effect": "has_met_example_NPC", "duration": "PERMANENT" }
},
{
  "text": "Nice to meet you too.",
  "topic": "TALK_NONE",
  "condition": {
    "not": {
      "npc_has_var": "has_met_PC", "type": "general", "context": "examples", "value": "yes"
    }
  },
  "effect": {
    "npc_add_var": "has_met_PC", "type": "general", "context": "examples", "value": "yes" }
  }

{
  "text": "[INT 11] I'm sure I can organize salvage operations to increase the bounty scavengers bring in!",
  "topic": "TALK_EVAC_MERCHANT_NO",
  "condition": { "u_has_intelligence": 11 }
},
{
  "text": "[STR 11] I punch things in face real good!",
  "topic": "TALK_EVAC_MERCHANT_NO",
  "condition": { "and": [ { "not": { "u_has_intelligence": 7 } }, { "u_has_strength": 11 } ] }
},
{ "text": "[100 Merch, 1d] 10 logs", "topic": "TALK_DONE", "effect": "buy_10_logs", "condition":
{ "and": [ "npc_service", { "u_has_items": { "item": "FMCNote", "count": 100 } } ] } },
{ "text": "Maybe later.", "topic": "TALK_RANCH_WOODCUTTER", "condition": "npc_available" },
{
  "text": "[1 Merch] I'll take a beer",
  "topic": "TALK_DONE",
  "condition": { "u_has_item": "FMCNote" },
  "effect": [{ "u_sell_item": "FMCNote" }, { "u_buy_item": "beer", "container": "bottle_glass", "count": 2 }]
},
{
  "text": "Okay.  Lead the way.",
  "topic": "TALK_DONE",
  "condition": { "not": "at_safe_space" },
  "effect": "lead_to_safety"
},
{
  "text": "About one of those missions...",
  "topic": "TALK_MISSION_LIST_ASSIGNED",
  "condition": { "and": [ "has_many_assigned_missions", { "u_is_wearing": "badge_marshal" } ] }
},
{
  "text": "[MISSION] The captain sent me to get a frequency list from you.",
  "topic": "TALK_OLD_GUARD_NEC_COMMO_FREQ",
  "condition": {
    "and": [
      { "u_is_wearing": "badge_marshal" },
      { "u_has_mission": "MISSION_OLD_GUARD_NEC_1" },
      { "not": { "u_has_effect": "has_og_comm_freq" } }
    ]
  }
},
{
    "text": "I killed them.  All of them.",
    "topic": "TALK_MISSION_SUCCESS",
    "condition": {
        "and": [ { "or": [ { "mission_goal": "KILL_MONSTER_SPEC" }, { "mission_goal": "KILL_MONSTER_TYPE" } ] }, "mission_complete" ]
    },
    "switch": true
},
{
    "text": "Glad to help.  I need no payment.",
    "topic": "TALK_NONE",
    "effect": "clear_mission",
    "mission_opinion": { "trust": 4, "value": 3 },
    "opinion": { "fear": -1, "anger": -1 }
},
{
    "text": "Maybe you can teach me something as payment?",
    "topic": "TALK_TRAIN",
    "condition": { "or": [ "npc_train_skills", "npc_train_styles" ] },
    "effect": "mission_reward"
},
{
    "truefalsetext": {
        "true": "I killed him.",
        "false": "I killed it.",
        "condition": { "mission_goal": "ASSASSINATE" }
    },
    "condition": {
        "and": [
            "mission_incomplete",
            {
                "or": [
                    { "mission_goal": "ASSASSINATE" },
                    { "mission_goal": "KILL_MONSTER" },
                    { "mission_goal": "KILL_MONSTER_SPEC" },
                    { "mission_goal": "KILL_MONSTER_TYPE" }
                ]
            }
        ]
    },
    "trial": { "type": "LIE", "difficulty": 10, "mod": [ [ "TRUST", 3 ] ] },
    "success": { "topic": "TALK_NONE" },
    "failure": { "topic": "TALK_MISSION_FAILURE" }
},
{
  "text": "Didn't you say you knew where the Vault was?",
  "topic": "TALK_VAULT_INFO",
  "condition": { "not": { "u_has_var": "asked_about_vault", "value": "yes", "type": "sentinel", "context": "old_guard_rep" } },
  "effect": [
      { "u_add_var": "asked_about_vault", "value": "yes", "type": "sentinel", "context": "old_guard" },
      { "mapgen_update": "hulk_hairstyling", "om_terrain": "necropolis_a_13", "om_special": "Necropolis", "om_terrain_replace": "field", "z": 0 }
  ]
},
{
  "text": "Why do zombies keep attacking every time I talk to you?",
  "topic": "TALK_RUN_AWAY_MORE_ZOMBIES",
  "condition": { "u_has_var": "even_more_zombies", "value": "yes", "type": "trigger", "context": "learning_experience" },
  "effect": [
    { "mapgen_update": [ "even_more_zombies", "more zombies" ], "origin_npc": true },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_x": 1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_x": -1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_y": 1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_y": -1 }
  ]
}
```
