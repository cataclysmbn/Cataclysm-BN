# Addiction types

Addiction types define the addictions characters can get from comestibles (`"addiction_type"` on
the item), professions (`"addictions"`) and Lua (`Character:add_addiction`). The core types are in
`data/json/addiction_types.json`.

```json
{
  "type": "addiction_type",
  "id": "nicotine",
  "name": "nicotine",
  "withdrawal_name": "Nicotine Withdrawal",
  "description": "Intelligence - 1;   Occasional cravings",
  "craving_morale": "morale_craving_nicotine",
  "legacy_enum": "CIG"
}
```

## Fields

| Identifier      | Description                                                                                                                                                                                                                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| id              | (_mandatory_) Unique string id. Items, professions and Lua refer to the type by this id.                                                                                                                                                                                                               |
| name            | (_mandatory_) Name that finishes the sentence "Became addicted to ____." in the memorial log.                                                                                                                                                                                                          |
| description     | (_mandatory_) Effects shown in the character status UI.                                                                                                                                                                                                                                                |
| withdrawal_name | (_optional_) Name of the withdrawal effect in the character status UI, e.g. "Nicotine Withdrawal". Defaults to `name`.                                                                                                                                                                                 |
| craving_morale  | (_optional_) Morale type of the cravings, e.g. `morale_craving_nicotine`.                                                                                                                                                                                                                              |
| legacy_enum     | (_optional_) The hardcoded addiction this type replaces (`CAFFEINE`, `ALCOHOL`, `SLEEP`, `PKILLER`, `SPEED`, `CIG`, `COKE`, `CRACK`, `MUTAGEN`, `DIAZEPAM`, `MARLOSS_R`, `MARLOSS_B`, `MARLOSS_Y`). Used to read old saves and to select the built-in withdrawal behavior. Only the core types use it. |
| on_withdrawal   | (_optional_) `"lua:NAME"`: runs `game.addiction_functions["NAME"]` every turn while in withdrawal, instead of the built-in behavior. Must start with `lua:`.                                                                                                                                           |

A type with neither `on_withdrawal` nor `legacy_enum` has no withdrawal effects. See the Lua cookbook
("Custom Addiction Types") for the callback parameters.

Saved characters keep addictions by id. When a type no longer exists (for example after its mod was
removed), the addiction is dropped when the character loads.
