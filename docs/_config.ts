import lume from "lume/mod.ts"
import wiki from "wiki/mod.ts"

import date from "lume/plugins/date.ts"
import redirects from "lume/plugins/redirects.ts"
import relativeUrls from "lume/plugins/relative_urls.ts"

import { ko } from "npm:date-fns/locale/ko"

import { autoId } from "./plugins/auto_id.ts"
import { translationFallback } from "./plugins/translation_fallback.ts"
import "./plugins/prism.ts"
import { languages } from "./plugins/languages.ts"

const site = lume()

site
  .ignore("README.md")
  .use(relativeUrls())
  .use(autoId({ languages }))
  .use(wiki({ languages }))
  .use(translationFallback({ languages }))
  .use(redirects())
  .use(date({
    locales: { ko },
    formats: { HUMAN_DATE: "yyyy-MM-dd", HUMAN_DATETIME: "yyyy-MM-dd ppp" },
  }))
  .copy("redirect.js")

export default site
