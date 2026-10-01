# タイルセット Web ツール（実験的）

このページでは、ブラウザー上で **小さなタイルセット** を直接 compose / decompose できます。

通常のタイルセット作業には、代わりに `scripts/tileset.ts` を使用してください:

```sh
deno run -A scripts/tileset.ts --pack gfx/Retrodays
deno run -A scripts/tileset.ts --unpack gfx/ChestHole16Tileset
```

完全な手順については [タイルセット](/mod/json/reference/graphics/tileset/#typescript-tileset-tool) を参照してください。
TypeScript ツールは [PR #8151](https://github.com/cataclysmbn/Cataclysm-BN/pull/8151) で追加されました。

## 入力ディレクトリ

`タイルセットのルートフォルダー` とは、作業コピー内で `tileset.txt` を含むフォルダーです。例えば `gfx/Retrodays/` や `gfx/ChestHole16Tileset/` です。

compose では、次を含むフォルダーを選択します:

- `tileset.txt`
- `tile_info.json`
- スプライト PNG と `tile_entry` JSON ファイルを含む1つ以上の `pngs_*` ディレクトリ

decompose では、次を含むフォルダーを選択します:

- `tileset.txt`
- `tile_config.json`
- `tile_config.json` が参照するタイルシート PNG ファイル

ブラウザーはファイルをアップロードしません。選択したローカルフォルダーを読み込み、結果を ZIP としてダウンロードします。

## 制限

- クイックな検証とチュートリアル用途を想定しています。
- タイルセットごとに通常のタイルシートを1つサポートします。
- スプライトの抽出と合成ではピクセルデータを保持します。

<div id="tileset-web-tool">
  <p>
    <label>
      タイルセットのルートフォルダー:
      <input id="tileset-input" type="file" webkitdirectory directory multiple />
    </label>
  </p>
  <p>
    <label><input type="radio" name="mode" value="compose" checked /> Compose</label>
    <label><input type="radio" name="mode" value="decompose" /> Decompose</label>
  </p>
  <p>
    <button id="tileset-run" type="button">実行</button>
    <button id="tileset-download" type="button" disabled>ZIP をダウンロード</button>
  </p>
  <pre id="tileset-log" style="white-space: pre-wrap; max-height: 22rem; overflow: auto;"></pre>
</div>

<script type="module" src="/tools/tileset_web_tool.js"></script>
