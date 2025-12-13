#!/usr/bin/env -S deno run --allow-read --allow-write --allow-env --allow-ffi
/**
 * @module tileset
 *
 * Merge all tile entries and PNGs in a compositing tileset directory into
 * a tile_config.json and tilesheet .png file(s) ready for use in BN.
 *
 * Also supports unpacking a tileset into individual folders with images and JSON metadata.
 *
 * Examples:
 *   deno run -A scripts/tileset.ts --pack gfx/Retrodays/
 *   deno run -A scripts/tileset.ts --unpack gfx/MSX++UndeadPeopleEdition/
 */

import { Command } from "@cliffy/command"
import { walk } from "@std/fs"
import { join, resolve } from "@std/path"
import sharp from "npm:sharp@^0.33.5"

const PROPERTIES_FILENAME = "tileset.txt"
const IGNORE_FILE = ".scratch"

const PNG_SAVE_ARGS = {
  compression: 9 as const,
  effort: 10,
}

const FALLBACK_TEMPLATE = {
  file: "fallback.png",
  tiles: [],
  ascii: [
    { offset: 0, bold: false, color: "BLACK" },
    { offset: 256, bold: true, color: "WHITE" },
    { offset: 512, bold: false, color: "WHITE" },
    { offset: 768, bold: true, color: "BLACK" },
    { offset: 1024, bold: false, color: "RED" },
    { offset: 1280, bold: false, color: "GREEN" },
    { offset: 1536, bold: false, color: "BLUE" },
    { offset: 1792, bold: false, color: "CYAN" },
    { offset: 2048, bold: false, color: "MAGENTA" },
    { offset: 2304, bold: false, color: "YELLOW" },
    { offset: 2560, bold: true, color: "RED" },
    { offset: 2816, bold: true, color: "GREEN" },
    { offset: 3072, bold: true, color: "BLUE" },
    { offset: 3328, bold: true, color: "CYAN" },
    { offset: 3584, bold: true, color: "MAGENTA" },
    { offset: 3840, bold: true, color: "YELLOW" },
  ],
}

interface TileInfo {
  width?: number
  height?: number
  pixelscale?: number
  iso?: boolean
  retract_dist_min?: number
  retract_dist_max?: number
}

interface TileSheetConfig {
  sprite_width?: number
  sprite_height?: number
  sprite_offset_x?: number
  sprite_offset_y?: number
  sprite_offset_x_retracted?: number
  sprite_offset_y_retracted?: number
  pixelscale?: number
  sprites_across?: number
  exclude?: string[]
  fallback?: boolean
  filler?: boolean
}

interface TileEntry {
  id: string | string[]
  fg?: number | number[] | Record<string, unknown>[] | Record<string, unknown>
  bg?: number | number[] | Record<string, unknown>[] | Record<string, unknown>
  additional_tiles?: TileEntry[]
  [key: string]: unknown
}

interface TileSheetOutput {
  file: string
  "//": string
  sprite_width?: number
  sprite_height?: number
  sprite_offset_x?: number
  sprite_offset_y?: number
  sprite_offset_x_retracted?: number
  sprite_offset_y_retracted?: number
  pixelscale?: number
  tiles: unknown[]
}

class ComposingException extends Error {
  constructor(message: string) {
    super(message)
    this.name = "ComposingException"
  }
}

function listOrFirst<T>(arr: T[]): T | T[] {
  return arr.length === 1 ? arr[0] : arr
}

function readProperties(filepath: string): Record<string, string> {
  const content = Deno.readTextFileSync(filepath)
  const pairs: Record<string, string> = {}
  for (const line of content.split("\n")) {
    const trimmed = line.trim()
    if (trimmed && !trimmed.startsWith("#")) {
      const [key, value] = trimmed.split(":")
      if (key && value) {
        pairs[key.trim()] = value.trim()
      }
    }
  }
  return pairs
}

async function writeToJson(
  pathname: string,
  data: unknown,
  formatJson: boolean = false,
): Promise<void> {
  const jsonContent = formatJson ? JSON.stringify(data, null, 2) : JSON.stringify(data)
  await Deno.writeTextFile(pathname, jsonContent)
}

class Tileset {
  sourceDir: string
  outputDir: string
  useAll: boolean
  obsoleteFillers: boolean
  paletteCopies: boolean
  palette: boolean
  formatJson: boolean
  onlyJson: boolean
  outputConfFile: string | null = null

  pngnum = 0
  unreferencedPngnames = {
    main: [] as string[],
    filler: [] as string[],
  }
  pngnameToNum: Record<string, number> = { null_image: 0 }
  processedIds: string[] = []

  spriteWidth = 16
  spriteHeight = 16
  pixelscale = 1
  iso = false
  retractDistMin = -1.0
  retractDistMax = 1.0
  info: Array<Record<string, TileSheetConfig | TileInfo>> = [{}]

  constructor(
    sourceDir: string,
    outputDir: string,
    options: {
      useAll?: boolean
      obsoleteFillers?: boolean
      paletteCopies?: boolean
      palette?: boolean
      formatJson?: boolean
      onlyJson?: boolean
    } = {},
  ) {
    this.sourceDir = resolve(sourceDir)
    this.outputDir = outputDir ? resolve(outputDir) : this.sourceDir
    this.useAll = options.useAll ?? false
    this.obsoleteFillers = options.obsoleteFillers ?? false
    this.paletteCopies = options.paletteCopies ?? false
    this.palette = options.palette ?? false
    this.formatJson = options.formatJson ?? false
    this.onlyJson = options.onlyJson ?? false

    try {
      const stat = Deno.statSync(this.sourceDir)
      if (!stat.isDirectory) {
        throw new ComposingException(`Error: ${this.sourceDir} is not a directory`)
      }
    } catch {
      throw new ComposingException(`Error: cannot open directory ${this.sourceDir}`)
    }

    const infoPath = join(this.sourceDir, "tile_info.json")
    try {
      const infoContent = Deno.readTextFileSync(infoPath)
      this.info = JSON.parse(infoContent)
      const baseInfo = this.info[0] as TileInfo
      this.spriteWidth = baseInfo.width ?? this.spriteWidth
      this.spriteHeight = baseInfo.height ?? this.spriteHeight
      this.pixelscale = baseInfo.pixelscale ?? this.pixelscale
      this.retractDistMin = baseInfo.retract_dist_min ?? this.retractDistMin
      this.retractDistMax = baseInfo.retract_dist_max ?? this.retractDistMax
      this.iso = baseInfo.iso ?? this.iso
    } catch {
      throw new ComposingException(`Error: cannot open ${infoPath}`)
    }
  }

