# シナリオ

シナリオは、`type` メンバーを `scenario` に設定した JSON オブジェクトで指定します。

```json
{
    "type": "scenario",
    "id": "schools_out",
    ...
}
```

`id` メンバーにはシナリオの一意な ID を指定します。

以下のプロパティに対応しています（特記がない限り必須です）。

## `description`

（文字列）

ゲーム内で表示する説明。

## `name`

（文字列、または `male` と `female` メンバーを持つオブジェクト）

ゲーム内で表示する名前。性別に依存しない文字列 1 つ、または性別ごとの名前を持つオブジェクトを指定します。例:

```json
"name": {
    "male": "Runaway groom",
    "female": "Runaway bride"
}
```

## `points`

（整数）

シナリオのポイントコスト。正の値はポイントを消費し、負の値はポイントを与えます。

## `items`

（任意。`both`、`male`、`female` を任意で持つオブジェクト）

このシナリオを選んだときにプレイヤーが開始時に持つアイテム。キャラクターの性別に応じて異なるアイテムを指定できます。各アイテム一覧はアイテム ID の配列です。同じ ID を複数回指定すると、その回数だけアイテムが作成されます。

例:

```json
"items": {
    "both": [
        "pants",
        "rock",
        "rock"
    ],
    "male": [ "briefs" ],
    "female": [ "panties" ]
}
```

この例では、プレイヤーにズボン、石 2 個、そして性別に応じてブリーフまたはパンティーを与えます。

Mod は `add:both` / `add:male` / `add:female` と `remove:both` / `remove:male` / `remove:female` を使って既存シナリオの一覧を変更できます。

Mod の例:

```json
{
  "type": "scenario",
  "id": "schools_out",
  "edit-mode": "modify",
  "items": {
    "remove:both": ["rock"],
    "add:female": ["2x4"]
  }
}
```

## `surround_groups`

（任意。グループと密度の数値を含む配列）

シナリオの `SUR_START` フラグを置き換え、シナリオの開始地点を囲む地域に生成するモンスターグループを指定します。

```json
"surround_groups": [ [ "GROUP_BLACK_ROAD", 70.0 ] ],
```

文字列は周囲に生成するモンスターグループの ID、数値は生成密度です。70.0 は従来の `SUR_START` の動作と同じ値です。

## `flags`

（任意。文字列の配列）

フラグの一覧。TODO: ここでフラグを文書化する。

Mod は `add:flags` と `remove:flags` で変更できます。

## `cbms`

（任意。文字列の配列）

キャラクターに埋め込む CBM ID の一覧。

Mod は `add:CBMs` と `remove:CBMs` で変更できます。

## `traits", "forced_traits", "forbidden_traits`

（任意。文字列の配列）

特性・変異 ID の一覧。`forbidden_traits` の特性は禁止され、キャラクター作成中に選択できません。`forced_traits` の特性はキャラクターに自動的に追加されます。`traits` の特性は開始特性でなくても選択できるようになります。

## `bionics", "forced_bionics", "forbidden_bionics`

（任意。文字列の配列）

生体工学 ID の一覧。`forbidden_bionics` の生体工学はキャラクター作成中に選択できません。`forced_bionics` の生体工学はキャラクターに自動的に追加されます。`bionics` の生体工学は開始時の生体工学でなくても選択できるようになります。

## `spells", "forbidden_spells`

（任意。文字列の配列）

呪文 ID の一覧。`forbidden_spells` の呪文は禁止され、キャラクター作成中に選択できません。`spells` の呪文は開始呪文でなくても選択できるようになります。

## `forbids_bionics`

（任意。真偽値）

生体工学のキャラクター作成タブからプレイヤーが生体工学を追加することを禁止します。

## `allowed_locs`

（任意。文字列の配列）

このシナリオで選択できる開始地点 ID の一覧（`start_locations.json` を参照）。

## `start_name`

（文字列）

開始地点として表示する名前。シナリオが複数の開始地点を許可しているものの、ゲームがシナリオの説明にすべてを同時に表示できない場合に便利です。たとえば荒野のどこかから開始できるシナリオでは、開始地点に forest と fields を含めつつ、`start_name` は単に「荒野」と表示できます。

## `professions`

（任意。文字列の配列）

このシナリオで選択できる職業の一覧。最初の項目がデフォルトの職業です。空の場合はすべての職業を選択できます。

## `map_special`

（任意。文字列）

開始地点にマップスペシャルを追加します。指定できるスペシャルについては json_flags を参照してください。

## `missions`

（任意。文字列の配列）

ゲーム開始時に開始され、プレイヤーに割り当てられるミッション ID の一覧。`ORIGIN_GAME_START` 起源のミッションだけを指定できます。複数のミッションを割り当てた場合、一覧の最後のミッションがアクティブになります。
