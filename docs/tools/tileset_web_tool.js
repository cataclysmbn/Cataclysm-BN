// @ts-nocheck: browser-only script with dynamic DOM APIs and CDN import.
import JSZip from "https://esm.sh/jszip@3.10.1"

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

const listOrFirst = (values) => (values.length === 1 ? values[0] : values)

const normalizePath = (pathname) => pathname.replaceAll("\\", "/")

const stripUploadRoot = (pathname) => {
  const normalized = normalizePath(pathname)
  const separator = normalized.indexOf("/")
  if (separator === -1) {
    return normalized
  }
  return normalized.slice(separator + 1)
}

const makeError = (message) => new Error(`Tileset web tool: ${message}`)

const readText = (file) => file.text()

const readJson = async (file) => JSON.parse(await readText(file))

const canvasToPngBlob = async (canvas) => {
  const blob = await new Promise((resolve) => canvas.toBlob(resolve, "image/png"))
  if (!blob) {
    throw makeError("failed to encode PNG")
  }
  return blob
}

const readImageData = async (file) => {
  const bitmap = await createImageBitmap(file)
  const canvas = document.createElement("canvas")
  canvas.width = bitmap.width
  canvas.height = bitmap.height
  const context = canvas.getContext("2d")
  context.drawImage(bitmap, 0, 0)
  bitmap.close()
  return {
    width: canvas.width,
    height: canvas.height,
    imageData: context.getImageData(0, 0, canvas.width, canvas.height),
  }
}

const createTransparentImageData = (width, height) => new ImageData(width, height)

const composeAtlas = (sprites, spriteWidth, spriteHeight, spritesAcross) => {
  const rows = Math.ceil(sprites.length / spritesAcross)
  const canvas = document.createElement("canvas")
  canvas.width = spriteWidth * spritesAcross
  canvas.height = spriteHeight * rows

  const context = canvas.getContext("2d")
  for (let index = 0; index < sprites.length; index += 1) {
    const column = index % spritesAcross
    const row = Math.floor(index / spritesAcross)
    context.putImageData(sprites[index], column * spriteWidth, row * spriteHeight)
  }

  return canvasToPngBlob(canvas)
}

const buildFileMap = (fileList) => {
  const files = new Map()
  for (const file of fileList) {
    const relativePath = stripUploadRoot(file.webkitRelativePath || file.name)
    files.set(relativePath, file)
  }
  return files
}

const getRequiredFile = (files, pathname) => {
  const file = files.get(pathname)
  if (!file) {
    throw makeError(`missing required file: ${pathname}`)
  }
  return file
}

const listSheetFiles = (files, prefix, extension) => {
  const matches = []
  for (const pathname of files.keys()) {
    if (pathname.startsWith(prefix) && pathname.endsWith(extension)) {
      matches.push(pathname)
    }
  }
  matches.sort((left, right) => left.localeCompare(right))
  return matches
}

const convertLayer = (layer, context) => {
  const output = []

  const appendSpriteIndex = (spriteName, target) => {
    if (typeof spriteName === "number") {
      if (spriteName > 0) {
        target.push(spriteName)
        return true
      }
      return false
    }

    if (typeof spriteName !== "string" || !spriteName) {
      return false
    }

    const spriteIndex = context.nameToIndex.get(spriteName) ?? 0
    if (!spriteIndex) {
      context.log(`missing sprite reference: ${spriteName}`)
      return false
    }

    context.unreferenced.delete(spriteName)
    target.push(spriteIndex)
    return true
  }

  const convertVariations = (variations) => {
    const converted = []
    if (Array.isArray(variations)) {
      for (const variation of variations) {
        appendSpriteIndex(variation, converted)
      }
    } else {
      appendSpriteIndex(variations, converted)
    }
    return converted
  }

  if (Array.isArray(layer)) {
    for (const part of layer) {
      if (part && typeof part === "object" && "sprite" in part) {
        const convertedSprite = convertVariations(part.sprite)
        if (convertedSprite.length > 0) {
          const convertedPart = structuredClone(part)
          convertedPart.sprite = listOrFirst(convertedSprite)
          output.push(convertedPart)
        }
      } else {
        appendSpriteIndex(part, output)
      }
    }
  } else {
    appendSpriteIndex(layer, output)
  }

  return output
}

