import { assert, assertEquals } from "@std/assert"
import { copy, ensureDir, exists, walk } from "@std/fs"
import { dirname, fromFileUrl, join, relative, resolve } from "@std/path"
import sharp from "npm:sharp@^0.33.5"

const SCRIPT_DIR = dirname(fromFileUrl(import.meta.url))
const REPO_ROOT = resolve(SCRIPT_DIR, "..")

const TS_TILESET_SCRIPT = join(REPO_ROOT, "scripts/tileset.ts")
const PY_COMPOSE_SCRIPT = join(REPO_ROOT, "tools/gfx_tools/compose.py")
const PY_DECOMPOSE_SCRIPT = join(REPO_ROOT, "tools/gfx_tools/decompose.py")

const DEFAULT_PYTHON = join(REPO_ROOT, ".venv/bin/python")

const TEXT_DECODER = new TextDecoder()

interface CommandOptions {
  cwd: string
}

interface Rgba {
  r: number
  g: number
  b: number
  alpha: number
}

const normalizePath = (path: string): string => path.replaceAll("\\", "/")

const sha256Hex = async (bytes: Uint8Array): Promise<string> => {
  const digestInput = new Uint8Array(bytes)
  const digest = await crypto.subtle.digest("SHA-256", digestInput)
  return Array.from(new Uint8Array(digest))
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("")
}

const runCommand = async (
  executable: string,
  args: string[],
  options: CommandOptions,
): Promise<void> => {
  const output = await new Deno.Command(executable, {
    args,
    cwd: options.cwd,
    stdout: "piped",
    stderr: "piped",
  }).output()

  if (output.code !== 0) {
    const stdout = TEXT_DECODER.decode(output.stdout)
    const stderr = TEXT_DECODER.decode(output.stderr)
    throw new Error(
      [
        `Command failed (${output.code}): ${executable} ${args.join(" ")}`,
        `cwd: ${options.cwd}`,
        `stdout:\n${stdout}`,
        `stderr:\n${stderr}`,
      ].join("\n"),
    )
  }
}

const runTypescriptCompose = async (sourceDir: string, outputDir: string): Promise<void> => {
  await runCommand(
    "deno",
    [
      "run",
      "--allow-read",
      "--allow-write",
      "--allow-env",
      "--allow-ffi",
      TS_TILESET_SCRIPT,
      "--pack",
      sourceDir,
      outputDir,
    ],
    { cwd: REPO_ROOT },
  )
}

const runTypescriptDecompose = async (tilesetDir: string): Promise<void> => {
  await runCommand(
    "deno",
    [
      "run",
      "--allow-read",
      "--allow-write",
      "--allow-env",
      "--allow-ffi",
      TS_TILESET_SCRIPT,
      "--unpack",
      tilesetDir,
    ],
    { cwd: REPO_ROOT },
  )
}

const runPythonCompose = async (
  pythonExecutable: string,
  sourceDir: string,
  outputDir: string,
): Promise<void> => {
  await runCommand(
    pythonExecutable,
    [PY_COMPOSE_SCRIPT, sourceDir, outputDir, "--feedback", "SILENT"],
    { cwd: REPO_ROOT },
  )
}

const runPythonDecompose = async (
  pythonExecutable: string,
  tilesetDir: string,
): Promise<void> => {
  await runCommand(pythonExecutable, [PY_DECOMPOSE_SCRIPT, tilesetDir], {
    cwd: REPO_ROOT,
  })
}

const getPythonExecutable = async (): Promise<string | null> => {
  const pythonExecutable = Deno.env.get("TILESET_PYTHON") ?? DEFAULT_PYTHON

  if (!await exists(pythonExecutable)) {
    console.warn(
      `Skipping tileset parity tests: ${pythonExecutable} not found. ` +
        "Create a uv venv and install pyvips (uv venv .venv && uv pip install --python .venv/bin/python pyvips pyvips-binary).",
    )
    return null
  }

  const output = await new Deno.Command(pythonExecutable, {
    args: ["-c", "import pyvips"],
    stdout: "null",
    stderr: "null",
  }).output()

  if (output.code !== 0) {
    console.warn(
      `Skipping tileset parity tests: ${pythonExecutable} cannot import pyvips. ` +
        "Install pyvips into your uv venv.",
    )
    return null
  }

  return pythonExecutable
}

