# アイテム クックブック

ここでは、アイテムを使って実行したい一般的なタスクをいくつか紹介します。
[アイテム(ゲームオブジェクト)の詳細については、こちらを確認してください](../explanation/game_objects.md)。

## あるtripointから別のtripointへのアイテムの移動

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    map_stack items_src = here.i_at( src );
    map_stack items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```