  determineConffile(): string {
    let properties: Record<string, string> = {}

    for (const candidatePath of [this.sourceDir, this.outputDir]) {
      const propertiesPath = join(candidatePath, PROPERTIES_FILENAME)
      try {
        properties = readProperties(propertiesPath)
        if (Object.keys(properties).length > 0) break
      } catch {
        continue
      }
    }

    if (Object.keys(properties).length === 0) {
      throw new ComposingException(`No valid ${PROPERTIES_FILENAME} found`)
    }

    const confFilename = properties.JSON
    if (!confFilename) {
      throw new ComposingException(`No JSON key found in ${PROPERTIES_FILENAME}`)
    }

    this.outputConfFile = confFilename
    return this.outputConfFile
  }

  async compose(): Promise<void> {
    await Deno.mkdir(this.outputDir, { recursive: true })
    const tilesetConfpath = join(this.outputDir, this.determineConffile())

    const typedSheets: {
      main: Tilesheet[]
      filler: Tilesheet[]
      fallback: Tilesheet[]
    } = {
      main: [],
      filler: [],
      fallback: [],
    }

    let fallbackName = "fallback.png"
    let addedFirstNull = false

    for (const config of this.info.slice(1)) {
      const sheet = new Tilesheet(this, config)

      if (!addedFirstNull) {
        sheet.sprites.push(await sheet.nullImage())
        addedFirstNull = true
      }

      const sheetType = sheet.isFallback ? "fallback" : sheet.isFiller ? "filler" : "main"

      console.log(`parsing ${sheetType} tilesheet ${sheet.name}`)

      if (sheetType !== "fallback") {
        await sheet.walkDirs()
        if (!await sheet.writeCompositePng()) {
          continue
        }
        sheet.maxIndex = this.pngnum
      }

      typedSheets[sheetType].push(sheet)
    }

    const sheetConfigs = [...typedSheets.main, ...typedSheets.filler, ...typedSheets.fallback]
    const tilesNewDict = new Map<number, TileSheetOutput>()

    const createTileEntriesForUnused = (
      unused: string[],
      fillers: boolean,
    ) => {
      for (const unusedPng of unused) {
        if (this.processedIds.includes(unusedPng)) {
          if (!fillers) {
            console.warn(
              `${unusedPng} sprite was not mentioned in any tile entry but there is a tile entry for the ${unusedPng} ID`,
            )
          }
          if (fillers && this.obsoleteFillers) {
            console.warn(`there is a tile entry for ${unusedPng} in a non-filler sheet`)
          }
          continue
        }

        const unusedNum = this.pngnameToNum[unusedPng]
        let sheetMinIndex = 0

        for (const [sheetMaxIndex, sheetData] of tilesNewDict) {
          if (sheetMinIndex < unusedNum && unusedNum <= sheetMaxIndex) {
            sheetData.tiles.push({
              id: unusedPng,
              fg: unusedNum,
            })
            this.processedIds.push(unusedPng)
            break
          }
          sheetMinIndex = sheetMaxIndex
        }
      }
    }

    let mainFinished = false

    for (const sheet of sheetConfigs) {
      if (sheet.isFallback) {
        fallbackName = sheet.name
        if (!sheet.isStandard()) {
          Object.assign(FALLBACK_TEMPLATE, {
            sprite_width: sheet.spriteWidth,
            sprite_height: sheet.spriteHeight,
            sprite_offset_x: sheet.offsetX,
            sprite_offset_y: sheet.offsetY,
          })
          if (
            sheet.offsetXRetracted !== sheet.offsetX ||
            sheet.offsetYRetracted !== sheet.offsetY
          ) {
            Object.assign(FALLBACK_TEMPLATE, {
              sprite_offset_x_retracted: sheet.offsetXRetracted,
              sprite_offset_y_retracted: sheet.offsetYRetracted,
            })
          }
          if (sheet.pixelscale !== 1.0) {
            Object.assign(FALLBACK_TEMPLATE, { pixelscale: sheet.pixelscale })
          }
        }
        continue
      }

      if (sheet.isFiller && !mainFinished) {
        createTileEntriesForUnused(this.handleUnreferencedSprites("main"), false)
        mainFinished = true
      }

      const sheetEntries = []
      for (const tileEntry of sheet.tileEntries) {
        const converted = tileEntry.convert()
        if (converted) sheetEntries.push(converted)
      }

      const sheetConf: TileSheetOutput = {
        file: sheet.name,
        "//": `range ${sheet.firstIndex} to ${sheet.maxIndex}`,
        tiles: sheetEntries,
      }

      if (!sheet.isStandard()) {
        sheetConf.sprite_width = sheet.spriteWidth
        sheetConf.sprite_height = sheet.spriteHeight
        sheetConf.sprite_offset_x = sheet.offsetX
        sheetConf.sprite_offset_y = sheet.offsetY
        if (
          sheet.offsetXRetracted !== sheet.offsetX ||
          sheet.offsetYRetracted !== sheet.offsetY
        ) {
          sheetConf.sprite_offset_x_retracted = sheet.offsetXRetracted
          sheetConf.sprite_offset_y_retracted = sheet.offsetYRetracted
        }
        if (sheet.pixelscale !== 1.0) {
          sheetConf.pixelscale = sheet.pixelscale
        }
      }

      tilesNewDict.set(sheet.maxIndex, sheetConf)
    }

    if (!mainFinished) {
      createTileEntriesForUnused(this.handleUnreferencedSprites("main"), false)
    }

    createTileEntriesForUnused(this.handleUnreferencedSprites("filler"), true)

    const tilesNew = Array.from(tilesNewDict.values())
    const fallback = { ...FALLBACK_TEMPLATE, file: fallbackName }
    tilesNew.push(fallback)

    const outputConf = {
      tile_info: [{
        pixelscale: this.pixelscale,
        width: this.spriteWidth,
        height: this.spriteHeight,
        iso: this.iso,
        retract_dist_min: this.retractDistMin,
        retract_dist_max: this.retractDistMax,
      }],
      "tiles-new": tilesNew,
    }

    await writeToJson(tilesetConfpath, outputConf, this.formatJson)
    console.log("done.")
  }