const writeSolidPng = async (
  pathname: string,
  width: number,
  height: number,
  color: Rgba,
): Promise<void> => {
  await sharp({
    create: {
      width,
      height,
      channels: 4,
      background: color,
    },
  }).png().toFile(pathname)
}

const createFixtureTileset = async (sourceDir: string): Promise<void> => {
  await ensureDir(sourceDir)
  await Deno.writeTextFile(join(sourceDir, "tileset.txt"), "JSON: tile_config.json\n")

  const tileInfo = [
    {
      width: 2,
      height: 2,
    },
    {
      "main.png": {
        sprites_across: 2,
      },
    },
    {
      "filler.png": {
        filler: true,
        sprites_across: 2,
      },
    },
    {
      "fallback.png": {
        fallback: true,
      },
    },
  ]

  await Deno.writeTextFile(
    join(sourceDir, "tile_info.json"),
    JSON.stringify(tileInfo, null, 2),
  )

  const mainDir = join(sourceDir, "pngs_main_2x2", "images0")
  await ensureDir(mainDir)

  await writeSolidPng(join(mainDir, "alpha.png"), 2, 2, {
    r: 255,
    g: 0,
    b: 0,
    alpha: 255,
  })
  await writeSolidPng(join(mainDir, "beta.png"), 2, 2, {
    r: 0,
    g: 255,
    b: 0,
    alpha: 255,
  })
  await writeSolidPng(join(mainDir, "gamma.png"), 2, 2, {
    r: 0,
    g: 0,
    b: 255,
    alpha: 255,
  })

  const mainEntries = [
    {
      id: "t_alpha",
      fg: "alpha",
    },
    {
      id: ["t_beta", "t_beta_alt"],
      fg: ["beta", { sprite: ["gamma", "beta"], weight: 2 }],
    },
    {
      id: "f_floor",
      fg: "beta",
      bg: "alpha",
      additional_tiles: [
        {
          id: "open",
          fg: "gamma",
        },
      ],
    },
  ]

  await Deno.writeTextFile(
    join(mainDir, "entries.json"),
    JSON.stringify(mainEntries, null, 2),
  )

  const fillerDir = join(sourceDir, "pngs_filler_2x2", "images0")
  await ensureDir(fillerDir)

  await writeSolidPng(join(fillerDir, "overlay_custom.png"), 2, 2, {
    r: 255,
    g: 255,
    b: 0,
    alpha: 255,
  })

  const fillerEntries = {
    id: "overlay_custom",
    fg: "overlay_custom",
  }

  await Deno.writeTextFile(
    join(fillerDir, "overlay.json"),
    JSON.stringify(fillerEntries, null, 2),
  )
}

const readJson = async (pathname: string): Promise<unknown> => {
  const text = await Deno.readTextFile(pathname)
  return JSON.parse(text)
}

const compareJsonFiles = async (leftPath: string, rightPath: string): Promise<void> => {
  const [left, right] = await Promise.all([readJson(leftPath), readJson(rightPath)])
  assertEquals(left, right)
}

const comparePngPixelPerfect = async (leftPath: string, rightPath: string): Promise<void> => {
  const [left, right] = await Promise.all([
    sharp(leftPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true }),
    sharp(rightPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true }),
  ])

  assertEquals(left.info.width, right.info.width)
  assertEquals(left.info.height, right.info.height)
  assertEquals(left.info.channels, right.info.channels)

  const [leftHash, rightHash] = await Promise.all([
    sha256Hex(left.data),
    sha256Hex(right.data),
  ])
  assertEquals(leftHash, rightHash)
}

const collectReferencedSpriteIndexes = (
  layer: unknown,
  target: Set<number>,
): void => {
  if (Array.isArray(layer)) {
    for (const part of layer) {
      collectReferencedSpriteIndexes(part, target)
    }
    return
  }

  if (typeof layer === "number") {
    if (layer > 0) {
      target.add(layer)
    }
    return
  }

  if (
    typeof layer === "object" && layer !== null &&
    "sprite" in layer
  ) {
    const randomSprite = (layer as { sprite: unknown }).sprite
    collectReferencedSpriteIndexes(randomSprite, target)
  }
}

