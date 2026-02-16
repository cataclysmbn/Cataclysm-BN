# Lua 스타일 가이드

Lua 생태계는 프로젝트에 새로 추가되었으므로, 현재는 포매팅 가이드라인만 있습니다.

## 포매팅

기본 설정의 [dprint-plugin-stylua](https://github.com/RubixDev/dprint-plugin-stylua)로 Lua 파일을 포맷합니다.

Lua 파일을 포맷하려면 다음을 실행하세요:

```sh
deno task dprint fmt
```

### VSCode에서 Lua 파일 포매팅

1. [dprint vscode 확장](https://marketplace.visualstudio.com/items?itemName=dprint.dprint)을 설치하세요.
2. `.vscode/settings.json`에 다음 줄을 추가하세요:

```json
{
  "[lua]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "dprint.dprint"
  }
}
```

이제 Lua 파일을 저장하면 자동으로 포맷됩니다.
