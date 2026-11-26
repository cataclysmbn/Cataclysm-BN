# 開始地点

開始地点は、`type` メンバーを `start_location` に設定した JSON オブジェクトで指定します。

```json
{
    "type": "start_location",
    "id": "field",
    "name": "An empty field",
    "target": "field",
    ...
}
```

`id` メンバーには地点の一意な ID を指定します。

以下のプロパティに対応しています（特記がない限り必須です）。

## `name`

（文字列）

ゲーム内で表示する地点の名前。

## `target`

（文字列）

開始地点のオーバーマップ地形タイプの ID（`overmap_terrain.json` を参照）。ゲームはその地形を持つ場所をランダムに選びます。

## `flags`

（任意。文字列の配列）

任意のフラグ。Mod は `add:flags` / `remove:flags` で変更できます。TODO: フラグを文書化する。
