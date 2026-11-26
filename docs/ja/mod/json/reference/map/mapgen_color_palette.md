# Mapgen カラーパレット

```jsonc
{
  "type": "mapgen_color_palette", // 必須の type
  "id": "plaster_wall_palette", // パレットの ID
  "colors": [
    "Emerald Green", // 名前付き色
    "Purple", // あいまい一致する名前付き色
    "Carmine Red", // デフォルトの重みは 100
    {
      "color": "#ffffff", // 16進数コードも使用可能
      "weight": 600, // 指定した重み
    },
  ],
}
```
