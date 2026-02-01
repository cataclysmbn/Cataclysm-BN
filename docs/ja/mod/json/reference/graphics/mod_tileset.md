# MODタイルセット

MODタイルセットは、追加のスプライトシートを定義します。これはJSONオブジェクトとして指定し、`type` メンバーに `mod_tileset` を設定することで機能します。

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

対応するタイルセットの内部IDを指定します。MODタイルセットは、現在使用されているベースタイルセットのIDがこのフィールドに含まれている場合にのみ適用されます。

## `tiles-new`

スプライトシートの設定です。`tile_config` 内の `tiles-new` フィールドと同じ形式で記述します。スプライトの画像ファイルは、このJSONファイルが存在するフォルダと同じ場所から読み込まれます。
