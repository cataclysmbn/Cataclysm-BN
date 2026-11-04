---
title: Loot zones
---

`LOOT_ZONE` definitions describe zone types. Their translated `name` is used in
the type picker, selected-zone details, and the suggested name for a new zone.
Each zone instance has its own editable name. `name` accepts a translation object
with `str` and `ctxt`.

## Category storage

The creation checklist offers groups and individual storage categories. Selected
categories create one `LOOT_CUSTOM` zone. Area rules create separate native zones
on the same rectangle. Native functions can be checked alongside storage where appropriate. Construction,
tree cutting, mining, fishing and farming open separately. Each created zone asks for a name. The manual custom filter remains a separate
text-only option using the original item filter syntax.

Custom options save a `categories` array of stable zone IDs for category storage,
and a fallback `mark: "-"` that matches nothing in older game versions.
Manual text filters save only `mark`. The presence of `categories`, even when empty,
identifies category storage. Category storage cannot add a text filter;
editing it opens the category checklist directly.
Legacy custom zones without category fields keep their text-filter behavior.
Older versions cannot sort category storage and safely leave it unused. If an
older version saves the zones again, the category selection is lost and the zone
remains a manual filter that matches nothing; recreate its categories after upgrading.
Selecting only Corpses, Wood or Seeds creates the corresponding native zone type.
Mixed category storage remains one custom zone. Butchery, dissection, plank
cutting and seed selection also discover the corresponding categories in mixed
storage; this activity lookup does not change storage sorting priorities.

The ordered groups are loaded from `data/json/loot_zone_groups.json`.
Unknown storage types are retained as individual entries.

Sorting checks explicit text filters, available specialized zones, category
storage, then dump zones. Among category zones, the smaller category selection
wins; equal selections prefer the current storage location, then the closest
available tile. Full or blocked specialized storage allows a general fallback.
The original transfer mechanics are retained: sorting checks the route to the
source, without adding a route check to the destination. Items beyond an
inaccessible passage can therefore behave as they did in the original sorter. Cargo uses its own item
count and volume limits, independently of items on the ground beneath it.
Overlapping custom zones accept the union of their matching filters; an item at
a custom source zone is retained only if that zone accepts it.

The details panel shows selected category names directly under "Zone type:".
The Interface option **Zone category display limit** defaults to 8. Extra
categories are summarized as "and others"; 0 shows all. This display limit does
not change saved selections or sorting.
Other zones on the same rectangle are not included in its description.
Area rules remain independent zones and apply to their own tiles.
Manual custom zones show their type followed by a blank line and the raw
filter. Comma-separated conditions move intact to the next line whenever they
fit the panel width. Conditions wider than the entire panel must still wrap.
Details grow upwards, retaining the bottom shortcuts and at least one list row;
PgUp/PgDn scroll content that exceeds the available height.

## Configuring the creation checklist

`data/json/loot_zone_groups.json` defines `LOOT_ZONE_GROUP` objects:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "name": "Tools",
  "kind": "storage",
  "sort_order": 20,
  "categories": ["LOOT_WORKSHOP_TOOLS", "LOOT_ENTRY_TOOLS", "LOOT_TOOLS"]
}
```

`id` is stable and untranslated. `name` is an English source string (or a normal
translation object), extracted for translation alongside other JSON names.
Changing the game language changes group and type labels. Instance names already
saved by the player remain literal editable names.

`kind` is `storage` (combined category checkbox), `activity` (native functions, with individual setup when needed), or `rules` (separate native zones on the chosen rectangle). Storage groups
come first, followed by activity groups and then area rules. Within each section `sort_order`
controls the order, with `id` as a deterministic tie breaker. Members follow the
order of `categories`. Duplicate members appear only in their first group;
references to zone types not installed in the current world are skipped.

Mods can add groups or update an existing group by ID. Omitted fields retain the
existing values. `categories` replaces membership; `extend`/`delete` update it
without copying the original definition:

```json
{
  "type": "LOOT_ZONE_GROUP",
  "id": "tools",
  "extend": { "categories": ["MY_MOD_TOOL_STORAGE"] }
}
```

New storage zone types remain selectable even before they are grouped: types
with a `LOOT_` ID, types referenced by an item category's `zone`, and explicitly
configured storage members are discovered from the loaded world data. Ungrouped
storage appears after configured storage groups, sorted by stable zone ID.
Ungrouped native types appear under "Other zone types" and open individually.
The enabled manager never opens the legacy type picker.
A new item category still needs its usual zone mapping for category sorting;
this UI does not invent sorting behavior for categories without a destination.

Suggested names follow checklist order. One selected category uses its own name;
two names are joined with "and", and three or more names are comma-separated.
A storage group contributes its title only when more than three of its categories
are selected; smaller selections list the individual category names. The same
rule applies independently to each group in a mixed selection. Unknown/unassigned
storage types use their translated type name instead. Existing zone names remain unchanged.

The Zone Manager returns to its creation checklist if the manual filter dialog
is canceled; pending checkboxes are preserved and no name/area dialog opens.
Recognized filter prefixes (`c:`, `m:`, `q:`, etc.) are highlighted only for display.
The raw filter remains unchanged. Selected zones use the normal color overlay;
zone option text is never interpreted as a tile ID.

The Interface option **Use zone manager** is enabled by default. Disabling it
restores the standard zone-type picker for new zones without changing saved zones
or sorting behavior. In the checkbox manager, Left and Right both toggle the
highlighted checkbox; Enter on a category confirms the current selection. Service
entries and native types requiring setup retain their normal Enter action. There is no separate confirm row.
Unassigned categories use stable zone IDs for ordering, independent of language.

Selection markers are gray `−` (unselected), green `+` (selected), and yellow `#`
(partially selected group). Descriptions follow the highlighted entry, with current
key bindings shown underneath after a blank line.

Window sizing uses the same automatic `uilist` layout as the original zone picker:
width follows the contents and is capped to the screen; height is capped and the
list scrolls. Control hints use two lines below the selected entry description.
All checklist labels share one left alignment, with a single space after their
selection marker. Area-rule headings are not checkboxes; select each rule individually.
While text search is active, results contain only individual zone types; group
toggles and service entries are hidden. Clearing the search restores the full
checklist without changing the selected categories.

Editing an existing zone type also uses the manager when enabled. One existing zone
can become category storage or one native type; extra area rules must be created
as separate zones. Its name, bounds, enabled state, faction and vehicle binding
are retained, and cancellation leaves it unchanged.

The edit menu shows **Edit type** for category/native zones and **Edit filter**
for manual text filters. All zones retain **Edit type**, including legacy manual filters. Native setup dialogs, such as
farming and construction options, remain available separately.
