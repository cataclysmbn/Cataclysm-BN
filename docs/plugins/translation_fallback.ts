import { SEPARATOR } from "jsr:@std/path@^1.1.1/constants"
import { getLangPattern } from "./languages.ts"

/**
 * Creates automatic redirects for missing translations to their English equivalents.
 * Uses Lume's redirects plugin by adding oldUrl data to English pages.
 * e.g., if /ko/mod/json/tutorial/modding doesn't exist, redirect to /mod/json/tutorial/modding
 */
export const translationFallback = (options: { languages: string[] }) => {
  const langPattern = getLangPattern(options.languages.filter((l) => l !== "en"))

  return (site: Lume.Site) => {
    site.preprocess([".html"], (pages) => {
      const pagesByPath = Map.groupBy(
        pages.filter((p) => p.data.url),
        (page) => {
          const url = page.data.url
          const match = url.match(langPattern)
          return match ? url.replace(match[0], "") : url
        },
      )

      let redirectCount = 0
      for (const [pathWithoutLang, pagesForPath] of pagesByPath) {
        const existingLangs = new Set(
          pagesForPath.map((page) => (page.data.url.match(langPattern))?.[1] ?? "en"),
        )

        if (!existingLangs.has("en") && pathWithoutLang !== "/") continue

        const missingLangPaths = options.languages
          .filter((lang) => lang !== "en" && !existingLangs.has(lang))
          .map((lang) => `${SEPARATOR}${lang}${pathWithoutLang}`)

        if (missingLangPaths.length == 0) continue
        const enPage = pagesForPath.find((p) => !p.data.url.match(langPattern))
        if (!enPage) continue
        enPage.data.oldUrl = [...(enPage.data.oldUrl ?? []), ...missingLangPaths]
        redirectCount += missingLangPaths.length
        // console.log(
        //   `[translation-fallback] Adding redirects for ${pathWithoutLang}:`,
        //   missingLangPaths,
        // )
      }
      //   console.log(`[translation-fallback] Added ${redirectCount} redirects to pages`)
    })
  }
}
