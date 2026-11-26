# MODタイルセット

MODタイルセットは追加のスプライトシートを定義します。`type` メンバーを `mod_tileset` に設定したJSONオブジェクトとして指定します。

例:

```json
[
  {
    "type": "mod_tileset",
    "compatibility": ["MshockXottoplus"],
    "tiles-new": [
      {
        "file": "test_tile.png",
        "tiles": [
          {
            "id": "player_female",
            "fg": 1,
            "bg": 0
          },
          {
            "id": "player_male",
            "fg": 2,
            "bg": 0
          }
        ]
      }
    ]
  }
]
```

## `compatibility`

(文字列型)

対応するタイルセットの内部IDです。ベースタイルセットのIDがこのフィールドに含まれている場合にのみ、MODタイルセットが適用されます。

## `tiles-new`

スプライトシートの設定です。`tile_config` の `tiles-new` フィールドと同じ形式です。スプライト画像は、このJSONファイルが存在するフォルダから読み込まれます。

## `state-modifiers`

状態モディファイアを使うと、MODタイルセットでキャラクターの状態に応じたUVベースのスプライト変更を定義または上書きできます。ベースタイルセットと同じ `id` を持つ状態モディファイアグループをMODタイルセットが定義すると、MODの定義がベースタイルセットの定義を置き換えます。

```json
{
  "type": "mod_tileset",
  "compatibility": ["UndeadPeopleTileset"],
  "tiles-new": [
    {
      "file": "uv-tiles.png",
      "tiles": [],
      "state-modifiers": [
        {
          "id": "movement_mode",
          "override": false,
          "use_offset": false,
          "tiles": [
            { "id": "walk", "fg": null },
            { "id": "crouch", "fg": 1 },
            { "id": "run", "fg": 2 }
          ]
        }
      ]
    }
  ]
}
```

`state-modifiers` 配列は `tiles-new` エントリ内で `tiles` と並べて指定します。各モディファイアグループには次の項目が必要です。

| フィールド   | 型     | 説明                                                         |
| ------------ | ------ | ------------------------------------------------------------ |
| `id`         | string | グループ識別子（`movement_mode`、`downed`、`lying_down`）。  |
| `override`   | bool   | この状態が有効なとき、優先度の低いグループをスキップします。 |
| `use_offset` | bool   | オフセットモードでは `true`、正規化UVモードでは `false`。    |
| `tiles`      | array  | 状態とスプライトのマッピング。                               |

UVマッピングモードとモディファイア用スプライトの作成方法については、[タイルセットの状態モディファイア節](./tileset.md#state-modifiers)を参照してください。
