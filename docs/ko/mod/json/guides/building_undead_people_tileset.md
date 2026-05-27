# Undead People 타일셋 빌드

UndeadPeople 방식의 unpacked 타일셋은 TypeScript 타일셋 도구로 합성합니다.

링크:

- unpacked 타일셋 저장소: <https://github.com/Theawesomeboophis/UndeadPeopleUnpacked>
- 프로젝트 Discord: <https://discord.gg/ftgMS5Rcsd>
- Deno 설치: <https://deno.com/>

기존 `compose.py` 설정은 Python, Libvips, pyvips를 사용했습니다. 다음 링크는 이전 절차 확인용입니다: <https://www.python.org/downloads/>, <https://github.com/libvips/build-win64-mxe/releases>, <https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/>.

## 빌드

1. Deno를 설치합니다.
2. unpacked 타일셋 저장소를 준비합니다.
3. Cataclysm: Bright Nights 작업 사본에서 unpacked 타일셋 디렉터리를 입력으로 지정하여 pack 명령을 실행합니다:

```sh
deno run -A scripts/tileset.ts --pack path/to/UndeadPeopleUnpacked path/to/UndeadPeoplePacked
```

첫 번째 경로는 unpacked 타일셋 루트입니다. `tileset.txt`, `tile_info.json`, `pngs_*` 디렉터리 등이 들어 있어야 합니다. 두 번째 경로에는 합성된 `tile_config.json`과 타일시트 PNG가 저장됩니다.

자세한 형식은 [타일셋](/mod/json/reference/graphics/tileset/#typescript-tileset-tool)을 참고하세요. 브라우저에서 작은 예제를 확인하려면 [타일셋 웹 도구](/dev/reference/tileset_web_tool/)를 사용할 수 있습니다.

## 이전 자료

다음 링크는 오래된 자료라 현재 명령과 다를 수 있습니다.

- [DDA 타일셋 튜토리얼](https://github.com/CleverRaven/Cataclysm-DDA/wiki/Tileset-creation)
- [DDA 타일 설정 참조](https://github.com/CleverRaven/Cataclysm-DDA/blob/master/doc/TILESET.md)
- [이전 Undead People 저장소](https://github.com/SomeDeadGuy/UndeadPeopleTileset)