const convertTileEntry = (entry, prefix, context) => {
  const converted = structuredClone(entry)
  const ids = Array.isArray(converted.id) ? [...converted.id] : [converted.id]

  if (!ids[0] || (converted.fg === undefined && converted.bg === undefined)) {
    return null
  }

  if (converted.fg !== undefined) {
    const fgLayer = convertLayer(converted.fg, context)
    if (fgLayer.length > 0) {
      converted.fg = listOrFirst(fgLayer)
    } else {
      delete converted.fg
    }
  }

  if (converted.bg !== undefined) {
    const bgLayer = convertLayer(converted.bg, context)
    if (bgLayer.length > 0) {
      converted.bg = listOrFirst(bgLayer)
    } else {
      delete converted.bg
    }
  }

  if (Array.isArray(converted.masks)) {
    converted.masks = converted.masks.map((mask) => {
      const convertedMask = structuredClone(mask)
      if (convertedMask.fg) {
        convertedMask.fg = listOrFirst(convertLayer(convertedMask.fg, context))
      } else {
        delete convertedMask.fg
      }
      if (convertedMask.bg) {
        convertedMask.bg = listOrFirst(convertLayer(convertedMask.bg, context))
      } else {
        delete convertedMask.bg
      }
      return convertedMask
    })
  }

  const additionalTiles = Array.isArray(converted.additional_tiles)
    ? converted.additional_tiles
    : []
  const nestedPrefix = `${ids[0]}_`
  const nextAdditional = []
  for (const additional of additionalTiles) {
    const convertedAdditional = convertTileEntry(additional, nestedPrefix, context)
    if (convertedAdditional) {
      nextAdditional.push(convertedAdditional)
    }
  }
  if (nextAdditional.length > 0) {
    converted.additional_tiles = nextAdditional
  } else {
    delete converted.additional_tiles
  }

  const validIds = []
  for (const id of ids) {
    const fullId = `${prefix}${id}`
    if (!context.processedIds.has(fullId)) {
      context.processedIds.add(fullId)
      validIds.push(id)
    }
  }

  if (validIds.length === 0) {
    return null
  }

  converted.id = listOrFirst(validIds)
  return converted
}

