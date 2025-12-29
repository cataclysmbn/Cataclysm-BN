import { slugify } from "lume/cms/core/utils/string.ts"
import { SEPARATOR } from "jsr:@std/path@^1.1.1/constants"

/**
 * Assigns default `id` to files based on their path excluding the language code.
 * e.g `/ko/foo/bar.md` will have `id` of `foo/bar`
 */
export const autoId = (options: { languages: string[] }) => {
  const LANGUAGE_CODE = new RegExp(
    `^\\${SEPARATOR}(${options.languages.join("|")})(?=\\${SEPARATOR})`,
  )

  return (site: Lume.Site) => {
    site.preprocess([".md"], (files) =>
      files.forEach((file) => {
        if (!file.data.id) file.data.id = slugify(file.src.path).replace(LANGUAGE_CODE, "")
      }))
  }
}
