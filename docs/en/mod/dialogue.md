---
title: Dialogue System (Yarn)
tableOfContents:
  maxHeadingLevel: 4
---

NPC dialogue is authored in [Yarn Spinner](https://www.yarnspinner.dev/) `.yarn` files and
loaded at game start. The older JSON-based `TALK_TOPIC` system remains functional but is
considered legacy — see [NPCs (Legacy)](./json/reference/creatures/npcs.md) for that reference.

## File Location and NPC Wiring

Dialogue files live under `data/dialogue/`. An NPC picks up a Yarn story by having its
`yarn_story` field set to a story name (the filename without the `.yarn` extension). Example:

```json
{
  "type": "npc",
  "id": "my_npc",
  "chatbin": { "yarn_story": "my_npc_story" }
}
```

Each `.yarn` file becomes one _story_. The story name equals the filename without the `.yarn`
extension. Multiple NPCs can share the same story.

## Node Format

A `.yarn` file contains one or more **nodes** separated by `===`. Each node starts with
frontmatter headers and then a body.

```yarn
title: MyNode
position: 0,0
---
NPC: Hello there.
-> Hi. #spoken
-> Goodbye.
    <<stop>>
===
```

### Frontmatter Keys

| Key               | Description                                                                                                 |
| ----------------- | ----------------------------------------------------------------------------------------------------------- |
| `title`           | Node name. Used by `<<jump>>`, `<<goto>>`, and `shared_choices`.                                            |
| `position`        | `x,y` coordinates for the VS Code graph view only; no runtime effect.                                       |
| `shared_choices`  | Space-separated list of nodes whose choices are appended to this node.                                      |
| `inject_into`     | `story::Node` — this node's choices are inserted into the named node.                                       |
| `inject_priority` | Integer sort key for injection order. Negative values inject before native choices; default is `0` (after). |

## Entry Node Convention

When a conversation starts, the runtime looks for a `Greeting` node first. If found, it runs
it once (for first-contact NPC speech) and then falls through to `<<jump Start>>`. If no
`Greeting` node exists, the runtime enters at `Start`.

Return-to-menu jumps always target `Start` so the greeting is not repeated.

```yarn
title: Greeting
---
NPC: Good to see you.  I've been here since the first night.
<<jump Start>>
===

title: Start
shared_choices: CommonActions
---
-> Ask about supplies.
    <<jump Start_Supplies>>
===
```

## Speaker Attribution

Lines in a node body are attributed by a `Name:` prefix. Four forms are supported:

| Form                          | What Renders                                                      |
| ----------------------------- | ----------------------------------------------------------------- |
| `NPC: text`                   | NPC speaks; resolved to the NPC's actual in-game name at runtime. |
| `You: text` or `Player: text` | Player speaks.                                                    |
| `SomeName: text`              | Literal speaker name as written.                                  |
| `text` (no prefix)            | Unattributed narration — displayed as-is with no name or quotes.  |

For multi-sentence NPC speeches, attribute only the first line and leave continuations plain.
This avoids the NPC's name appearing on every line:

```yarn
NPC: I was at work when the sirens went off.
I drove here thinking it'd be temporary.
That was... I don't even know how long ago.
```

For explicit programmatic attribution (inside command or conditional blocks) use
`<<npc "text">>` and `<<player "text">>`.

## Choice Labels and `#spoken`

A choice line begins with `->`. The label is the button text shown to the player. By default,
selecting a choice does **not** echo the label as player speech.

Add `#spoken` to the end of the label to opt in to echoing:

```yarn
-> I hear you. #spoken
```

If the spoken wording differs from the button text, write it explicitly in the choice body
using `You:`:

```yarn
-> That sounds rough.
    You: I'm sorry.  That sounds really rough.
    <<jump Start>>
```

## Conditions

Use `<<if condition>>` / `<<else>>` / `<<endif>>` to make sections conditional.
Conditions can also be placed on individual choice lines:

```yarn
-> I'm stronger than I look. <<if u_get_str() >= 10>>
    You: Trust me.
```

Conditions use function-call syntax. All built-in functions are listed in the
[Condition Reference](#condition-reference) below.

Boolean operators `and`, `or`, `not` and comparison operators `==`, `!=`, `<`, `>`, `<=`, `>=`
are supported. Parentheses may be used for grouping.

## Text Interpolation

Embed expressions in dialogue text using `{expression}`:

```yarn
NPC: Your strength is {u_get_str()}.
NPC: Hello, {u_name()}.
```

All talk tags are also expanded in dialogue text and choice labels. These are unchanged
from the old JSON system:

| Tag                             | Expands to                                                                     |
| ------------------------------- | ------------------------------------------------------------------------------ |
| `<yrwp>`                        | Player's primary weapon name.                                                  |
| `<mywp>`                        | NPC's primary weapon name, or "fists".                                         |
| `<ammo>`                        | Player's weapon ammo name.                                                     |
| `<mypronoun>`                   | NPC's pronoun ("He" / "She").                                                  |
| `<current_activity>`            | NPC's current activity as a verb.                                              |
| `<topic_item>`                  | Item name from a repeat response (see Repeat Choices).                         |
| `<topic_item_price>`            | Unit price of the topic item.                                                  |
| `<topic_item_my_total_price>`   | NPC's total stock price for the topic item.                                    |
| `<topic_item_your_total_price>` | Player's total inventory price for the topic item.                             |
| `<interval>`                    | Time until the NPC restocks their shop.                                        |
| `<utalk_var_X>`                 | Value of player variable stored under the talk key `npctalk_var_X`.            |
| `<npctalk_var_X>`               | Value of NPC variable stored under the talk key `npctalk_var_X`.               |
| Snippet tags                    | Any tag registered in `talk_tags.json` (e.g. `<neutralchitchat>`, `<name_g>`). |

The Yarn-native alternative to `<utalk_var_X>` is `{u_get_var("X")}`, which is cleaner and
does not require the legacy key naming convention. That implementation was a hack to improve the legacy system until
a better system replaced it.

## Navigation

| Command             | Behaviour                                                                                            |
| ------------------- | ---------------------------------------------------------------------------------------------------- |
| `<<jump NodeName>>` | Replace the current node with `NodeName`. Conversation ends when `NodeName` ends.                    |
| `<<goto NodeName>>` | Push `NodeName` onto the navigation stack. When `NodeName` ends, the caller re-presents its choices. |
| `<<stop>>`          | End the conversation immediately.                                                                    |

For cross-story navigation, prefix the node name with the story: `<<jump story_name::NodeName>>`.
Bare names resolve in the current story.

## Shared Choices

`shared_choices` appends another node's choices to the current node's choice list. The source
node's choice bodies navigate within the story that owns the source node.

```yarn
title: Start
shared_choices: ShelterActions common::CommonActions
---
-> My own choice.
    <<jump Start>>
===
```

## Mod Injection

A mod can add choices to any existing node without modifying its file:

```yarn
title: ModExtra
inject_into: shelter_npc::Start
inject_priority: 0
---
-> Ask something new.
    You: What about that door?
    <<jump ModDetail>>
===
```

- `inject_priority < 0` — injected choices appear before the node's own choices.
- `inject_priority >= 0` — injected choices appear after (default).
- The runtime automatically appends `<<jump TargetNode>>` to injected choice bodies; the
  player is returned to the injection target. An explicit `<<stop>>` or `<<jump>>` in the
  body overrides this.
- `<<goto story::NodeName>>` in an injected body pushes the named node and returns to the
  injection target when it finishes.

## Social Checks (Trials)

Use `trial_roll(type, difficulty)` as a condition to gate content behind a skill check.
Supported types: `"PERSUADE"`, `"LIE"`, `"INTIMIDATE"`.

```yarn
-> I'm sure I can talk my way through this. <<if trial_roll("PERSUADE", 50)>>
    NPC: ...Fine.  You win.
    <<jump Start>>
```

## Random Lines

You can use line groups like so:

```yarn
=> NPC: Haven't seen anyone else until you showed up.
I thought I heard voices a few days ago — west side of town — but I didn't dare go look.
=> NPC: You're the only person I've seen since this started.  Well, besides you showing up.
=> NPC: Nobody seems to be around anymore. Not that I want to look around to much...
```

This will pick one of the given lines at random.
Mind the indentation, since these pick up every line >= the line group entry's indentation level.
This is native to Yarn Spinner, and allows randomized dialogue flow in a very compact and clean
format.
In addition to simply picking an option, this method also allows you to set conditions on them
by appending <<if X>> at the end of the line.
It's recommended to ensure that there's at least one option that is always valid,
but technically this is not required.

If you'd rather have a more direct and programmatic method, we also support the random_line command.
`random_line("text1", "text2", ...)` selects one string at random:

```yarn
NPC: {random_line("Stay sharp.", "Keep your eyes open.", "Watch yourself out there.")}
```

## Variables

Two variable systems are available.

### Simple variables

Store and retrieve arbitrary string or numeric values:

```yarn
<<u_set_var "met_npc" "yes">>
<<if u_get_var("met_npc") == "yes">>
```

| Command / Function                                   | Description                  |
| ---------------------------------------------------- | ---------------------------- |
| `<<u_set_var "key" value>>`                          | Store value under key.       |
| `<<u_remove_var "key">>`                             | Delete key.                  |
| `<<u_adjust_var "key" amount>>`                      | Add amount to stored number. |
| `u_get_var("key")`                                   | Retrieve as string.          |
| `u_get_var_num("key")`                               | Retrieve as number.          |
| `<<npc_set_var "key" value>>` / `npc_get_var("key")` | Same, on the NPC.            |

### Legacy-compatible talk variables

For interoperability with the legacy JSON dialogue system, use the four-argument
`u_add_var` / `u_has_var` family which follows the `npctalk_var_type_context_name` key format:

```yarn
<<u_add_var "asked_about_vault" "sentinel" "old_guard" "yes">>
<<if u_has_var("asked_about_vault", "sentinel", "old_guard", "yes")>>
```

It's recommended that you not use this because it may be depreciated. If you need an equivalent,
Lua is always an option. Though with the better flow control of Yarn Spinner, it's unlikely
that a 3 key system will be desirable.

---

## Condition Reference

All condition functions are usable inside `<<if>>` blocks and on `->` choice lines.

### Player state (`u_` prefix)

| Function                                        | Returns | Description                                     |
| ----------------------------------------------- | ------- | ----------------------------------------------- |
| `u_has_trait("id")`                             | bool    | Player has trait.                               |
| `u_has_any_trait("id1", "id2", ...)`            | bool    | Player has any of the listed traits.            |
| `u_has_trait_flag("flag")`                      | bool    | Player has any trait with the given flag.       |
| `u_has_effect("id")`                            | bool    | Player has effect.                              |
| `u_has_bionic("id")`                            | bool    | Player has installed bionic.                    |
| `u_has_item("id")`                              | bool    | Player has item in inventory.                   |
| `u_has_items("id", count)`                      | bool    | Player has at least `count` of item.            |
| `u_has_item_category("cat")`                    | bool    | Player has any item in category.                |
| `u_is_wearing("id")`                            | bool    | Player is wearing item.                         |
| `u_has_weapon()`                                | bool    | Player is wielding a weapon.                    |
| `u_can_stow_weapon()`                           | bool    | Player can put away their current weapon.       |
| `u_has_skill("id", level)`                      | bool    | Player has at least `level` in skill.           |
| `u_has_strength(n)`                             | bool    | Player STR ≥ n.                                 |
| `u_has_dexterity(n)`                            | bool    | Player DEX ≥ n.                                 |
| `u_has_intelligence(n)`                         | bool    | Player INT ≥ n.                                 |
| `u_has_perception(n)`                           | bool    | Player PER ≥ n.                                 |
| `u_get_str()`                                   | number  | Player's current strength.                      |
| `u_get_dex()`                                   | number  | Player's current dexterity.                     |
| `u_get_int()`                                   | number  | Player's current intelligence.                  |
| `u_get_per()`                                   | number  | Player's current perception.                    |
| `u_get_hunger()`                                | number  | Player's effective hunger level.                |
| `u_get_thirst()`                                | number  | Player's thirst level.                          |
| `u_get_fatigue()`                               | number  | Player's fatigue level.                         |
| `u_need("fatigue"\|"hunger"\|"thirst", amount)` | bool    | Player's need exceeds `amount`.                 |
| `u_get_ecash()`                                 | number  | Player's pre-cataclysm bank balance.            |
| `u_get_owed()`                                  | number  | Amount the NPC owes the player.                 |
| `u_male()`                                      | bool    | Player is male.                                 |
| `u_female()`                                    | bool    | Player is female.                               |
| `u_has_stolen_item()`                           | bool    | Player has an item that belongs to the NPC.     |
| `u_has_mission("id")`                           | bool    | Player has an active mission of the given type. |
| `u_know_recipe("id")`                           | bool    | Player has the recipe memorized.                |
| `u_at_om_location("id")`                        | bool    | Player is standing on overmap tile type.        |
| `u_driving()`                                   | bool    | Player is operating a moving vehicle.           |
| `u_has_var("name","type","context","value")`    | bool    | Legacy talk variable check.                     |
| `u_compare_var("name","type","context","op",n)` | bool    | Compare legacy talk variable.                   |
| `u_get_var("key")`                              | string  | Simple variable lookup.                         |
| `u_get_var_num("key")`                          | number  | Simple variable lookup as number.               |
| `u_name()`                                      | string  | Player's name.                                  |

### NPC state (`npc_` prefix)

| Function                                          | Returns | Description                                                    |
| ------------------------------------------------- | ------- | -------------------------------------------------------------- |
| `npc_has_trait("id")`                             | bool    | NPC has trait.                                                 |
| `npc_has_any_trait("id1", ...)`                   | bool    | NPC has any of the listed traits.                              |
| `npc_has_trait_flag("flag")`                      | bool    | NPC has any trait with the given flag.                         |
| `npc_has_effect("id")`                            | bool    | NPC has effect.                                                |
| `npc_has_bionic("id")`                            | bool    | NPC has installed bionic.                                      |
| `npc_has_item("id")`                              | bool    | NPC has item.                                                  |
| `npc_has_items("id", count)`                      | bool    | NPC has at least `count` of item.                              |
| `npc_has_item_category("cat")`                    | bool    | NPC has any item in category.                                  |
| `npc_is_wearing("id")`                            | bool    | NPC is wearing item.                                           |
| `npc_has_weapon()`                                | bool    | NPC is wielding a weapon.                                      |
| `npc_can_stow_weapon()`                           | bool    | NPC can put away their current weapon.                         |
| `npc_has_skill("id", level)`                      | bool    | NPC has at least `level` in skill.                             |
| `npc_get_skill("id")`                             | number  | NPC's level in a skill.                                        |
| `npc_has_strength(n)`                             | bool    | NPC STR ≥ n.                                                   |
| `npc_has_dexterity(n)`                            | bool    | NPC DEX ≥ n.                                                   |
| `npc_has_intelligence(n)`                         | bool    | NPC INT ≥ n.                                                   |
| `npc_has_perception(n)`                           | bool    | NPC PER ≥ n.                                                   |
| `npc_get_str()`                                   | number  | NPC's current strength.                                        |
| `npc_get_dex()`                                   | number  | NPC's current dexterity.                                       |
| `npc_get_int()`                                   | number  | NPC's current intelligence.                                    |
| `npc_get_per()`                                   | number  | NPC's current perception.                                      |
| `npc_get_hunger()`                                | number  | NPC's effective hunger.                                        |
| `npc_get_thirst()`                                | number  | NPC's thirst.                                                  |
| `npc_get_fatigue()`                               | number  | NPC's fatigue.                                                 |
| `npc_need("fatigue"\|"hunger"\|"thirst", amount)` | bool    | NPC need exceeds `amount`.                                     |
| `npc_trust()`                                     | number  | NPC's trust of the player.                                     |
| `npc_male()`                                      | bool    | NPC is male.                                                   |
| `npc_female()`                                    | bool    | NPC is female.                                                 |
| `npc_following()` or `npc_is_following()`         | bool    | NPC is following the player.                                   |
| `npc_is_ally()`                                   | bool    | NPC has an ally attitude (follow/lead/heal).                   |
| `npc_is_enemy()`                                  | bool    | NPC has a hostile attitude.                                    |
| `npc_friend()`                                    | bool    | NPC is a player ally.                                          |
| `npc_hostile()`                                   | bool    | NPC is hostile.                                                |
| `npc_available()` or `npc_service()`              | bool    | NPC does not have the `currently_busy` effect.                 |
| `npc_is_riding()`                                 | bool    | NPC is mounted.                                                |
| `npc_has_activity()`                              | bool    | NPC has an ongoing activity.                                   |
| `npc_has_class("id")`                             | bool    | NPC belongs to a class.                                        |
| `npc_has_rule("rule")`                            | bool    | NPC follower AI rule is active.                                |
| `npc_has_override("rule")`                        | bool    | NPC follower rule has an override set.                         |
| `npc_aim_rule("rule")`                            | bool    | NPC aim rule matches the given string.                         |
| `npc_engagement_rule("rule")`                     | bool    | NPC engagement rule matches.                                   |
| `npc_cbm_reserve_rule("rule")`                    | bool    | NPC CBM reserve rule matches.                                  |
| `npc_cbm_recharge_rule("rule")`                   | bool    | NPC CBM recharge rule matches.                                 |
| `npc_train_skills()`                              | bool    | NPC can teach the player a skill.                              |
| `npc_train_styles()`                              | bool    | NPC can teach the player a martial art.                        |
| `npc_has_var("name","type","context","value")`    | bool    | Legacy talk variable check.                                    |
| `npc_compare_var("name","type","context","op",n)` | bool    | Compare legacy variable.                                       |
| `npc_get_var("key")`                              | string  | Simple variable lookup.                                        |
| `npc_get_var_num("key")`                          | number  | Simple variable as number.                                     |
| `npc_name()`                                      | string  | NPC's name.                                                    |
| `has_pickup_list()`                               | bool    | NPC has a pickup whitelist configured.                         |
| `npc_role_nearby("role")`                         | bool    | NPC with the given companion mission role is within 100 tiles. |
| `npc_allies(n)`                                   | bool    | Player has at least `n` NPC allies.                            |

### Mission state

| Function                        | Returns | Description                                     |
| ------------------------------- | ------- | ----------------------------------------------- |
| `has_no_assigned_mission()`     | bool    | NPC has no missions assigned to the player.     |
| `has_assigned_mission()`        | bool    | Player has exactly one mission from this NPC.   |
| `has_many_assigned_missions()`  | bool    | Player has more than one mission from this NPC. |
| `has_no_available_mission()`    | bool    | NPC has no jobs to offer.                       |
| `has_available_mission()`       | bool    | NPC has exactly one job to offer.               |
| `has_many_available_missions()` | bool    | NPC has more than one job to offer.             |
| `mission_complete()`            | bool    | The selected mission is completed.              |
| `mission_incomplete()`          | bool    | The selected mission is not yet completed.      |
| `mission_has_generic_rewards()` | bool    | The selected mission has generic rewards.       |
| `mission_goal("GOAL")`          | bool    | The selected mission has the given goal type.   |

### Environment

| Function                   | Returns | Description                                                        |
| -------------------------- | ------- | ------------------------------------------------------------------ |
| `get_season()`             | string  | Current season: `"spring"`, `"summer"`, `"autumn"`, or `"winter"`. |
| `is_season("season")`      | bool    | Current season matches the argument.                               |
| `days_since_cataclysm()`   | number  | Integer days elapsed since the Cataclysm.                          |
| `is_day()`                 | bool    | It is currently daytime.                                           |
| `is_outside()`             | bool    | NPC is outdoors (no roof).                                         |
| `at_safe_space()`          | bool    | NPC is on a safe-space overmap tile.                               |
| `u_at_om_location("id")`   | bool    | Player is on overmap tile type.                                    |
| `npc_at_om_location("id")` | bool    | NPC is on overmap tile type.                                       |

### Social

| Function                                                  | Returns | Description                                                          |
| --------------------------------------------------------- | ------- | -------------------------------------------------------------------- |
| `trial_roll("PERSUADE"\|"LIE"\|"INTIMIDATE", difficulty)` | bool    | Dice-based social check.                                             |
| `random_line("a", "b", ...)`                              | string  | Randomly selects one string argument.                                |
| `has_reason()`                                            | bool    | Always false — the legacy "use_reason" mechanism is not implemented. |
| `is_by_radio()`                                           | bool    | Always false — radio dialogue detection is not wired up.             |

---

## Command Reference

Commands are written `<<command_name args>>`. String arguments should be quoted.

### Items

| Command                           | Description                                               |
| --------------------------------- | --------------------------------------------------------- |
| `<<give_item "id" [count]>>`      | Give the player `count` (default 1) of an item.           |
| `<<take_item "id" [count]>>`      | Remove `count` (default 1) of an item from the player.    |
| `<<u_buy_item "id" cost count>>`  | Deduct `cost` from the player's e-cash and give the item. |
| `<<u_sell_item "id" cost count>>` | Add `cost` to the player's e-cash and take the item.      |
| `<<u_consume_item "id" count>>`   | Remove `count` of item from the player (unconditional).   |
| `<<npc_consume_item "id" count>>` | Remove `count` of item from the NPC.                      |
| `<<u_remove_item_with "id">>`     | Remove all instances of item from the player.             |
| `<<npc_remove_item_with "id">>`   | Remove all instances of item from the NPC.                |
| `<<give_equipment>>`              | Player selects items from the NPC's inventory to take.    |
| `<<npc_gets_item>>`               | Player selects an item from inventory to give to the NPC. |
| `<<npc_gets_item_to_use>>`        | Player selects an item for the NPC to equip/use.          |

### Character Effects and Traits

| Command                                  | Description                                      |
| ---------------------------------------- | ------------------------------------------------ |
| `<<add_effect "id" [duration_turns]>>`   | Add effect to the player.                        |
| `<<remove_effect "id">>`                 | Remove effect from the player.                   |
| `<<u_add_effect "id" duration_turns>>`   | Add effect to the player (negative = permanent). |
| `<<u_lose_effect "id">>`                 | Remove effect from the player.                   |
| `<<npc_add_effect "id" duration_turns>>` | Add effect to the NPC.                           |
| `<<npc_lose_effect "id">>`               | Remove effect from the NPC.                      |
| `<<u_add_trait "id">>`                   | Grant trait to the player.                       |
| `<<u_lose_trait "id">>`                  | Remove trait from the player.                    |
| `<<npc_add_trait "id">>`                 | Grant trait to the NPC.                          |
| `<<npc_lose_trait "id">>`                | Remove trait from the NPC.                       |

### NPC Opinion

| Command                          | Description                                                               |
| -------------------------------- | ------------------------------------------------------------------------- |
| `<<npc_add_trust amount>>`       | Adjust NPC trust of the player by `amount`.                               |
| `<<npc_add_fear amount>>`        | Adjust NPC fear of the player.                                            |
| `<<npc_add_value amount>>`       | Adjust NPC's value opinion of the player.                                 |
| `<<npc_add_anger amount>>`       | Adjust NPC anger toward the player.                                       |
| `<<add_debt "TYPE" factor ...>>` | Adjust NPC owed debt. Type/factor pairs; `"TOTAL"` is a final multiplier. |

### Economy

| Command                    | Description                                          |
| -------------------------- | ---------------------------------------------------- |
| `<<u_spend_ecash amount>>` | Subtract from player's e-cash (negative values add). |
| `<<u_faction_rep amount>>` | Adjust the NPC's faction opinion of the player.      |

### NPC Follower Behaviour

| Command                                | Description                                           |
| -------------------------------------- | ----------------------------------------------------- |
| `<<npc_follow>>`                       | NPC starts following the player.                      |
| `<<npc_stop_follow>>`                  | NPC stops following.                                  |
| `<<npc_set_attitude "ATTITUDE">>`      | Set NPC attitude by string ID (e.g. `"NPCATT_KILL"`). |
| `<<follow>>`                           | NPC follows and joins player's faction.               |
| `<<follow_only>>`                      | NPC follows without changing faction.                 |
| `<<stop_following>>`                   | NPC stops following.                                  |
| `<<leave>>`                            | NPC leaves player's faction.                          |
| `<<toggle_npc_rule "rule">>`           | Toggle a boolean follower AI rule.                    |
| `<<set_npc_rule "rule">>`              | Enable a boolean follower AI rule.                    |
| `<<clear_npc_rule "rule">>`            | Disable a boolean follower AI rule.                   |
| `<<set_npc_engagement_rule "rule">>`   | Set NPC engagement distance rule.                     |
| `<<set_npc_aim_rule "rule">>`          | Set NPC aiming rule.                                  |
| `<<set_npc_cbm_reserve_rule "rule">>`  | Set NPC CBM energy reserve rule.                      |
| `<<set_npc_cbm_recharge_rule "rule">>` | Set NPC CBM recharge rule.                            |
| `<<copy_npc_rules>>`                   | Copy player follower rules to the NPC.                |
| `<<clear_overrides>>`                  | Clear all NPC rule overrides.                         |
| `<<set_npc_pickup>>`                   | Open the NPC pickup whitelist editor.                 |
| `<<assign_guard>>`                     | Make the NPC a guard.                                 |
| `<<stop_guard>>`                       | Release NPC from guard duty.                          |

### NPC Activities

| Command                      | Description                       |
| ---------------------------- | --------------------------------- |
| `<<sort_loot>>`              | NPC sorts loot.                   |
| `<<do_construction>>`        | NPC does construction.            |
| `<<do_mining>>`              | NPC mines.                        |
| `<<do_read>>`                | NPC reads.                        |
| `<<do_chop_plank>>`          | NPC chops planks.                 |
| `<<do_vehicle_deconstruct>>` | NPC deconstructs vehicle.         |
| `<<do_vehicle_repair>>`      | NPC repairs vehicle.              |
| `<<do_chop_trees>>`          | NPC chops trees.                  |
| `<<do_fishing>>`             | NPC fishes.                       |
| `<<do_farming>>`             | NPC farms.                        |
| `<<do_butcher>>`             | NPC butchers.                     |
| `<<revert_activity>>`        | NPC reverts to previous activity. |
| `<<goto_location>>`          | NPC goes to a location.           |
| `<<find_mount>>`             | NPC finds a mount.                |
| `<<dismount>>`               | NPC dismounts.                    |

### Services and Training

| Command                                             | Description                                                         |
| --------------------------------------------------- | ------------------------------------------------------------------- |
| `<<start_trade>>`                                   | Open the trade screen.                                              |
| `<<buy_10_logs>>`                                   | NPC places 10 logs in the ranch garage (1 day).                     |
| `<<buy_100_logs>>`                                  | NPC places 100 logs in the ranch garage (7 days).                   |
| `<<buy_horse>>` / `<<buy_cow>>` / `<<buy_chicken>>` | NPC sells the player the given animal.                              |
| `<<give_aid>>`                                      | NPC heals the player. Assigns NPC activity; conversation ends.      |
| `<<give_all_aid>>`                                  | NPC heals the player and all allies. Conversation ends.             |
| `<<barber_hair>>`                                   | Open hair style menu.                                               |
| `<<barber_beard>>`                                  | Open beard style menu.                                              |
| `<<buy_haircut>>`                                   | NPC gives the player a haircut morale boost. Conversation ends.     |
| `<<buy_shave>>`                                     | NPC gives the player a shave morale boost. Conversation ends.       |
| `<<bionic_install>>`                                | NPC installs a bionic from the player's inventory.                  |
| `<<bionic_remove>>`                                 | NPC removes a bionic from the player.                               |
| `<<start_training>>`                                | NPC trains the player in a skill or martial art. Conversation ends. |
| `<<morale_chat>>`                                   | Player gets a pleasant conversation morale boost.                   |
| `<<morale_chat_activity>>`                          | Same, assigns NPC activity. Conversation ends.                      |
| `<<reveal_stats>>`                                  | NPC reveals their stats.                                            |

### Missions

| Command                                | Description                                                |
| -------------------------------------- | ---------------------------------------------------------- |
| `<<npc_assign_selected_mission>>`      | Assign the NPC's currently selected mission to the player. |
| `<<add_mission "id">>`                 | Create and assign a mission to the player from NPC.        |
| `<<assign_mission "id">>`              | Create and assign a player-facing mission (no NPC owner).  |
| `<<finish_mission "id" success_bool>>` | Complete a mission as success or failure.                  |
| `<<mission_success>>`                  | Resolve the selected mission successfully.                 |
| `<<mission_failure>>`                  | Resolve the selected mission as a failure.                 |
| `<<clear_mission>>`                    | Clear the selected mission.                                |
| `<<mission_reward>>`                   | Give the player the mission reward.                        |
| `<<lead_to_safety>>`                   | NPC leads the player to safety.                            |

### Map Updates

| Command                             | Description                                         |
| ----------------------------------- | --------------------------------------------------- |
| `<<mapgen_update "id1" "id2" ...>>` | Apply mapgen updates at the NPC's overmap location. |

### NPC State

| Command                                                                                                  | Description                                             |
| -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| `<<npc_set_first_topic "topic">>`                                                                        | Set the NPC's first dialogue topic (legacy-compatible). |
| `<<npc_change_faction "id">>`                                                                            | Change the NPC's faction membership.                    |
| `<<hostile>>`                                                                                            | Make the NPC hostile and end conversation.              |
| `<<flee>>`                                                                                               | NPC flees from the player.                              |
| `<<end_conversation>>`                                                                                   | End conversation; NPC ignores player.                   |
| `<<insult_combat>>`                                                                                      | NPC becomes hostile and starts a fight.                 |
| `<<start_mugging>>`                                                                                      | NPC mugs the player.                                    |
| `<<player_leaving>>`                                                                                     | Signal the player is leaving.                           |
| `<<stranger_neutral>>`                                                                                   | Change NPC attitude to neutral.                         |
| `<<npc_thankful>>`                                                                                       | NPC becomes positively inclined.                        |
| `<<wake_up>>`                                                                                            | Wake a sleeping NPC.                                    |
| `<<npc_die>>`                                                                                            | NPC dies at end of conversation.                        |
| `<<control_npc>>`                                                                                        | Player takes control of the NPC. Conversation ends.     |
| `<<drop_weapon>>`                                                                                        | NPC drops their weapon.                                 |
| `<<drop_stolen_item>>`                                                                                   | NPC drops a stolen item.                                |
| `<<remove_stolen_status>>`                                                                               | Clear stolen item status.                               |
| `<<deny_follow>>` / `<<deny_lead>>` / `<<deny_equipment>>` / `<<deny_train>>` / `<<deny_personal_info>>` | Set denial effects on NPC.                              |
| `<<player_weapon_away>>`                                                                                 | Player holsters their weapon.                           |
| `<<player_weapon_drop>>`                                                                                 | Player drops their weapon.                              |

### Variables

See the [Variables](#variables) section above for usage. Commands:
`<<u_set_var>>`, `<<u_remove_var>>`, `<<u_adjust_var>>`,
`<<npc_set_var>>`, `<<npc_remove_var>>`, `<<npc_adjust_var>>`,
`<<u_add_var>>`, `<<u_lose_var>>`, `<<u_adjust_var_legacy>>`,
`<<npc_add_var>>`, `<<npc_lose_var>>`, `<<npc_adjust_var_legacy>>`.

---

## VS Code Integration

Install the [Yarn Spinner extension](https://marketplace.visualstudio.com/items?itemName=SecretLab.yarn-spinner)
to get the node graph view, syntax highlighting, and hover documentation for game-specific
functions and commands.

The file `data/dialogue/dialogue.ysls.json` registers the game's custom functions and commands
with the extension. Keep it updated when adding new conditions or commands.

---

## Legacy JSON Path

NPCs without a `yarn_story` field continue to use the old JSON `TALK_TOPIC` system. Both paths
drive the same `dialogue_window` for display. The legacy system remains fully functional during
the migration period; there is no deprecation deadline.

For the old JSON system, see [NPCs (Legacy)](./json/reference/creatures/npcs.md).