const composeSmallTileset = async (files, log) => {
  const tileInfo = await readJson(getRequiredFile(files, "tile_info.json"))
  if (!Array.isArray(tileInfo) || tileInfo.length < 2) {
    throw makeError("tile_info.json must contain base info and one sheet entry")
  }

  const baseInfo = tileInfo[0] ?? {}
  const defaultWidth = baseInfo.width ?? 16
  const defaultHeight = baseInfo.height ?? 16

  const sheetEntries = tileInfo.slice(1)
    .map((entry) => {
      const filename = Object.keys(entry)[0]
      return [filename, entry[filename] ?? {}]
    })

  const fallbackEntry = sheetEntries.find(([, spec]) => spec.fallback)
  const composeEntry = sheetEntries.find(([, spec]) => !spec.fallback)
  if (!composeEntry) {
    throw makeError("tile_info.json does not contain a non-fallback sheet")
  }

  const [sheetName, spec] = composeEntry
  const spriteWidth = spec.sprite_width ?? defaultWidth
  const spriteHeight = spec.sprite_height ?? defaultHeight
  const spritesAcross = spec.sprites_across ?? 16

  const sheetBase = sheetName.replace(/\.png$/i, "")
  const sheetPrefix = `pngs_${sheetBase}_${spriteWidth}x${spriteHeight}/`
  const pngPaths = listSheetFiles(files, sheetPrefix, ".png")
  const jsonPaths = listSheetFiles(files, sheetPrefix, ".json")

  if (pngPaths.length === 0) {
    throw makeError(`no PNG sprites found in ${sheetPrefix}`)
  }

  if (jsonPaths.length === 0) {
    throw makeError(`no tile JSON entries found in ${sheetPrefix}`)
  }

  const sprites = [createTransparentImageData(spriteWidth, spriteHeight)]
  const nameToIndex = new Map([["null_image", 0]])
  const unreferenced = new Set()

  for (const pngPath of pngPaths) {
    const stem = pngPath.split("/").at(-1).replace(/\.png$/i, "")
    if (nameToIndex.has(stem)) {
      log(`duplicate sprite name skipped: ${stem}`)
      continue
    }

    const image = await readImageData(getRequiredFile(files, pngPath))
    if (image.width !== spriteWidth || image.height !== spriteHeight) {
      throw makeError(
        `${pngPath} is ${image.width}x${image.height}, expected ${spriteWidth}x${spriteHeight}`,
      )
    }

    const nextIndex = sprites.length
    sprites.push(image.imageData)
    nameToIndex.set(stem, nextIndex)
    unreferenced.add(stem)
  }

  const processedIds = new Set()
  const tileEntries = []

  for (const jsonPath of jsonPaths) {
    const parsed = await readJson(getRequiredFile(files, jsonPath))
    const entries = Array.isArray(parsed) ? parsed : [parsed]

    for (const entry of entries) {
      const converted = convertTileEntry(entry, "", {
        log,
        nameToIndex,
        processedIds,
        unreferenced,
      })
      if (converted) {
        tileEntries.push(converted)
      }
    }
  }

  for (const spriteName of unreferenced) {
    if (!processedIds.has(spriteName)) {
      tileEntries.push({ id: spriteName, fg: nameToIndex.get(spriteName) })
    }
  }

  const remainder = sprites.length % spritesAcross
  const padding = remainder === 0 ? 0 : spritesAcross - remainder
  for (let index = 0; index < padding; index += 1) {
    sprites.push(createTransparentImageData(spriteWidth, spriteHeight))
  }

  const sheetBlob = await composeAtlas(sprites, spriteWidth, spriteHeight, spritesAcross)
  const maxIndex = sprites.length - 1

  const sheetConfig = {
    file: sheetName,
    "//": `range 1 to ${maxIndex}`,
    tiles: tileEntries,
  }

  const isStandard = spriteWidth === defaultWidth &&
    spriteHeight === defaultHeight &&
    (spec.sprite_offset_x ?? 0) === 0 &&
    (spec.sprite_offset_y ?? 0) === 0 &&
    (spec.pixelscale ?? 1) === 1

  if (!isStandard) {
    sheetConfig.sprite_width = spriteWidth
    sheetConfig.sprite_height = spriteHeight
    sheetConfig.sprite_offset_x = spec.sprite_offset_x ?? 0
    sheetConfig.sprite_offset_y = spec.sprite_offset_y ?? 0
    if ((spec.pixelscale ?? 1) !== 1) {
      sheetConfig.pixelscale = spec.pixelscale
    }
  }

  const fallbackName = fallbackEntry?.[0] ?? "fallback.png"
  const fallbackConfig = structuredClone(FALLBACK_TEMPLATE)
  fallbackConfig.file = fallbackName

  const outputConfig = {
    tile_info: [
      {
        width: defaultWidth,
        height: defaultHeight,
        pixelscale: baseInfo.pixelscale ?? 1,
        iso: baseInfo.iso ?? false,
        retract_dist_min: baseInfo.retract_dist_min ?? -1,
        retract_dist_max: baseInfo.retract_dist_max ?? 1,
      },
    ],
    "tiles-new": [sheetConfig, fallbackConfig],
  }

  return new Map([
    ["tile_config.json", JSON.stringify(outputConfig, null, 2)],
    ["tileset.txt", "JSON: tile_config.json\n"],
    [sheetName, sheetBlob],
  ])
}

const collectIndicesFromLayer = (layer, destination) => {
  if (Array.isArray(layer)) {
    for (const part of layer) {
      if (typeof part === "number" && part >= 0) {
        destination.add(part)
      } else if (part && typeof part === "object" && "sprite" in part) {
        collectIndicesFromLayer(part.sprite, destination)
      }
    }
    return
  }

  if (typeof layer === "number" && layer >= 0) {
    destination.add(layer)
  }
}

const convertLayerToNames = (layer, indexToName) => {
  const output = []

  const appendName = (value, target) => {
    if (typeof value === "number") {
      const name = indexToName.get(value)
      if (name) {
        target.push(name)
      }
      return
    }

    if (Array.isArray(value)) {
      for (const child of value) {
        appendName(child, target)
      }
    }
  }

  if (Array.isArray(layer)) {
    for (const part of layer) {
      if (part && typeof part === "object" && "sprite" in part) {
        const convertedPart = structuredClone(part)
        const names = []
        appendName(part.sprite, names)
        convertedPart.sprite = listOrFirst(names)
        output.push(convertedPart)
      } else {
        appendName(part, output)
      }
    }
  } else {
    appendName(layer, output)
  }

  return output
}

