# Tileset Web Tool (Experimental)

Use this page to compose/decompose a **small tileset** directly in your browser.

- It is intended for quick validation and tutorial usage.
- It currently supports one non-fallback tilesheet per tileset.
- It preserves pixel data for sprite extraction/composition.

<div id="tileset-web-tool">
  <p>
    <label>
      Tileset directory:
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
