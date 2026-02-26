# 아이템 쿡북

아이템과 관련하여 자주 수행하는 작업들입니다.
[아이템(게임 객체)에 대한 자세한 내용은 여기를 확인하세요.](../explanation/game_objects.md)

## 한 tripoint에서 다른 tripoint로 아이템 이동하기

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    map_stack items_src = here.i_at( src );
    map_stack items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```
