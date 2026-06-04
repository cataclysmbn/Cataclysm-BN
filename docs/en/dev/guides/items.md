# Items and Ammo Cookbook

Here are common item tasks and the rules that keep item, ammo, magazine, and charge handling
consistent. For more on item game objects, see [Game Objects](../explanation/game_objects.md).
For JSON item fields, see the modding references for
[item creation](../../mod/json/reference/items/item_creation.md),
[item spawning](../../mod/json/reference/items/item_spawn.md), and
[recipes](../../mod/json/reference/items/recipes.md).

## Mental model

An `item` can represent several different concepts:

- A normal object, such as a screwdriver or backpack.
- A count-by-charges stack, such as ammo, food portions, liquid, or components.
- A gun or tool with stored ammo/energy.
- A magazine that stores ammo as contained ammo items.
- A container that happens to hold other items.

Do not assume that `item::charges` always means the same thing. Depending on the item type, charges
may be stack count, tool energy, ammo remaining, or irrelevant. Prefer the higher-level item APIs
listed below.

## Choosing the right API

| Task                                  | Preferred API                                    | Avoid                                      |
| ------------------------------------- | ------------------------------------------------ | ------------------------------------------ |
| Query loaded ammo type                | `item::ammo_current()`                           | Reading `curammo` directly                 |
| Query loaded amount                   | `item::ammo_remaining()`                         | Reading `charges` directly                 |
| Query capacity                        | `item::ammo_capacity()`                          | Recalculating from JSON fields             |
| Load ammo into gun/tool/magazine      | `item::ammo_set()`                               | Manually setting `charges` and ammo fields |
| Empty gun/tool/magazine               | `item::ammo_unset()` or `recover_stored_ammo()`  | Clearing only `charges`                    |
| Consume ammo from a loaded item       | `item::ammo_consume()`                           | Subtracting from `charges`                 |
| Consume inventory/map charges         | `Character::use_charges()`, `map::use_charges()` | Manual inventory scans                     |
| Consume whole item components         | `Character::use_amount()`, `map::use_amount()`   | Manual detach loops                        |
| Turn stored ammo into inventory items | `stored_ammo.h` helpers                          | `item::spawn()` plus ad-hoc charge math    |

Use direct field access only in low-level item implementation code where the invariant is local and
obvious.

## Moving items from one tripoint to another

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    auto items_src = here.i_at( src );
    auto items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```

## Spawning items with ammo or charges

For a plain count-by-charges item, spawning with charges is fine:

```cpp
auto nails = item::spawn( "nail", calendar::turn, 50 );
```

For guns, tools, and magazines, prefer `ammo_set()` after spawning. It knows whether the target uses
integral ammo storage or a contained magazine/ammo item.

```cpp
auto cell = item::spawn( "medium_battery_cell" );
cell->ammo_set( itype_id( "battery" ), 500 );
```

Do not initialize loaded ammo by manually writing `charges` unless you are in item internals and
also maintain all related ammo invariants.

## Guns, tools, and magazines

Use the same public queries for all loaded items:

```cpp
auto describe_loaded_ammo( const item &it ) -> std::string
{
    if( it.ammo_current().is_null() || it.ammo_remaining() <= 0 ) {
        return "empty";
    }
    return string_format( "%s: %d", it.ammo_current().str(), it.ammo_remaining() );
}
```

The storage layout differs by item type:

- Integral guns/tools often store ammo amount in `charges` and ammo type separately.
- Guns/tools with detachable magazines forward ammo queries to the magazine.
- Magazines store ammo as contained ammo items.

That is why callers should use `ammo_current()`, `ammo_remaining()`, `ammo_set()`, `ammo_unset()`,
and `ammo_consume()` instead of inspecting the layout.

## Recovering ammo or charges stored inside items

Some items store ammo as abstract charges, while the physical item that appears in the inventory may
use different units. Plutonium fuel is the important special case: tools and vehicle parts store it
in `PLUTONIUM_CHARGES` units, but inventory fuel cells are whole items. Partial plutonium charges
must not be converted with raw `item::spawn` plus manual `charges` assignment, because that can
create zero-charge ghost items.

Use `stored_ammo.h` whenever code turns stored ammo from guns, tools, magazines, or vehicle fuel
stores into physical items:

```cpp
for( auto &ammo : recover_stored_ammo( source_item, stored_ammo_remainder_handling::discard ) ) {
    drop_or_handle( std::move( ammo ), who );
}
```

Choose the remainder policy explicitly:

- `stored_ammo_remainder_handling::discard`: use when the source item is being destroyed, such as
  disassembly or salvage. Recover complete ammo items and clear unitemizable remainders.
- `stored_ammo_remainder_handling::preserve`: use when the source remains in the world, such as
  unloading complete fuel items from a vehicle while leaving partial fuel in the part.

Crafting is a two-stage case: preserve stored ammo while the in-progress craft owns the components,
then on completion load that ammo into a compatible result before recovering any leftovers as
physical items. This prevents charged battery-cell components from turning into loose `battery`
items when the recipe result is another battery cell.

For code that only needs to inspect or spawn the physical representation, use
`stored_ammo_item_charges`, `stored_ammo_charges_for_items`, and `spawn_stored_ammo`. This keeps
charge-to-item conversion in one documented path and prevents each caller from reimplementing
plutonium rounding rules.

## Vehicle fuel stores

Vehicle parts have their own ammo/fuel storage. Use vehicle APIs such as `vehicle::fuel_left()`,
`vehicle::fuels_left()`, `vehicle::drain()`, and part ammo APIs instead of reading part internals.

When unloading solid vehicle fuel into inventory items, still use `stored_ammo.h` for the final
stored-charge to item-charge conversion. This preserves partial fuel that cannot be represented as a
complete item.

```cpp
const auto stored_charges = veh.fuel_left( fuel );
auto fuel_item = spawn_stored_ammo( fuel, stored_charges );
if( fuel_item ) {
    veh.drain( fuel, stored_ammo_charges_for_items( fuel, fuel_item->charges ) );
}
```

## `NO_UNLOAD` and recovery policy

`NO_UNLOAD` on an item means the player should not be able to unload it through normal unload UI.
It does not automatically answer what should happen when the containing item is destroyed by
crafting, disassembly, or salvage. In those paths, make the policy explicit and add a test:

- If the source item survives, preserve unitemizable remainders.
- If the source item is consumed, recover complete ammo items and discard any remainder that cannot
  exist as an inventory item.

## Testing checklist

When changing item, ammo, or fuel code, include tests for the storage layout and edge cases:

- A tool or gun with integral ammo.
- A magazine with contained ammo, such as a rechargeable battery cell.
- A source item that is consumed as a crafting/disassembly component.
- A source item that remains in the world after unloading.
- Zero ammo and partial ammo below one physical item, especially plutonium fuel.
- Multiple complete physical items plus a partial remainder.

Prefer checking both sides of the operation: the recovered item stack and the remaining source ammo.
