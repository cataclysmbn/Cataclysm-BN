import { slugify } from "lume/cms/core/utils/string.ts"
import { getLangPattern } from "./languages.ts"

/**
 * Assigns default `id` to files based on their path excluding the language code.
 * e.g `/ko/foo/bar.md` will have `id` of `foo/bar`
 */
export const autoId = (options: { languages: string[] }) => {
  const langPattern = getLangPattern(options.languages)
  return (site: Lume.Site) => {
    site.preprocess([".md"], (files) =>
      files.forEach((file) => {
        if (!file.data.id) file.data.id = slugify(file.src.path).replace(langPattern, "")
      }))
  }
}