const compareReferencedSpritesOnly = async (
  leftPath: string,
  rightPath: string,
  indexes: number[],
  spriteWidth: number,
  spriteHeight: number,
  rangeStart: number,
): Promise<void> => {
  const [leftMeta, rightMeta] = await Promise.all([
    sharp(leftPath).metadata(),
    sharp(rightPath).metadata(),
  ])

  assertEquals(leftMeta.width, rightMeta.width)
  assertEquals(leftMeta.height, rightMeta.height)

  const width = leftMeta.width ?? 0
  const spritesAcross = Math.floor(width / spriteWidth)
  assert(spritesAcross > 0, "Invalid sprites_across value")

  for (const index of indexes) {
    const localIndex = rangeStart === 1 ? index - rangeStart + 1 : index - rangeStart

    if (localIndex < 0) {
      continue
    }

    const x = (localIndex % spritesAcross) * spriteWidth
    const y = Math.floor(localIndex / spritesAcross) * spriteHeight

    const [leftTile, rightTile] = await Promise.all([
      sharp(leftPath)
        .extract({ left: x, top: y, width: spriteWidth, height: spriteHeight })
        .ensureAlpha()
        .raw()
        .toBuffer(),
      sharp(rightPath)
        .extract({ left: x, top: y, width: spriteWidth, height: spriteHeight })
        .ensureAlpha()
        .raw()
        .toBuffer(),
    ])

    const [leftHash, rightHash] = await Promise.all([
      sha256Hex(leftTile),
      sha256Hex(rightTile),
    ])
    assertEquals(leftHash, rightHash)
  }
}

const collectDecomposedFiles = async (rootDir: string): Promise<string[]> => {
  const files: string[] = []

  for await (
    const entry of walk(rootDir, {
      includeFiles: true,
      includeDirs: false,
      followSymlinks: false,
    })
  ) {
    const relPath = normalizePath(relative(rootDir, entry.path))
    if (relPath === "tile_info.json" || relPath.startsWith("pngs_")) {
      files.push(relPath)
    }
  }

  files.sort()
  return files
}

const compareDecomposedOutputs = async (leftDir: string, rightDir: string): Promise<void> => {
  const [leftFiles, rightFiles] = await Promise.all([
    collectDecomposedFiles(leftDir),
    collectDecomposedFiles(rightDir),
  ])

  assertEquals(leftFiles, rightFiles)

  for (const relativePath of leftFiles) {
    const leftPath = join(leftDir, relativePath)
    const rightPath = join(rightDir, relativePath)

    if (relativePath.endsWith(".json")) {
      await compareJsonFiles(leftPath, rightPath)
      continue
    }

    if (relativePath.endsWith(".png")) {
      await comparePngPixelPerfect(leftPath, rightPath)
      continue
    }

    const [leftBytes, rightBytes] = await Promise.all([
      Deno.readFile(leftPath),
      Deno.readFile(rightPath),
    ])

    assertEquals(leftBytes, rightBytes)
  }
}

