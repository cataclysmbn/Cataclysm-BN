# ter_furn_transform

ter_furn_transform는 한 타일을 한 지형에서 다른 지형으로, 한 가구에서 다른 가구로 변환하는 것을 지정할 수 있는 JSON 객체 유형입니다.

```json
[
  {
    "type": "ter_furn_transform",
    "id": "example",
    "terrain": [
      {
        "result": "t_dirt",
        "valid_terrain": ["t_sand"],
        "message": "sandy!",
        "message_good": true
      }
    ]
  }
]
```

위의 예는 "모래"를 "흙"으로 바꿉니다. 직접적인 지형 ID를 비교하여 그렇게 합니다. 또한 변환에 실패 메시지를 추가할 수 있습니다. 그러나 모래를 "흙 또는 풀"로 바꾸고 싶다면 다음과 같이 할 수 있습니다:

```json
"terrain": [
  {
    "fail_message": "no sand!",
    "result": [ "t_dirt", "t_grass" ],
    "valid_terrain": [ "t_sand" ],
    "message": "sandy!"
  }
]
```

message_good는 선택 사항이며 기본값은 true입니다. 이 예는 흙 또는 풀을 1:1 비율로 선택합니다. 그러나 4:1 비율을 원한다면:

```json
"terrain": [
  {
    "result": [ [ "t_dirt", 4 ], "t_grass" ],
    "valid_terrain": [ "t_sand" ],
    "message": "sandy!"
  }
]
```

보시다시피 가중치가 있는 배열과 단일 문자열을 혼합하고 일치시킬 수 있습니다. 각 단일 문자열은 가중치가 1입니다.

위의 모든 것은 가구에도 적용됩니다.

```json
"furniture": [
  {
    "result": [ [ "f_null", 4 ], "f_chair" ],
    "valid_furniture": [ "f_hay", "f_woodchips" ],
   "message": "I need a chair"
  }
]
```

가구와 지형 모두에 대해 특정 ID 대신 플래그를 사용할 수도 있습니다.

```json
"terrain": [
  {
    "result": "t_floor",
    "valid_flags": [ "FLAT" ],
    "message": "flooring"
  }
]
```

모든 파낼 수 있는 지형을 대상으로 하려면 불리언을 사용하세요.

```json
"terrain": [
  {
    "result": "t_dirt",
    "diggable": true,
    "message": "digdug"
  }
]
```

ter_furn_transform는 지형과 가구 필드를 모두 가질 수 있습니다. 별도로 처리하므로 "흙이면 의자 추가"는 불가능합니다.