  handleUnreferencedSprites(sheetType: "main" | "filler"): string[] {
    if (this.useAll) {
      return this.unreferencedPngnames[sheetType]
    }

    for (const pngname of this.unreferencedPngnames[sheetType]) {
      if (this.processedIds.includes(pngname)) {
        console.error(
          `${pngname}.png not used when ${pngname} ID is mentioned in a tile entry`,
        )
      } else {
        console.warn(
          `sprite filename ${pngname} was not used in any ${sheetType} ${this.outputConfFile} entries`,
        )
      }
    }
    return []
  }
}

class Tilesheet {
  tileset: Tileset
  name: string
  spriteWidth: number
  spriteHeight: number
  offsetX: number
  offsetY: number
  offsetXRetracted: number
  offsetYRetracted: number
  pixelscale: number
  spritesAcross: number
  exclude: string[]
  isFallback: boolean
  isFiller: boolean
  subdirPath: string
  output: string
  tileEntries: TileEntryHandler[] = []
  sprites: sharp.Sharp[] = []
  firstIndex: number
  maxIndex: number

  constructor(tileset: Tileset, config: Record<string, TileSheetConfig | TileInfo>) {
    this.tileset = tileset
    this.name = Object.keys(config)[0]
    const specs = config[this.name] as TileSheetConfig

    this.spriteWidth = specs.sprite_width ?? tileset.spriteWidth
    this.spriteHeight = specs.sprite_height ?? tileset.spriteHeight
    this.offsetX = specs.sprite_offset_x ?? 0
    this.offsetY = specs.sprite_offset_y ?? 0
    this.offsetXRetracted = specs.sprite_offset_x_retracted ?? this.offsetX
    this.offsetYRetracted = specs.sprite_offset_y_retracted ?? this.offsetY
    this.pixelscale = specs.pixelscale ?? 1.0
    this.spritesAcross = specs.sprites_across ?? 16
    this.exclude = specs.exclude ?? []
    this.isFallback = specs.fallback ?? false
    this.isFiller = !this.isFallback && (specs.filler ?? false)

    const outputRoot = this.name.split(".png")[0]
    const dirName = `pngs_${outputRoot}_${this.spriteWidth}x${this.spriteHeight}`
    this.subdirPath = join(tileset.sourceDir, dirName)
    this.output = join(tileset.outputDir, this.name)

    this.firstIndex = this.tileset.pngnum + 1
    this.maxIndex = this.tileset.pngnum
  }

  async nullImage(): Promise<sharp.Sharp> {
    const buffer = Buffer.alloc(this.spriteWidth * this.spriteHeight * 4)
    return sharp(buffer, {
      raw: {
        width: this.spriteWidth,
        height: this.spriteHeight,
        channels: 4,
      },
    })
  }

  isStandard(): boolean {
    if (this.offsetX || this.offsetY) return false
    if (this.offsetXRetracted !== this.offsetX || this.offsetYRetracted !== this.offsetY) {
      return false
    }
    if (this.spriteWidth !== this.tileset.spriteWidth) return false
    if (this.spriteHeight !== this.tileset.spriteHeight) return false
    if (this.pixelscale !== 1.0) return false
    return true
  }

  async walkDirs(): Promise<void> {
    const excludedPaths = this.exclude.map((e) => join(this.subdirPath, e))

    const shouldSkip = async (path: string): Promise<boolean> => {
      for (const excluded of excludedPaths) {
        if (path.startsWith(excluded)) return true
      }
      try {
        await Deno.stat(join(path, IGNORE_FILE))
        return true
      } catch {
        return false
      }
    }

    const entries: Array<{ path: string; isFile: boolean; name: string }> = []

    for await (const entry of walk(this.subdirPath, { followSymlinks: true })) {
      if (await shouldSkip(entry.path)) continue
      entries.push(entry)
    }

    entries.sort((a, b) => a.path.localeCompare(b.path))

    for (const entry of entries) {
      if (!entry.isFile) continue

      if (entry.name.endsWith(".png")) {
        await this.processPng(entry.path)
      } else if (entry.name.endsWith(".json")) {
        await this.processJson(entry.path)
      }
    }
  }

  async processPng(filepath: string): Promise<void> {
    const stem = filepath.split("/").pop()!.replace(".png", "")

    if (stem in this.tileset.pngnameToNum) {
      if (!this.isFiller) {
        console.error(`duplicate root name ${stem}: ${filepath}`)
      }
      if (this.isFiller && this.tileset.obsoleteFillers) {
        console.warn(
          `root name ${stem} is already present in a non-filler sheet: ${filepath}`,
        )
      }
      return
    }

    if (!this.tileset.onlyJson) {
      this.sprites.push(await this.loadImage(filepath))
    } else {
      this.sprites.push(null as unknown as sharp.Sharp)
    }

    this.tileset.pngnum += 1
    this.tileset.pngnameToNum[stem] = this.tileset.pngnum
    this.tileset.unreferencedPngnames[this.isFiller ? "filler" : "main"].push(stem)
  }

