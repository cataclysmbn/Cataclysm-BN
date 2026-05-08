# Lua Style Guide

As Lua ecosystem is a new addition to the project, we only have formatting guidelines.

## Formatting

We format Lua files with [dprint-plugin-stylua](https://github.com/RubixDev/dprint-plugin-stylua)
with default configuration.

To format lua files, run:

```sh
deno task dprint fmt
```

### Formatting Lua files in VSCode

1. Install [dprint vscode extension](https://marketplace.visualstudio.com/items?itemName=dprint.dprint).
2. Add following lines on `.vscode/settings.json`:

```json
{
  "[lua]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "dprint.dprint"
  }
}
```

now saving your lua files will also format them.
