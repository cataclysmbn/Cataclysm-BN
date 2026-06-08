# Tileset Web Tool (Experimental)

Use this page to compose/decompose a **small tileset** directly in your browser.

For normal tileset work, use `scripts/tileset.ts` instead:

```sh
deno run -A scripts/tileset.ts --pack gfx/Retrodays
deno run -A scripts/tileset.ts --unpack gfx/ChestHole16Tileset
```

See [Tilesets](/mod/json/reference/graphics/tileset/#typescript-tileset-tool) for the full workflow.
The TypeScript tool was added in [PR #8151](https://github.com/cataclysmbn/Cataclysm-BN/pull/8151).

## Input directory

`Tileset root folder` means the folder in your working copy that contains `tileset.txt`, for
example `gfx/Retrodays/` or `gfx/ChestHole16Tileset/`.

For compose, select a folder that contains:

- `tileset.txt`
- `tile_info.json`
- one or more `pngs_*` directories with sprite PNGs and `tile_entry` JSON files

For decompose, select a folder that contains:

- `tileset.txt`
- `tile_config.json`
- the tilesheet PNG files referenced by `tile_config.json`

The browser does not upload files. It reads the selected local folder and downloads a ZIP with the
result.

## Limits

- Intended for quick validation and tutorial usage.
- Supports one regular tilesheet per tileset.
- Preserves pixel data for sprite extraction/composition.

<div id="tileset-web-tool">
  <p>
    <label>
      Tileset root folder:
      <input id="tileset-input" type="file" webkitdirectory directory multiple />
    </label>
  </p>
  <p>
    <label><input type="radio" name="mode" value="compose" checked /> Compose</label>
    <label><input type="radio" name="mode" value="decompose" /> Decompose</label>
  </p>
  <p>
    <button id="tileset-run" type="button">Run</button>
    <button id="tileset-download" type="button" disabled>Download ZIP</button>
  </p>
  <pre id="tileset-log" style="white-space: pre-wrap; max-height: 22rem; overflow: auto;"></pre>
</div>

<script type="module" src="/tools/tileset_web_tool.js"></script>
