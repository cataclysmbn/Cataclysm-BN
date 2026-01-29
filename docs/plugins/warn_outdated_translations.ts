import { getLangPattern } from "./languages.ts"

type options_t = {
  languages: string[]
  source_language?: string
}

const get_path_without_language = (
  url: string,
  lang_pattern: RegExp,
) => {
  const match = url.match(lang_pattern)
  return match ? url.replace(match[0], "") : url
}

const get_language_from_url = (
  url: string,
  lang_pattern: RegExp,
  source_language: string,
) => {
  const match = url.match(lang_pattern)
  return match?.[1] ?? source_language
}

const get_last_modified = (page: Lume.Page) => {
  const values = [
    page.data.updated as Date | string | undefined,
    page.data.date as Date | string | undefined,
    page.data.created as Date | string | undefined,
    (page.src as { date?: Date | string } | undefined)?.date,
  ]

  for (const value of values) {
    if (value instanceof Date) {
      return value
    }
    if (typeof value === "string") {
      const parsed = new Date(value)
      if (!Number.isNaN(parsed.getTime())) {
        return parsed
      }
    }
  }

  return undefined
}

export const warn_outdated_translations = (options: options_t) => {
  const source_language = options.source_language ?? "en"
  const lang_pattern = getLangPattern(
    options.languages.filter((l) => l !== source_language),
  )

  return (site: Lume.Site) => {
    site.preprocess([".html"], (pages) => {
      if (Deno.env.get("LUME_SERVE")) {
        return
      }

      const pages_by_path = Map.groupBy(
        pages.filter((p) => p.data.url),
        (page) => get_path_without_language(page.data.url, lang_pattern),
      )

      for (const [path_without_lang, pages_for_path] of pages_by_path) {
        const source_page = pages_for_path.find((p) =>
          get_language_from_url(p.data.url, lang_pattern, source_language) ===
            source_language
        )
        if (!source_page) {
          continue
        }

        const source_last_modified = get_last_modified(source_page)
        if (!source_last_modified) {
          continue
        }

        for (const translated_page of pages_for_path) {
          const translated_lang = get_language_from_url(
            translated_page.data.url,
            lang_pattern,
            source_language,
          )
          if (translated_lang === source_language) {
            continue
          }

          const translated_last_modified = get_last_modified(translated_page)
          if (!translated_last_modified) {
            continue
          }

          if (translated_last_modified < source_last_modified) {
            // eslint-disable-next-line no-console
            console.warn(
              `[outdated-translation] ${translated_page.data.url} (${translated_lang}) is older than ${source_page.data.url} (${source_language})`,
            )
          }
        }

        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        void path_without_lang
      }
    })
  }
}