const convertEntryToNames = (entry, indexToName) => {
  const converted = structuredClone(entry)

  const fgNames = convertLayerToNames(converted.fg, indexToName)
  if (fgNames.length > 0) {
    converted.fg = fgNames
  } else {
    delete converted.fg
  }

  const bgNames = convertLayerToNames(converted.bg, indexToName)
  if (bgNames.length > 0) {
    converted.bg = bgNames
  } else {
    delete converted.bg
  }

  if (Array.isArray(converted.masks)) {
    converted.masks = converted.masks
      .map((mask) => {
        const convertedMask = structuredClone(mask)
        const maskFg = convertLayerToNames(convertedMask.fg, indexToName)
        const maskBg = convertLayerToNames(convertedMask.bg, indexToName)
        if (maskFg.length > 0) {
          convertedMask.fg = listOrFirst(maskFg)
        } else {
          delete convertedMask.fg
        }
        if (maskBg.length > 0) {
          convertedMask.bg = listOrFirst(maskBg)
        } else {
          delete convertedMask.bg
        }
        return maskFg.length > 0 || maskBg.length > 0 ? convertedMask : null
      })
      .filter(Boolean)
  }

  if (Array.isArray(converted.additional_tiles)) {
    converted.additional_tiles = converted.additional_tiles
      .map((additional) => convertEntryToNames(additional, indexToName)[1])
      .filter(Boolean)
  }

  const id = Array.isArray(converted.id)
    ? converted.id.join("_").slice(0, 150)
    : String(converted.id ?? "tile")

  return [id, converted]
}

const extractSprite = (sourceCanvas, x, y, width, height) => {
  const canvas = document.createElement("canvas")
  canvas.width = width
  canvas.height = height
  const context = canvas.getContext("2d")
  context.drawImage(sourceCanvas, x, y, width, height, 0, 0, width, height)
  return canvasToPngBlob(canvas)
}

const decomposeSmallTileset = async (files) => {
  const tileConfig = await readJson(getRequiredFile(files, "tile_config.json"))
  const tileInfo = tileConfig.tile_info ?? []
  const baseInfo = tileInfo[0] ?? {}
  const defaultWidth = baseInfo.width ?? 16
  const defaultHeight = baseInfo.height ?? 16

  const sheets = tileConfig["tiles-new"] ?? []
  const composeSheet = sheets.find((sheet) => sheet.file && Array.isArray(sheet.tiles))
  if (!composeSheet) {
    throw makeError("tile_config.json does not contain a decomposable tilesheet")
  }

  const sheetName = composeSheet.file
  const spriteWidth = composeSheet.sprite_width ?? defaultWidth
  const spriteHeight = composeSheet.sprite_height ?? defaultHeight

  const sheetImage = await readImageData(getRequiredFile(files, sheetName))
  const tilesPerRow = Math.floor(sheetImage.width / spriteWidth)
  const rows = Math.floor(sheetImage.height / spriteHeight)
  const maxIndex = tilesPerRow * rows - 1

  const sourceCanvas = document.createElement("canvas")
  sourceCanvas.width = sheetImage.width
  sourceCanvas.height = sheetImage.height
  sourceCanvas.getContext("2d").putImageData(sheetImage.imageData, 0, 0)

  const indexToName = new Map()
  const usedNames = new Set()

  const safeId = (value) => String(value ?? "tile").replaceAll("/", "_")

  const collectEntryIndices = (entry, indices) => {
    collectIndicesFromLayer(entry.fg, indices)
    collectIndicesFromLayer(entry.bg, indices)

    if (Array.isArray(entry.masks)) {
      for (const mask of entry.masks) {
        collectIndicesFromLayer(mask.fg, indices)
        collectIndicesFromLayer(mask.bg, indices)
      }
    }

    if (Array.isArray(entry.additional_tiles)) {
      for (const additional of entry.additional_tiles) {
        collectEntryIndices(additional, indices)
      }
    }
  }

  for (const entry of composeSheet.tiles) {
    const indices = new Set()
    collectEntryIndices(entry, indices)

    const ids = Array.isArray(entry.id) ? entry.id : [entry.id]
    const baseName = safeId(ids[0])

    let offset = 0
    const sortedIndices = Array.from(indices).sort((left, right) => left - right)
    for (let position = 0; position < sortedIndices.length; position += 1) {
      const index = sortedIndices[position]
      if (indexToName.has(index)) {
        continue
      }

      let candidate = `${index}_${baseName}_${position + offset}`
      while (usedNames.has(candidate)) {
        offset += 1
        candidate = `${index}_${baseName}_${position + offset}`
      }

      indexToName.set(index, candidate)
      usedNames.add(candidate)
    }
  }

  const outputFiles = new Map()
  const sheetBase = sheetName.replace(/\.png$/i, "")
  const targetPrefix = `pngs_${sheetBase}_${spriteWidth}x${spriteHeight}/images0`

  const sortedIndices = Array.from(indexToName.keys()).sort((left, right) => left - right)
  for (const index of sortedIndices) {
    const name = indexToName.get(index)
    const column = index % tilesPerRow
    const row = Math.floor(index / tilesPerRow)
    if (row >= rows) {
      continue
    }

    const x = column * spriteWidth
    const y = row * spriteHeight
    const spriteBlob = await extractSprite(sourceCanvas, x, y, spriteWidth, spriteHeight)
    outputFiles.set(`${targetPrefix}/${name}.png`, spriteBlob)
  }

  const tileEntries = Array.isArray(composeSheet.tiles) ? composeSheet.tiles : []
  for (let index = 0; index < tileEntries.length; index += 1) {
    const [tileName, converted] = convertEntryToNames(tileEntries[index], indexToName)
    outputFiles.set(`${targetPrefix}/${tileName}_${index}.json`, JSON.stringify(converted, null, 2))
  }

  const tileInfoOut = [
    {
      width: defaultWidth,
      height: defaultHeight,
      pixelscale: baseInfo.pixelscale ?? 1,
      iso: baseInfo.iso ?? false,
      retract_dist_min: baseInfo.retract_dist_min ?? -1,
      retract_dist_max: baseInfo.retract_dist_max ?? 1,
    },
    {
      [sheetName]: {
        "//": `indices 0 to ${maxIndex}`,
      },
    },
  ]

  const fallbackSheet = sheets.find((sheet) => sheet.ascii && sheet.file)
  if (fallbackSheet) {
    tileInfoOut.push({
      [fallbackSheet.file]: {
        fallback: true,
      },
    })
  }

  outputFiles.set("tile_info.json", JSON.stringify(tileInfoOut, null, 2))
  outputFiles.set("tileset.txt", "JSON: tile_config.json\n")

  return outputFiles
}