  async loadImage(pngPath: string): Promise<sharp.Sharp> {
    try {
      const image = sharp(pngPath)
      const metadata = await image.metadata()

      if (metadata.width !== this.spriteWidth || metadata.height !== this.spriteHeight) {
        console.error(
          `${pngPath} is ${metadata.width}x${metadata.height}, but ${this.name} sheet sprites have to be ${this.spriteWidth}x${this.spriteHeight}.`,
        )
      }

      return image.ensureAlpha()
    } catch (error) {
      throw new ComposingException(`Cannot load ${pngPath}: ${error}`)
    }
  }

  async processJson(filepath: string): Promise<void> {
    try {
      const content = await Deno.readTextFile(filepath)
      let tileEntries = JSON.parse(content)

      if (!Array.isArray(tileEntries)) {
        tileEntries = [tileEntries]
      }

      for (const inputEntry of tileEntries) {
        this.tileEntries.push(new TileEntryHandler(this, inputEntry, filepath))
      }
    } catch (error) {
      console.error(`error loading ${filepath}`, error)
      throw error
    }
  }

  async writeCompositePng(): Promise<boolean> {
    if (this.sprites.length === 0) return false

    this.tileset.pngnum += this.spritesAcross -
      ((this.sprites.length % this.spritesAcross) || this.spritesAcross)

    if (this.tileset.onlyJson) return true

    const rows: sharp.Sharp[] = []
    for (let i = 0; i < this.sprites.length; i += this.spritesAcross) {
      const rowSprites = this.sprites.slice(i, i + this.spritesAcross)
      while (rowSprites.length < this.spritesAcross) {
        rowSprites.push(await this.nullImage())
      }

      const buffers = await Promise.all(
        rowSprites.map((s) => s.toFormat("png").toBuffer()),
      )

      const composites = buffers.map((buffer, idx) => ({
        input: buffer,
        left: idx * this.spriteWidth,
        top: 0,
      }))

      const row = sharp({
        create: {
          width: this.spriteWidth * this.spritesAcross,
          height: this.spriteHeight,
          channels: 4,
          background: { r: 0, g: 0, b: 0, alpha: 0 },
        },
      }).composite(composites)

      rows.push(row)
    }

    const rowBuffers = await Promise.all(
      rows.map((r) => r.toFormat("png").toBuffer()),
    )
    const sheetHeight = this.spriteHeight * rows.length
    const sheetWidth = this.spriteWidth * this.spritesAcross

    const composites = rowBuffers.map((buffer, idx) => ({
      input: buffer,
      left: 0,
      top: idx * this.spriteHeight,
    }))

    const sheet = sharp({
      create: {
        width: sheetWidth,
        height: sheetHeight,
        channels: 4,
        background: { r: 0, g: 0, b: 0, alpha: 0 },
      },
    }).composite(composites)

    await sheet.png({
      compressionLevel: PNG_SAVE_ARGS.compression,
      effort: PNG_SAVE_ARGS.effort,
    }).toFile(this.output)

    if (this.tileset.paletteCopies && !this.tileset.palette) {
      await sharp(this.output).png({
        palette: true,
        compressionLevel: PNG_SAVE_ARGS.compression,
        effort: PNG_SAVE_ARGS.effort,
      }).toFile(`${this.output}8`)
    }

    return true
  }
}

class TileEntryHandler {
  tilesheet: Tilesheet
  data: TileEntry
  filepath: string

  constructor(tilesheet: Tilesheet, data: TileEntry, filepath: string) {
    this.tilesheet = tilesheet
    this.data = data
    this.filepath = filepath
  }

  convert(entry: TileEntry = this.data, prefix = ""): TileEntry | null {
    const entryIds = entry.id
    const fgLayer = entry.fg
    const bgLayer = entry.bg

    if (!entryIds || (!fgLayer && !bgLayer)) {
      console.warn(
        `skipping empty entry in ${this.filepath}${entryIds ? ` with IDs ${prefix}${entryIds}` : ""}`,
      )
      return null
    }

    const idArray = Array.isArray(entryIds) ? entryIds : [entryIds]

    if (fgLayer) {
      entry.fg = listOrFirst(this.convertEntryLayer(fgLayer))
    } else {
      delete entry.fg
    }

    if (bgLayer) {
      entry.bg = listOrFirst(this.convertEntryLayer(bgLayer))
    } else {
      delete entry.bg
    }

    const additionalEntries = entry.additional_tiles ?? []
    for (const additionalEntry of additionalEntries) {
      this.convert(additionalEntry, `${idArray[0]}_`)
    }

    const validIds = []
    for (const entryId of idArray) {
      const fullId = `${prefix}${entryId}`

      if (!this.tilesheet.tileset.processedIds.includes(fullId)) {
        this.tilesheet.tileset.processedIds.push(fullId)
        validIds.push(entryId)
      } else {
        if (this.tilesheet.isFiller) {
          if (this.tilesheet.tileset.obsoleteFillers) {
            console.warn(`skipping filler for ${fullId} from ${this.filepath}`)
          }
        } else {
          console.error(`${fullId} encountered more than once, last time in ${this.filepath}`)
        }
      }
    }

    if (validIds.length > 0) {
      entry.id = listOrFirst(validIds)
      return entry
    }

    return null
  }

