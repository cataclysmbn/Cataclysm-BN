# Mapgen 색상 팔레트

```jsonc
{
  "type": "mapgen_color_palette", // 필수 타입
  "id": "plaster_wall_palette", // 팔레트 ID
  "colors": [
    "Emerald Green", // 색상 이름
    "Purple", // 퍼지 매칭되는 색상 이름
    "Carmine Red", // 기본 가중치는 100
    {
      "color": "#ffffff", // 16진수 코드도 가능
      "weight": 600, // 지정한 가중치
    },
  ],
}
```
