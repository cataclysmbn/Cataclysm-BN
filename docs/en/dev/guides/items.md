# Items Cookbook

Here are some common tasks that you might want to do with items.
[For more on item(game objects), check here.](../explanation/game_objects.md)

## Processing nested items

Top-level map and vehicle active item caches track the outer item that is placed in the world. If a
nested item needs per-turn or periodic processing, its parent containers must also report that they
need processing so the outer item stays in the cache.

Use the normal item and `item_contents` mutation APIs when changing active state, processing-related
flags, or contents. These paths invalidate the cached processing-item list up the parent chain and
resync the placed top-level item with the map or vehicle active cache when needed. Avoid changing
`item_tags`, `item_contents` storage, or active state directly unless the caller also invalidates the
processing cache upward.

Processing cadence is inherited from the fastest processing child. A food container with an active
tool inside it must keep the active tool's one-turn cadence even though food-only containers can use
a slower rot cadence. If a mutation can change `item::processing_speed()`, re-adding the outer item
to the active cache moves an existing reference to the correct speed queue instead of leaving stale
entries behind.

When processing contents recursively, take a mutation-safe snapshot of `contents.processing_items()`
before detaching or processing children. Preserve parent container context such as sealed or
preserving containers while processing nested food so rot behavior remains unchanged.

## Moving items from one tripoint to another

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    map_stack items_src = here.i_at( src );
    map_stack items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```