  convertEntryLayer(
    entryLayer: number | number[] | Record<string, unknown>[] | Record<string, unknown> | string |
      string[],
  ): (number | Record<string, unknown>)[] {
    const output: (number | Record<string, unknown>)[] = []

    if (Array.isArray(entryLayer)) {
      for (const layerPart of entryLayer) {
        if (typeof layerPart === "object" && layerPart !== null && "sprite" in layerPart) {
          const [variations, valid] = this.convertRandomVariations(
            layerPart.sprite as number | number[],
          )
          if (valid) {
            layerPart.sprite = variations.length === 1 ? variations[0] : variations
            output.push(layerPart)
          }
        } else {
          this.appendSpriteIndex(layerPart as string | number, output)
        }
      }
    } else {
      this.appendSpriteIndex(entryLayer as string | number, output)
    }

    return output
  }

  convertRandomVariations(
    spriteNames: number | number[] | string | string[],
  ): [number[], boolean] {
    let valid = false
    const convertedVariations: number[] = []

    if (Array.isArray(spriteNames)) {
      for (const spriteName of spriteNames) {
        valid = this.appendSpriteIndex(spriteName, convertedVariations) || valid
      }
    } else {
      valid = this.appendSpriteIndex(spriteNames, convertedVariations)
    }

    return [convertedVariations, valid]
  }

  appendSpriteIndex(
    spriteName: string | number,
    entry: (number | Record<string, unknown>)[],
  ): boolean {
    if (!spriteName) return false

    const spriteIndex = typeof spriteName === "number"
      ? spriteName
      : this.tilesheet.tileset.pngnameToNum[spriteName] ?? 0

    if (spriteIndex) {
      const sheetType = this.tilesheet.isFiller ? "filler" : "main"
      const idx = this.tilesheet.tileset.unreferencedPngnames[sheetType].indexOf(
        spriteName as string,
      )
      if (idx !== -1) {
        this.tilesheet.tileset.unreferencedPngnames[sheetType].splice(idx, 1)
      }

      entry.push(spriteIndex)
      return true
    }

    if (typeof spriteName === "string") {
      console.error(
        `${spriteName}.png file for ${spriteName} value from ${this.filepath} was not found. It will not be added to ${this.tilesheet.tileset.outputConfFile}`,
      )
    }

    return false
  }
}

async function pack(
  sourceDir: string,
  outputDir: string,
  options: {
    useAll?: boolean
    obsoleteFillers?: boolean
    paletteCopies?: boolean
    palette?: boolean
    formatJson?: boolean
    onlyJson?: boolean
  },
): Promise<void> {
  try {
    const tileset = new Tileset(sourceDir, outputDir, options)
    await tileset.compose()
  } catch (error) {
    if (error instanceof ComposingException) {
      console.error(error.message)
      Deno.exit(1)
    }
    throw error
  }
}

// Unpack functionality (decompose)
interface DecomposeTileEntry extends TileEntry {
  [key: string]: unknown
}

class TileSheetData {
  tsFilename: string
  tileIdToTileEntries: Record<string, DecomposeTileEntry[]> = {}
  spriteWidth: number
  spriteHeight: number
  spriteOffsetX: number
  spriteOffsetY: number
  writeDim: boolean
  tsPathname: string
  tsWidth: number
  tsTilesPerRow: number
  tsHeight: number
  tsRows: number
  pngnumMin: number
  pngnumMax: number
  expansions: DecomposeTileEntry[] = []
  fallback?: unknown

  constructor(tilesheetData: Record<string, unknown>, refs: PngRefs) {
    this.tsFilename = tilesheetData.file as string
    this.spriteHeight = (tilesheetData.sprite_height as number) ?? refs.defaultHeight
    this.spriteWidth = (tilesheetData.sprite_width as number) ?? refs.defaultWidth
    this.spriteOffsetX = (tilesheetData.sprite_offset_x as number) ?? 0
    this.spriteOffsetY = (tilesheetData.sprite_offset_y as number) ?? 0
    this.writeDim = this.spriteWidth !== refs.defaultWidth ||
      this.spriteHeight !== refs.defaultHeight ||
      this.spriteOffsetX !== 0 || this.spriteOffsetY !== 0

    this.tsPathname = join(refs.tilesetPathname, this.tsFilename)

    this.pngnumMin = refs.lastPngnum
    this.pngnumMax = 0
    this.tsWidth = 0
    this.tsHeight = 0
    this.tsTilesPerRow = 0
    this.tsRows = 0
    this.fallback = tilesheetData.ascii
  }

  async initialize(refs: PngRefs): Promise<void> {
    try {
      const metadata = await sharp(this.tsPathname).metadata()
      this.tsWidth = metadata.width ?? 0
      this.tsHeight = metadata.height ?? 0
      this.tsTilesPerRow = Math.floor(this.tsWidth / this.spriteWidth)
      this.tsRows = Math.floor(this.tsHeight / this.spriteHeight)
      this.pngnumMin = refs.lastPngnum
      this.pngnumMax = refs.lastPngnum + this.tsTilesPerRow * this.tsRows - 1
      refs.lastPngnum = this.pngnumMax + 1
      refs.tsPathnameToFilename.set(this.tsPathname, this.tsFilename)
    } catch {
      // PNG file doesn't exist (e.g., fallback sheet), keep default values
      this.tsWidth = 0
      this.tsHeight = 0
      this.tsTilesPerRow = 0
      this.tsRows = 0
    }
  }

  checkForExpansion(tileEntry: DecomposeTileEntry): boolean {
    if (tileEntry.fg === 0) {
      this.expansions.push(tileEntry)
      return true
    }
    return false
  }

  checkIdValid(tileId: string): boolean {
    if (!tileId) return true
    if (tileId.startsWith("overlay_wielded_t_")) return false
    if (tileId.startsWith("overlay_wielded_mon_")) return false
    if (tileId.startsWith("overlay_wielded_fd_")) return false
    if (tileId.startsWith("overlay_wielded_f_")) return false
    if (tileId.startsWith("overlay_wielded_overlay")) return false
    if (tileId.startsWith("overlay_worn_overlay")) return false
    return true
  }

