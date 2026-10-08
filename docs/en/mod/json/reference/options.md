# Options

## Option Group

Adds a new group to the options menu

| Key   | Description                                                                         |
| ----- | ----------------------------------------------------------------------------------- |
| type  | Always `OPTION_GROUP`                                                               |
| id    | id of the group                                                                     |
| page  | `general` `interface` `graphics` `performance` `world_default` `debug` or `android` |
| title | Visible Option Name                                                                 |
| desc  | Extended option description                                                         |

## Option Space

Adds a new space to the options menu

| Key  | Description                                                                         |
| ---- | ----------------------------------------------------------------------------------- |
| type | Always `OPTION_SPACE`                                                               |
| page | `general` `interface` `graphics` `performance` `world_default` `debug` or `android` |

## Option

Adds an option to the options menu

| Key             | Description                                                                                                                                                 |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| type            | Always `OPTION`                                                                                                                                             |
| id              | Unique option id                                                                                                                                            |
| stype           | Type of option                                                                                                                                              |
| page            | `general` `interface` `graphics` `performance` `world_default` `debug` or `android`                                                                         |
| group           | Optional group id it is under                                                                                                                               |
| menu_text       | Text displayed for option value                                                                                                                             |
| tooltip         | Extra info displayed for option                                                                                                                             |
| default         | Default value                                                                                                                                               |
| default_android | Default value on android                                                                                                                                    |
| min             | minimum value for integers / floats                                                                                                                         |
| max             | Maximum value for integers / floats                                                                                                                         |
| step            | Step for floats in menu                                                                                                                                     |
| hide            | Hides with `none`, `always`, `sdl_hide` ( with sdl ), `curses_hide` ( with curses ), `no_sound_hide` ( without sound ), `android_only` ( when not android ) |
| deps            | Array of `option` ( other option id ) and `values` ( valid option values ) to use the config                                                                |
