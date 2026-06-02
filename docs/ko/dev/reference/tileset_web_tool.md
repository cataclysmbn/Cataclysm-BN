# 타일셋 웹 도구 (실험적)

이 페이지를 사용하여 브라우저에서 직접 **작은 타일셋**을 합성/분해할 수 있습니다.

일반적인 타일셋 작업에는 `scripts/tileset.ts`를 사용하세요:

```sh
deno run -A scripts/tileset.ts --pack gfx/Retrodays
deno run -A scripts/tileset.ts --unpack gfx/ChestHole16Tileset
```

전체 작업 흐름은 [타일셋](/mod/json/reference/graphics/tileset/#typescript-tileset-tool)을 참고하세요.
TypeScript 도구는 [PR #8151](https://github.com/cataclysmbn/Cataclysm-BN/pull/8151)에서 추가되었습니다.

## 입력 디렉터리

`타일셋 루트 폴더`는 작업 사본에서 `tileset.txt`가 들어 있는 폴더를 뜻합니다. 예: `gfx/Retrodays/`,
`gfx/ChestHole16Tileset/`.

합성할 때는 다음 파일이 들어 있는 폴더를 선택하세요:

- `tileset.txt`
- `tile_info.json`
- 스프라이트 PNG와 `tile_entry` JSON 파일이 들어 있는 하나 이상의 `pngs_*` 디렉터리

분해할 때는 다음 파일이 들어 있는 폴더를 선택하세요:

- `tileset.txt`
- `tile_config.json`
- `tile_config.json`에서 참조하는 타일시트 PNG 파일

브라우저는 파일을 업로드하지 않습니다. 선택한 로컬 폴더를 읽고 결과 ZIP을 다운로드합니다.

## 제한

- 빠른 검증 및 튜토리얼 사용을 위한 것입니다.
- 일반 타일시트는 타일셋당 1개만 지원합니다.
- 스프라이트 추출/합성을 위해 픽셀 데이터를 보존합니다.

<div id="tileset-web-tool">
  <p>
    <label>
      타일셋 루트 폴더:
      <input id="tileset-input" type="file" webkitdirectory directory multiple />
    </label>
  </p>
  <p>
    <label><input type="radio" name="mode" value="compose" checked /> 합성</label>
    <label><input type="radio" name="mode" value="decompose" /> 분해</label>
  </p>
  <p>
    <button id="tileset-run" type="button">실행</button>
    <button id="tileset-download" type="button" disabled>ZIP 다운로드</button>
  </p>
  <pre id="tileset-log" style="white-space: pre-wrap; max-height: 22rem; overflow: auto;"></pre>
</div>

<script type="module" src="/tools/tileset_web_tool.js"></script>