  parseId(tileEntry: DecomposeTileEntry): [string | null, string[] | null] {
    const allTileIds: string[] = []
    const readTileIds = tileEntry.id
    let valid = true

    if (Array.isArray(readTileIds)) {
      for (const tileId of readTileIds) {
        valid = valid && this.checkIdValid(tileId)
        if (tileId && valid && !allTileIds.includes(tileId)) {
          allTileIds.push(tileId)
        }
      }
    } else if (typeof readTileIds === "string") {
      valid = valid && this.checkIdValid(readTileIds)
      if (readTileIds && valid && !allTileIds.includes(readTileIds)) {
        allTileIds.push(readTileIds)
      }
    }

    if (!valid) return [null, null]
    if (allTileIds.length === 0) return ["background", ["background"]]
    return [allTileIds[0], allTileIds]
  }

  parseIndex(
    readPngnums: number | number[] | Record<string, unknown> | Record<string, unknown>[],
    allPngnums: number[],
    refs: PngRefs,
  ): number[] {
    const localPngnums: number[] = []

    const addPngnum = (pngnum: number) => {
      if (pngnum < 0 || refs.deletePngnums.includes(pngnum)) return
      if (!allPngnums.includes(pngnum)) allPngnums.push(pngnum)
      if (!localPngnums.includes(pngnum)) localPngnums.push(pngnum)
    }

    if (Array.isArray(readPngnums)) {
      for (const pngnum of readPngnums) {
        if (typeof pngnum === "object" && pngnum !== null && "sprite" in pngnum) {
          const spriteIds = pngnum.sprite
          if (Array.isArray(spriteIds)) {
            for (const spriteId of spriteIds) {
              if (typeof spriteId === "number") addPngnum(spriteId)
            }
          } else if (typeof spriteIds === "number") {
            addPngnum(spriteIds)
          }
        } else if (typeof pngnum === "number") {
          addPngnum(pngnum)
        }
      }
    } else if (typeof readPngnums === "number") {
      addPngnum(readPngnums)
    }

    return allPngnums
  }

  parsePng(tileEntry: DecomposeTileEntry, refs: PngRefs): number[] {
    let allPngnums: number[] = []

    const fgId = tileEntry.fg
    if (fgId !== undefined) {
      allPngnums = this.parseIndex(fgId, allPngnums, refs)
    }

    const bgId = tileEntry.bg
    if (bgId !== undefined) {
      allPngnums = this.parseIndex(bgId, allPngnums, refs)
    }

    const addTileEntries = tileEntry.additional_tiles ?? []
    for (const addTileEntry of addTileEntries) {
      const addPngnums = this.parsePng(addTileEntry, refs)
      for (const addPngnum of addPngnums) {
        if (!allPngnums.includes(addPngnum)) {
          allPngnums.push(addPngnum)
        }
      }
    }

    return allPngnums
  }

  parseTileEntry(tileEntry: DecomposeTileEntry, refs: PngRefs): string | null {
    if (this.checkForExpansion(tileEntry)) return null

    const [tileId, allTileIds] = this.parseId(tileEntry)
    if (!tileId) return null

    const safeTileId = tileId.replace(/\//g, "_")
    const allPngnums = this.parsePng(tileEntry, refs)
    let offset = 0

    for (let i = 0; i < allPngnums.length; i++) {
      const pngnum = allPngnums[i]
      if (refs.pngnumToPngname.has(pngnum)) continue

      let pngname = `${pngnum}_${safeTileId}_${i + offset}`
      while (refs.pngnameToPngnum.has(pngname)) {
        offset++
        pngname = `${pngnum}_${safeTileId}_${i + offset}`
      }

      refs.pngnumToPngname.set(pngnum, pngname)
      refs.pngnameToPngnum.set(pngname, pngnum)
      refs.addPngnumToTsFilepath(pngnum)
    }

    return tileId
  }

  summarize(
    tileInfo: Array<Record<string, unknown>>,
    refs: PngRefs,
  ): void {
    if (this.fallback) {
      refs.tsData.set(this.tsFilename, this)
      tileInfo.push({ [this.tsFilename]: { fallback: true } })
      return
    }

    if (this.pngnumMax > 0) {
      refs.tsData.set(this.tsFilename, this)
      const tsTileInfo: Record<string, unknown> = {
        "//": `indices ${this.pngnumMin} to ${this.pngnumMax}`,
      }

      if (this.writeDim) {
        tsTileInfo.sprite_offset_x = this.spriteOffsetX
        tsTileInfo.sprite_offset_y = this.spriteOffsetY
        tsTileInfo.sprite_width = this.spriteWidth
        tsTileInfo.sprite_height = this.spriteHeight
      }

      tileInfo.push({ [this.tsFilename]: tsTileInfo })
    }
  }
}

class ExtractionData {
  tsData: TileSheetData
  valid = false
  tsDirPathname = ""
  tilenumInDir = 256
  dirCount = 0
  subdirPathname = ""

  constructor(tsFilename: string, refs: PngRefs) {
    const tsData = refs.tsData.get(tsFilename)
    if (!tsData) throw new Error(`No data for ${tsFilename}`)
    this.tsData = tsData

    if (!this.tsData.spriteWidth || !this.tsData.spriteHeight) {
      return
    }

    this.valid = true

    const tsBase = tsFilename.split(".png")[0]
    const geometryDim = `${this.tsData.spriteWidth}x${this.tsData.spriteHeight}`
    const pngsDir = `/pngs_${tsBase}_${geometryDim}`
    this.tsDirPathname = refs.tilesetPathname + pngsDir

    try {
      Deno.mkdirSync(this.tsDirPathname, { recursive: true })
    } catch {
      // Directory may already exist
    }
  }

  async writeExpansions(): Promise<void> {
    for (const expandEntry of this.tsData.expansions) {
      const expansionId = Array.isArray(expandEntry.id)
        ? expandEntry.id[0]
        : expandEntry.id
      if (typeof expansionId !== "string") continue

      const expandEntryPathname = join(this.tsDirPathname, `${expansionId}.json`)
      await writeToJson(expandEntryPathname, expandEntry)
    }
  }

