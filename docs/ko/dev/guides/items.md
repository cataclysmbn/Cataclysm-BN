# 아이템과 탄약 쿡북

아이템 작업에서 자주 쓰는 패턴과, 아이템/탄약/탄창/충전량 처리를 일관되게 유지하는
규칙입니다. 아이템 게임 객체에 대한 자세한 내용은 [게임 객체](../explanation/game_objects.md)를
참고하세요. JSON 아이템 필드는 모딩 문서의
[아이템 생성](../../mod/json/reference/items/item_creation.md),
[아이템 스폰](../../mod/json/reference/items/item_spawn.md),
[제작법](../../mod/json/reference/items/recipes.md)을 참고하세요.

## 기본 모델

`item`은 여러 개념을 나타낼 수 있습니다.

- 드라이버나 배낭 같은 일반 물체.
- 탄약, 음식 일부, 액체, 부품처럼 charges로 개수를 세는 스택.
- 탄약이나 에너지를 저장한 총기 또는 도구.
- 탄약을 내용물 아이템으로 저장하는 탄창.
- 다른 아이템을 담고 있는 컨테이너.

`item::charges`가 항상 같은 뜻이라고 가정하지 마세요. 아이템 타입에 따라 charges는
스택 개수, 도구 에너지, 남은 탄약, 또는 의미 없는 값일 수 있습니다. 아래의 상위 수준
아이템 API를 우선 사용하세요.

## 알맞은 API 선택하기

| 작업                                   | 권장 API                                          | 피할 것                            |
| -------------------------------------- | ------------------------------------------------- | ---------------------------------- |
| 장전된 탄약 타입 확인                  | `item::ammo_current()`                            | `curammo` 직접 읽기                |
| 장전량 확인                            | `item::ammo_remaining()`                          | `charges` 직접 읽기                |
| 용량 확인                              | `item::ammo_capacity()`                           | JSON 필드로 직접 재계산            |
| 총기/도구/탄창에 탄약 장전             | `item::ammo_set()`                                | `charges`와 탄약 필드 직접 설정    |
| 총기/도구/탄창 비우기                  | `item::ammo_unset()` 또는 `recover_stored_ammo()` | `charges`만 지우기                 |
| 장전된 아이템에서 탄약 소비            | `item::ammo_consume()`                            | `charges`에서 직접 빼기            |
| 인벤토리/맵 charges 소비               | `Character::use_charges()`, `map::use_charges()`  | 직접 인벤토리 순회                 |
| 온전한 아이템 재료 소비                | `Character::use_amount()`, `map::use_amount()`    | 직접 detach 루프 작성              |
| 저장된 탄약을 인벤토리 아이템으로 변환 | `stored_ammo.h` 헬퍼                              | `item::spawn()`과 임시 charge 계산 |

직접 필드 접근은 불변식이 해당 위치에서 명확한 저수준 아이템 구현 코드에서만 사용하세요.

