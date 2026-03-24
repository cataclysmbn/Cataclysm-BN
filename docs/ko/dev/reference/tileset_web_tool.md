# 타일셋 웹 도구 (실험적)

이 페이지를 사용하여 브라우저에서 직접 **작은 타일셋**을 구성/분해할 수 있습니다.

- 빠른 검증 및 튜토리얼 사용을 위한 것입니다.
- 현재 타일셋당 하나의 비폴백 타일시트를 지원합니다.
- 스프라이트 추출/구성을 위해 픽셀 데이터를 보존합니다.

<div id="tileset-web-tool">
  <p>
    <label>
      타일셋 디렉토리:
      <input id="tileset-input" type="file" webkitdirectory directory multiple />
    </label>
  </p>
  <p>
    <label><input type="radio" name="mode" value="compose" checked /> 구성</label>
    <label><input type="radio" name="mode" value="decompose" /> 분해</label>
  </p>
  <p>
    <button id="tileset-run" type="button">실행</button>
    <button id="tileset-download" type="button" disabled>ZIP 다운로드</button>
  </p>
  <pre id="tileset-log" style="white-space: pre-wrap; max-height: 22rem; overflow: auto;"></pre>
</div>

<script type="module" src="/tools/tileset_web_tool.js"></script>
