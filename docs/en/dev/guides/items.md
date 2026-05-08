# Items Cookbook

Here are some common tasks that you might want to do with items.
[For more on item(game objects), check here.](../explanation/game_objects.md)

## Moving items from one tripoint to another

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    map_stack items_src = here.i_at( src );
    map_stack items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```