  incrementDir(): string {
    if (this.tilenumInDir > 255) {
      this.subdirPathname = join(this.tsDirPathname, `images${this.dirCount}`)
      try {
        Deno.mkdirSync(this.subdirPathname, { recursive: true })
      } catch {
        // Directory may already exist
      }
      this.tilenumInDir = 0
      this.dirCount++
    } else {
      this.tilenumInDir++
    }
    return this.subdirPathname
  }

  async extractImage(pngIndex: number, refs: PngRefs): Promise<void> {
    if (!pngIndex || refs.extractedPngnums.has(pngIndex)) return
    if (!refs.pngnumToPngname.has(pngIndex)) return

    const pngname = refs.pngnumToPngname.get(pngIndex)!
    const tsPathname = refs.pngnumToTsPathname.get(pngIndex)
    if (!tsPathname) return

    const tsFilename = refs.tsPathnameToFilename.get(tsPathname)
    if (!tsFilename) return

    this.incrementDir()

    const tileData = refs.tsData.get(tsFilename)
    if (!tileData) return

    const fileIndex = pngIndex - tileData.pngnumMin
    const yIndex = Math.floor(fileIndex / tileData.tsTilesPerRow)
    const xIndex = fileIndex - yIndex * tileData.tsTilesPerRow
    const tileOffX = Math.max(0, tileData.spriteWidth * xIndex)
    const tileOffY = Math.max(0, tileData.spriteHeight * yIndex)

    const tilePngPathname = join(this.subdirPathname, `${pngname}.png`)

    await sharp(tileData.tsPathname)
      .extract({
        left: tileOffX,
        top: tileOffY,
        width: tileData.spriteWidth,
        height: tileData.spriteHeight,
      })
      .toFile(tilePngPathname)

    refs.extractedPngnums.add(pngIndex)
  }

  async writeImages(refs: PngRefs): Promise<void> {
    for (let pngnum = this.tsData.pngnumMin; pngnum <= this.tsData.pngnumMax; pngnum++) {
      await this.extractImage(pngnum, refs)
    }
  }
}

class PngRefs {
  pngnumToPngname = new Map<number, string>()
  pngnameToPngnum = new Map<string, number>()
  pngnumToTsPathname = new Map<number, string>()
  tsPathnameToFilename = new Map<string, string>()
  extractedPngnums = new Set<number>()
  deletePngnums: number[] = []
  tilesetPathname = ""
  defaultWidth = 16
  defaultHeight = 16
  lastPngnum = 0
  tsData = new Map<string, TileSheetData>()

  async getAllData(tilesetDirname: string, deletePathname?: string): Promise<Record<string, unknown>> {
    this.tilesetPathname = resolve(tilesetDirname)

    try {
      const stat = await Deno.stat(this.tilesetPathname)
      if (!stat.isDirectory) {
        console.error(`${this.tilesetPathname} is not a directory`)
        Deno.exit(1)
      }
    } catch {
      console.error(`cannot find directory ${this.tilesetPathname}`)
      Deno.exit(1)
    }

    const tilesetConfname = join(this.tilesetPathname, "tile_config.json")

    try {
      await Deno.stat(tilesetConfname)
    } catch {
      console.error(`cannot find ${tilesetConfname}`)
      Deno.exit(1)
    }

    if (deletePathname) {
      try {
        const delContent = await Deno.readTextFile(deletePathname)
        const delRanges = JSON.parse(delContent)
        for (const deleteRange of delRanges) {
          if (!Array.isArray(deleteRange)) continue
          const minPng = deleteRange[0]
          let maxPng = minPng
          if (deleteRange.length > 1) {
            maxPng = deleteRange[1]
          }
          for (let i = minPng; i <= maxPng; i++) {
            this.deletePngnums.push(i)
          }
        }
      } catch {
        // Ignore if delete file doesn't exist
      }
    }

    const confContent = await Deno.readTextFile(tilesetConfname)
    return JSON.parse(confContent)
  }

  addPngnumToTsFilepath(pngnum: number): void {
    if (typeof pngnum !== "number") return
    if (this.pngnumToTsPathname.has(pngnum)) return

    for (const [tsFilename, tsData] of this.tsData) {
      if (pngnum >= tsData.pngnumMin && pngnum <= tsData.pngnumMax) {
        this.pngnumToTsPathname.set(pngnum, tsData.tsPathname)
        return
      }
    }

    throw new Error(`index ${pngnum} out of range`)
  }

  convertIndex(
    readPngnums: number | number[] | Record<string, unknown> | Record<string, unknown>[],
    newIndex: (string | Record<string, unknown>)[],
  ): void {
    if (Array.isArray(readPngnums)) {
      for (const pngnum of readPngnums) {
        if (typeof pngnum === "object" && pngnum !== null && "sprite" in pngnum) {
          const spriteIds = pngnum.sprite
          if (Array.isArray(spriteIds)) {
            const newSprites: string[] = []
            for (const spriteId of spriteIds) {
              if (
                typeof spriteId === "number" && spriteId >= 0 &&
                !this.deletePngnums.includes(spriteId)
              ) {
                const name = this.pngnumToPngname.get(spriteId)
                if (name) newSprites.push(name)
              }
            }
            pngnum.sprite = newSprites
          } else if (
            typeof spriteIds === "number" && spriteIds >= 0 &&
            !this.deletePngnums.includes(spriteIds)
          ) {
            const name = this.pngnumToPngname.get(spriteIds)
            if (name) pngnum.sprite = name
          }
          newIndex.push(pngnum)
        } else if (typeof pngnum === "number" && pngnum >= 0 && !this.deletePngnums.includes(pngnum)) {
          const name = this.pngnumToPngname.get(pngnum)
          if (name) newIndex.push(name)
        }
      }
    } else if (
      typeof readPngnums === "number" && readPngnums >= 0 &&
      !this.deletePngnums.includes(readPngnums)
    ) {
      const name = this.pngnumToPngname.get(readPngnums)
      if (name) newIndex.push(name)
    }
  }