## 한 tripoint에서 다른 tripoint로 아이템 이동하기

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    auto items_src = here.i_at( src );
    auto items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```

## 탄약이나 charges가 있는 아이템 스폰하기

일반 count-by-charges 아이템은 charges를 지정해서 스폰해도 됩니다.

```cpp
auto nails = item::spawn( "nail", calendar::turn, 50 );
```

총기, 도구, 탄창은 스폰한 뒤 `ammo_set()`을 우선 사용하세요. 이 함수는 대상이 내부
탄약 저장소를 쓰는지, 내용물 탄창/탄약 아이템을 쓰는지 알고 있습니다.

```cpp
auto cell = item::spawn( "medium_battery_cell" );
cell->ammo_set( itype_id( "battery" ), 500 );
```

아이템 내부 구현에서 관련 탄약 불변식을 모두 함께 유지하는 경우가 아니라면, `charges`를
직접 써서 장전된 탄약을 초기화하지 마세요.

## 총기, 도구, 탄창

장전된 아이템은 같은 공개 질의 함수를 사용하세요.

```cpp
auto describe_loaded_ammo( const item &it ) -> std::string
{
    if( it.ammo_current().is_null() || it.ammo_remaining() <= 0 ) {
        return "empty";
    }
    return string_format( "%s: %d", it.ammo_current().str(), it.ammo_remaining() );
}
```

저장 방식은 아이템 타입마다 다릅니다.

- 내부 저장식 총기/도구는 보통 탄약량을 `charges`에 저장하고 탄약 타입은 따로 저장합니다.
- 탈착식 탄창을 쓰는 총기/도구는 탄약 질의를 탄창으로 전달합니다.
- 탄창은 탄약을 내용물 탄약 아이템으로 저장합니다.

따라서 호출자는 저장 구조를 검사하지 말고 `ammo_current()`, `ammo_remaining()`,
`ammo_set()`, `ammo_unset()`, `ammo_consume()`을 사용해야 합니다.

## 아이템 안에 저장된 탄약이나 charges 회수하기

일부 아이템은 탄약을 추상적인 charges로 저장하지만, 인벤토리에 나타나는 물리 아이템은
다른 단위를 쓸 수 있습니다. 중요한 특수 사례는 플루토늄 연료입니다. 도구와 차량 부품은
플루토늄을 `PLUTONIUM_CHARGES` 단위로 저장하지만, 인벤토리 연료전지는 온전한 아이템입니다.
부분 플루토늄 charges를 `item::spawn`과 수동 `charges` 대입으로 변환하면 0-charge 유령
아이템을 만들 수 있으므로 금지합니다.

총기, 도구, 탄창, 차량 연료 저장소의 저장된 탄약을 물리 아이템으로 바꿀 때는
`stored_ammo.h`를 사용하세요.

```cpp
for( auto &ammo : recover_stored_ammo( source_item, stored_ammo_remainder_handling::discard ) ) {
    drop_or_handle( std::move( ammo ), who );
}
```

나머지 처리 정책을 명시적으로 선택하세요.

- `stored_ammo_remainder_handling::discard`: 제작, 분해, 해체처럼 원본 아이템이 파괴될 때
  사용합니다. 온전한 탄약 아이템은 회수하고, 아이템으로 존재할 수 없는 나머지는 지웁니다.
- `stored_ammo_remainder_handling::preserve`: 차량에서 온전한 연료 아이템만 빼고 부분 연료는
  부품에 남기는 경우처럼 원본이 세계에 남을 때 사용합니다.

물리 표현을 검사하거나 스폰하기만 하는 코드에는 `stored_ammo_item_charges`,
`stored_ammo_charges_for_items`, `spawn_stored_ammo`를 사용하세요. 이렇게 하면 charge에서
아이템으로 변환하는 경로가 하나로 문서화되고, 호출자가 플루토늄 반올림 규칙을 다시 구현하지
않아도 됩니다.

## 차량 연료 저장소

차량 부품에는 별도의 탄약/연료 저장소가 있습니다. 부품 내부를 직접 읽지 말고
`vehicle::fuel_left()`, `vehicle::fuels_left()`, `vehicle::drain()`, 부품 탄약 API 같은 차량
API를 사용하세요.

고체 차량 연료를 인벤토리 아이템으로 꺼낼 때도 최종 저장 charge에서 아이템 charge로
변환하는 단계는 `stored_ammo.h`를 사용하세요. 이렇게 하면 온전한 아이템으로 표현할 수 없는
부분 연료를 보존할 수 있습니다.

```cpp
const auto stored_charges = veh.fuel_left( fuel );
auto fuel_item = spawn_stored_ammo( fuel, stored_charges );
if( fuel_item ) {
    veh.drain( fuel, stored_ammo_charges_for_items( fuel, fuel_item->charges ) );
}
```

## `NO_UNLOAD`와 회수 정책

아이템의 `NO_UNLOAD`는 플레이어가 일반 unload UI로 그 아이템을 비울 수 없어야 한다는 뜻입니다.
제작, 분해, 해체로 담고 있는 아이템이 파괴될 때 무슨 일이 일어나야 하는지를 자동으로 정하지는
않습니다. 그런 경로에서는 정책을 명시하고 테스트를 추가하세요.

- 원본 아이템이 살아남으면 아이템으로 표현할 수 없는 나머지를 보존합니다.
- 원본 아이템이 소비되면 온전한 탄약 아이템을 회수하고, 인벤토리 아이템으로 존재할 수 없는
  나머지는 버립니다.

## 테스트 체크리스트

아이템, 탄약, 연료 코드를 바꿀 때는 저장 구조와 경계 조건을 테스트하세요.

- 내부 탄약을 쓰는 도구 또는 총기.
- 충전식 배터리 셀처럼 내용물 탄약을 가진 탄창.
- 제작/분해 재료로 소비되는 원본 아이템.
- unload 뒤에도 세계에 남는 원본 아이템.
- 탄약 없음과 물리 아이템 1개 미만의 부분 탄약, 특히 플루토늄 연료.
- 여러 개의 온전한 물리 아이템과 부분 나머지가 함께 있는 경우.

회수된 아이템 스택과 원본에 남은 탄약을 모두 확인하는 테스트를 선호하세요.