const createZipBlob = (files) => {
  const zip = new JSZip()
  for (const [pathname, content] of files) {
    zip.file(pathname, content)
  }
  return zip.generateAsync({ type: "blob" })
}

const setupSpriteEditor = () => {
  const input = document.getElementById("sprite-editor-input")
  const widthInput = document.getElementById("sprite-editor-width")
  const heightInput = document.getElementById("sprite-editor-height")
  const newButton = document.getElementById("sprite-editor-new")
  const colorInput = document.getElementById("sprite-editor-color")
  const downloadButton = document.getElementById("sprite-editor-download")
  const canvas = document.getElementById("sprite-editor-canvas")

  if (
    !input || !widthInput || !heightInput || !newButton || !colorInput || !downloadButton || !canvas
  ) {
    return
  }

  const context = canvas.getContext("2d")
  const backingCanvas = document.createElement("canvas")
  const backingContext = backingCanvas.getContext("2d")
  let scale = 16
  let painting = false

  const clampDimension = (value) => Math.max(1, Math.min(256, Number.parseInt(value, 10) || 16))
  const currentTool = () =>
    document.querySelector("input[name='sprite-editor-tool']:checked")?.value ?? "pen"
  const syncSizeInputs = () => {
    widthInput.value = String(backingCanvas.width)
    heightInput.value = String(backingCanvas.height)
  }
  const redraw = () => {
    scale = Math.max(1, Math.floor(Math.min(512 / backingCanvas.width, 512 / backingCanvas.height)))
    canvas.width = backingCanvas.width * scale
    canvas.height = backingCanvas.height * scale
    context.imageSmoothingEnabled = false
    context.clearRect(0, 0, canvas.width, canvas.height)
    context.drawImage(backingCanvas, 0, 0, canvas.width, canvas.height)
    context.strokeStyle = "rgba(128,128,128,0.35)"
    context.lineWidth = 1
    for (let x = 0; x <= backingCanvas.width; x += 1) {
      context.beginPath()
      context.moveTo(x * scale + 0.5, 0)
      context.lineTo(x * scale + 0.5, canvas.height)
      context.stroke()
    }
    for (let y = 0; y <= backingCanvas.height; y += 1) {
      context.beginPath()
      context.moveTo(0, y * scale + 0.5)
      context.lineTo(canvas.width, y * scale + 0.5)
      context.stroke()
    }
  }
  const resizeBacking = (width, height) => {
    const previous = document.createElement("canvas")
    previous.width = backingCanvas.width
    previous.height = backingCanvas.height
    previous.getContext("2d").drawImage(backingCanvas, 0, 0)
    backingCanvas.width = width
    backingCanvas.height = height
    backingContext.clearRect(0, 0, width, height)
    backingContext.drawImage(previous, 0, 0)
    syncSizeInputs()
    redraw()
  }
  const drawAt = (event) => {
    const bounds = canvas.getBoundingClientRect()
    const x = Math.floor((event.clientX - bounds.left) / scale)
    const y = Math.floor((event.clientY - bounds.top) / scale)
    if (x < 0 || y < 0 || x >= backingCanvas.width || y >= backingCanvas.height) {
      return
    }
    backingContext.clearRect(x, y, 1, 1)
    if (currentTool() === "pen") {
      backingContext.fillStyle = colorInput.value
      backingContext.fillRect(x, y, 1, 1)
    }
    redraw()
  }

  resizeBacking(clampDimension(widthInput.value), clampDimension(heightInput.value))

  input.addEventListener("change", async () => {
    const file = input.files?.[0]
    if (!file) {
      return
    }
    const image = await readImageData(file)
    backingCanvas.width = image.width
    backingCanvas.height = image.height
    backingContext.putImageData(image.imageData, 0, 0)
    syncSizeInputs()
    redraw()
  })
  newButton.addEventListener("click", () =>
    resizeBacking(
      clampDimension(widthInput.value),
      clampDimension(heightInput.value),
    ))
  canvas.addEventListener("pointerdown", (event) => {
    painting = true
    canvas.setPointerCapture(event.pointerId)
    drawAt(event)
  })
  canvas.addEventListener("pointermove", (event) => {
    if (painting) {
      drawAt(event)
    }
  })
  canvas.addEventListener("pointerup", (event) => {
    painting = false
    canvas.releasePointerCapture(event.pointerId)
  })
  canvas.addEventListener("pointercancel", () => {
    painting = false
  })
  downloadButton.addEventListener("click", async () => {
    const blob = await canvasToPngBlob(backingCanvas)
    const url = URL.createObjectURL(blob)
    const anchor = document.createElement("a")
    anchor.href = url
    anchor.download = "sprite.png"
    anchor.click()
    URL.revokeObjectURL(url)
  })
}