Deno.test({
  name: "tileset: compose output matches compose.py pixel-perfect",
  sanitizeOps: false,
  sanitizeResources: false,
  async fn() {
    const pythonExecutable = await getPythonExecutable()
    if (!pythonExecutable) {
      return
    }

    const tempRoot = await Deno.makeTempDir({ prefix: "tileset_compose_parity_" })

    try {
      const sourceDir = join(tempRoot, "fixture_source")
      const pyOutDir = join(tempRoot, "py_packed")
      const tsOutDir = join(tempRoot, "ts_packed")

      await createFixtureTileset(sourceDir)

      await runPythonCompose(pythonExecutable, sourceDir, pyOutDir)
      await runTypescriptCompose(sourceDir, tsOutDir)

      await compareJsonFiles(join(pyOutDir, "tile_config.json"), join(tsOutDir, "tile_config.json"))

      const tileConfig = await readJson(join(pyOutDir, "tile_config.json")) as {
        tile_info: Array<{ width?: number; height?: number }>
        "tiles-new": Array<{
          file?: string
          "//"?: string
          sprite_width?: number
          sprite_height?: number
          tiles?: Array<Record<string, unknown>>
        }>
      }

      const defaultInfo = tileConfig.tile_info[0] ?? {}
      const defaultWidth = defaultInfo.width ?? 16
      const defaultHeight = defaultInfo.height ?? 16

      for (const sheet of tileConfig["tiles-new"]) {
        if (!sheet.file || !sheet.file.endsWith(".png")) {
          continue
        }

        const pySheetPath = join(pyOutDir, sheet.file)
        const tsSheetPath = join(tsOutDir, sheet.file)
        const [pyExists, tsExists] = await Promise.all([exists(pySheetPath), exists(tsSheetPath)])
        assertEquals(pyExists, tsExists, `Sheet existence mismatch for ${sheet.file}`)

        if (pyExists && tsExists && Array.isArray(sheet.tiles)) {
          const referencedIndexes = new Set<number>()

          for (const tile of sheet.tiles) {
            collectReferencedSpriteIndexes(tile.fg, referencedIndexes)
            collectReferencedSpriteIndexes(tile.bg, referencedIndexes)

            const additionalTiles = tile.additional_tiles
            if (Array.isArray(additionalTiles)) {
              for (const additionalTile of additionalTiles) {
                const entry = additionalTile as Record<string, unknown>
                collectReferencedSpriteIndexes(entry.fg, referencedIndexes)
                collectReferencedSpriteIndexes(entry.bg, referencedIndexes)
              }
            }
          }

          const spriteWidth = sheet.sprite_width ?? defaultWidth
          const spriteHeight = sheet.sprite_height ?? defaultHeight
          const rangeComment = sheet["//"] ?? ""
          const rangeMatch = rangeComment.match(/range\s+(\d+)\s+to\s+(\d+)/)
          const rangeStart = rangeMatch ? Number.parseInt(rangeMatch[1], 10) : 1

          await compareReferencedSpritesOnly(
            pySheetPath,
            tsSheetPath,
            [...referencedIndexes].sort((left, right) => left - right),
            spriteWidth,
            spriteHeight,
            rangeStart,
          )
        }
      }
    } finally {
      await Deno.remove(tempRoot, { recursive: true })
    }
  },
})

Deno.test({
  name: "tileset: decompose output matches decompose.py pixel-perfect",
  sanitizeOps: false,
  sanitizeResources: false,
  async fn() {
    const pythonExecutable = await getPythonExecutable()
    if (!pythonExecutable) {
      return
    }

    const tempRoot = await Deno.makeTempDir({ prefix: "tileset_decompose_parity_" })

    try {
      const sourceDir = join(tempRoot, "fixture_source")
      const pyPackedDir = join(tempRoot, "py_packed")
      const pyUnpackDir = join(tempRoot, "py_unpack")
      const tsUnpackDir = join(tempRoot, "ts_unpack")

      await createFixtureTileset(sourceDir)
      await runPythonCompose(pythonExecutable, sourceDir, pyPackedDir)

      await copy(pyPackedDir, pyUnpackDir)
      await copy(pyPackedDir, tsUnpackDir)

      await Promise.all([
        writeSolidPng(join(pyUnpackDir, "fallback.png"), 2, 2, {
          r: 0,
          g: 0,
          b: 0,
          alpha: 255,
        }),
        writeSolidPng(join(tsUnpackDir, "fallback.png"), 2, 2, {
          r: 0,
          g: 0,
          b: 0,
          alpha: 255,
        }),
      ])

      await runPythonDecompose(pythonExecutable, pyUnpackDir)
      await runTypescriptDecompose(tsUnpackDir)

      await compareDecomposedOutputs(pyUnpackDir, tsUnpackDir)

      const [pyTileInfoExists, tsTileInfoExists] = await Promise.all([
        exists(join(pyUnpackDir, "tile_info.json")),
        exists(join(tsUnpackDir, "tile_info.json")),
      ])

      assert(pyTileInfoExists, "Python decompose did not produce tile_info.json")
      assert(tsTileInfoExists, "TypeScript decompose did not produce tile_info.json")
    } finally {
      await Deno.remove(tempRoot, { recursive: true })
    }
  },
})
