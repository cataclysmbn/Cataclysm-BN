# Building UDP tileset

UndeadPeople-style unpacked tilesets are composed with the TypeScript tileset tool.

Links:

- Unpacked tileset repository: <https://github.com/Theawesomeboophis/UndeadPeopleUnpacked>
- Project Discord: <https://discord.gg/ftgMS5Rcsd>
- Deno installer: <https://deno.com/>

The old `compose.py` setup used Python, Libvips, and pyvips. Those links are kept for historical
reference only: <https://www.python.org/downloads/>,
<https://github.com/libvips/build-win64-mxe/releases>,
<https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/>.

## Setup

1. Install Deno.
2. Get an unpacked tileset repository.
3. From a Cataclysm: Bright Nights working copy, run the pack command with the unpacked tileset
   directory as input:

```sh
deno run -A scripts/tileset.ts --pack path/to/UndeadPeopleUnpacked path/to/UndeadPeoplePacked
```

The first path is the unpacked tileset root. It should contain files such as `tileset.txt`,
`tile_info.json`, and `pngs_*` directories. The second path is where the composed `tile_config.json`
and tilesheet PNG files will be written.

For more detail, see [Tilesets](/mod/json/reference/graphics/tileset/#typescript-tileset-tool).
For small checks in a browser, see the [Tileset Web Tool](/dev/reference/tileset_web_tool/).