const setupWebTool = () => {
  const input = document.getElementById("tileset-input")
  const runButton = document.getElementById("tileset-run")
  const downloadButton = document.getElementById("tileset-download")
  const logElement = document.getElementById("tileset-log")

  if (!input || !runButton || !downloadButton || !logElement) {
    return
  }

  let zipBlob = null

  const log = (message) => {
    logElement.textContent += `${message}\n`
  }

  const getMode = () => {
    const selected = document.querySelector("input[name='mode']:checked")
    return selected?.value ?? "compose"
  }

  runButton.addEventListener("click", async () => {
    logElement.textContent = ""
    zipBlob = null
    downloadButton.disabled = true

    if (!input.files || input.files.length === 0) {
      log("Pick a directory first.")
      return
    }

    runButton.disabled = true

    try {
      const files = buildFileMap(input.files)
      const mode = getMode()
      log(`Loaded ${files.size} files from browser directory picker.`)
      log(`Mode: ${mode}`)

      const outputFiles = mode === "compose"
        ? await composeSmallTileset(files, log)
        : await decomposeSmallTileset(files)

      zipBlob = await createZipBlob(outputFiles)
      downloadButton.disabled = false
      log(`Generated ${outputFiles.size} output files.`)
      log("Ready to download zip.")
    } catch (error) {
      log(`Error: ${error instanceof Error ? error.message : String(error)}`)
    } finally {
      runButton.disabled = false
    }
  })

  downloadButton.addEventListener("click", () => {
    if (!zipBlob) {
      return
    }

    const mode = getMode()
    const url = URL.createObjectURL(zipBlob)
    const anchor = document.createElement("a")
    anchor.href = url
    anchor.download = `tileset-${mode}-output.zip`
    anchor.click()
    URL.revokeObjectURL(url)
  })
}

setupWebTool()
setupSpriteEditor()