  convertPngnumToPngname(tileEntry: DecomposeTileEntry): [string, DecomposeTileEntry] {
    const newFg: (string | Record<string, unknown>)[] = []
    const fgId = tileEntry.fg
    if (fgId !== undefined) {
      this.convertIndex(fgId, newFg)
    }

    const newBg: (string | Record<string, unknown>)[] = []
    const bgId = tileEntry.bg
    if (bgId !== undefined) {
      this.convertIndex(bgId, newBg)
    }

    const addTileEntries = tileEntry.additional_tiles ?? []
    for (const addTileEntry of addTileEntries) {
      this.convertPngnumToPngname(addTileEntry)
    }

    tileEntry.fg = newFg
    tileEntry.bg = newBg

    let newId = ""
    const entityId = tileEntry.id
    if (Array.isArray(entityId)) {
      newId = entityId.join("_").substring(0, 150)
    } else {
      newId = entityId
    }

    return [newId, tileEntry]
  }

  reportMissing(): void {
    for (const [pngnum, pngname] of this.pngnumToPngname) {
      if (!this.extractedPngnums.has(pngnum)) {
        console.log(`missing index ${pngnum}, ${pngname}`)
      }
    }
  }
}

async function unpack(tilesetDir: string, deleteFile?: string): Promise<void> {
  const refs = new PngRefs()
  const allTiles = await refs.getAllData(tilesetDir, deleteFile)

  const allTilesheetData = (allTiles["tiles-new"] ?? []) as Record<string, unknown>[]
  const tileInfo = (allTiles.tile_info ?? []) as Array<Record<string, unknown>>

  if (tileInfo.length > 0) {
    const info = tileInfo[0] as TileInfo
    refs.defaultWidth = info.width ?? 16
    refs.defaultHeight = info.height ?? 16
  }

  const tsSequence: string[] = []
  const tsDataList: TileSheetData[] = []

  // Create all TileSheetData instances and initialize them
  for (const tilesheetData of allTilesheetData) {
    const tsData = new TileSheetData(tilesheetData, refs)
    await tsData.initialize(refs)
    tsDataList.push(tsData)
  }

  // Now summarize after all are initialized
  for (const tsData of tsDataList) {
    tsData.summarize(tileInfo, refs)
    tsSequence.push(tsData.tsFilename)
  }

  for (const tilesheet of allTilesheetData) {
    const tsFilename = tilesheet.file as string
    const tsData = refs.tsData.get(tsFilename)
    if (!tsData || tsData.fallback) continue

    const tileIdToTileEntries: Record<string, DecomposeTileEntry[]> = {}
    const allTileEntry = (tilesheet.tiles ?? []) as DecomposeTileEntry[]

    for (const tileEntry of allTileEntry) {
      const tileId = tsData.parseTileEntry(tileEntry, refs)
      if (tileId) {
        if (!tileIdToTileEntries[tileId]) {
          tileIdToTileEntries[tileId] = []
        }
        tileIdToTileEntries[tileId].push(tileEntry)
      }
    }

    tsData.tileIdToTileEntries = tileIdToTileEntries
  }

  for (const tsFilename of tsSequence) {
    const outData = new ExtractionData(tsFilename, refs)

    if (!outData.valid) continue
    await outData.writeExpansions()

    for (const [tileId, tileEntries] of Object.entries(outData.tsData.tileIdToTileEntries)) {
      for (let idx = 0; idx < tileEntries.length; idx++) {
        const tileEntry = tileEntries[idx]
        const subdirPathname = outData.incrementDir()
        const [tileEntryName, convertedEntry] = refs.convertPngnumToPngname(tileEntry)
        if (!tileEntryName) continue

        const tileEntryPathname = join(subdirPathname, `${tileEntryName}_${idx}.json`)
        await writeToJson(tileEntryPathname, convertedEntry)
      }
    }

    await outData.writeImages(refs)
  }

  if (tileInfo.length > 0) {
    const tileInfoPathname = join(refs.tilesetPathname, "tile_info.json")
    await writeToJson(tileInfoPathname, tileInfo, true)
  }

  refs.reportMissing()
}

await new Command()
  .name("tileset")
  .version("0.1.0")
  .description("Pack and unpack tileset atlases for Cataclysm: Bright Nights")
  .option("--pack", "Pack individual folders into tileset atlas")
  .option("--unpack", "Unpack tileset atlas into individual folders")
  .option("--use-all", "Add unused images with id being their basename")
  .option("--obsolete-fillers", "Warn about obsoleted fillers")
  .option("--palette-copies", "Produce copies of tilesheets quantized to 8bpp colormaps")
  .option("--palette", "Quantize all tilesheets to 8bpp colormaps")
  .option("--format-json", "Format the tile_config.json")
  .option("--only-json", "Only output the tile_config.json")
  .option("--delete-file <path:string>", "File containing lists of ranges of indices to delete")
  .arguments("<source_dir:string> [output_dir:string]")
  .action(async (options, sourceDir, outputDir) => {
    if (options.pack) {
      await pack(sourceDir, outputDir ?? sourceDir, {
        useAll: options.useAll,
        obsoleteFillers: options.obsoleteFillers,
        paletteCopies: options.paletteCopies,
        palette: options.palette,
        formatJson: options.formatJson,
        onlyJson: options.onlyJson,
      })
    } else if (options.unpack) {
      await unpack(sourceDir, options.deleteFile)
    } else {
      console.error("Please specify either --pack or --unpack")
      Deno.exit(1)
    }
  })
  .parse(Deno.args)
