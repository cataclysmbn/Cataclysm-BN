# UDP タイルセットのビルド

UndeadPeople 形式のアンパック済みタイルセットは、TypeScript 版タイルセットツールで合成します。

リンク:

- アンパック済みタイルセットのリポジトリ: <https://github.com/Theawesomeboophis/UndeadPeopleUnpacked>
- プロジェクト Discord: <https://discord.gg/ftgMS5Rcsd>
- Deno インストーラ: <https://deno.com/>

古い `compose.py` セットアップでは Python、Libvips、pyvips を使用していました。以下のリンクは履歴参照用です: <https://www.python.org/downloads/>、<https://github.com/libvips/build-win64-mxe/releases>、<https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/>。

## セットアップ

1. Deno をインストールします。
2. アンパック済みタイルセットを用意します。
3. Cataclysm: Bright Nights の作業コピーから、アンパック済みタイルセットのディレクトリを入力として pack コマンドを実行します:

```sh
deno run -A scripts/tileset.ts --pack path/to/UndeadPeopleUnpacked path/to/UndeadPeoplePacked
```

最初のパスはアンパック済みタイルセットのルートです。`tileset.txt`、`tile_info.json`、`pngs_*` ディレクトリなどが入っている必要があります。2つ目のパスには、合成された `tile_config.json` とタイルシート PNG が書き込まれます。

詳細は [タイルセット](/mod/json/reference/graphics/tileset/#typescript-tileset-tool) を参照してください。ブラウザで小規模な確認を行う場合は [Tileset Web Tool](/dev/reference/tileset_web_tool/) を参照してください。
