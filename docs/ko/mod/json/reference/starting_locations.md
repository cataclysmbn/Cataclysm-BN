# 시작 위치

시작 위치는 "type" 멤버가 "start_location"으로 설정된 JSON 객체로 지정됩니다:

```json
{
    "type": "start_location",
    "id": "field",
    "name": "An empty field",
    "target": "field",
    ...
}
```

id 멤버는 위치의 고유 id여야 합니다.

다음 속성들이 지원됩니다 (별도 표기가 없으면 필수):

## `name`

(문자열)

위치의 게임 내 이름.

## `target`

(문자열)

시작 위치의 오버맵 지형 타입의 id (overmap_terrain.json 참조). 게임은 해당 지형을 가진 무작위 장소를 선택합니다.

## `flags`

(선택사항, 문자열 배열)

임의의 플래그. 모드는 "add:flags" / "remove:flags"를 통해 이것을 수정할 수 있습니다. TODO: 문서화 필요.
