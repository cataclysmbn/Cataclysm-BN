import { SEPARATOR } from "jsr:@std/path@^1.1.1/constants"

export const languages = ["en", "ko", "ja", "ru", "de"]
export const getLangPattern = (languages: string[]) =>
  new RegExp(`^\\${SEPARATOR}(${languages.join("|")})(?=\\${SEPARATOR}|$)`)
