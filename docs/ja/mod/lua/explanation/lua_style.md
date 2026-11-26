# Lua スタイルガイド

Lua のエコシステムはプロジェクトに新しく追加されたため、現在はフォーマットのガイドラインだけがあります。

## フォーマット

Lua ファイルは、デフォルト設定の [dprint-plugin-stylua](https://github.com/RubixDev/dprint-plugin-stylua) でフォーマットします。

Lua ファイルをフォーマットするには、次を実行します:

```sh
deno task dprint fmt
```

### VSCode で Lua ファイルをフォーマットする

1. [dprint vscode 拡張機能](https://marketplace.visualstudio.com/items?itemName=dprint.dprint) をインストールします。
2. `.vscode/settings.json` に次の行を追加します:

```json
{
  "[lua]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "dprint.dprint"
  }
}
```

これでファイルを保存すると、Lua ファイルも自動的にフォーマットされます。
