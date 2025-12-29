import { join } from "node:path"
import { fileURLToPath } from "node:url"

import starlight from "@astrojs/starlight"
import { defineConfig } from "astro/config"
import starlightLinksValidator from "starlight-links-validator"
import { loadEnv } from "vite"

import { fixRelativeLinks } from "./remark.js"

const envPath = join(fileURLToPath(import.meta.url), "..")
const env = loadEnv("", envPath, "CUSTOM")

const { CUSTOM_SITE_URL, CUSTOM_REPO_URL } = env

const site = CUSTOM_SITE_URL || "https://docs.cataclysmbn.org"
const github = CUSTOM_REPO_URL || "https://github.com/cataclysmbnteam/Cataclysm-BN"
const itemGuide = "https://cbn-guide.mythoscraft.org"
const discord = "https://discord.gg/XW7XhXuZ89"

const docModes = (dir: string) => [
  { label: "tutorial", autogenerate: { directory: `${dir}/tutorial` } },
  { label: "guides", autogenerate: { directory: `${dir}/guides` } },
  { label: "reference", autogenerate: { directory: `${dir}/reference` } },
  { label: "explanation", autogenerate: { directory: `${dir}/explanation` } },
]

console.log({ ...env, site, github })

export default defineConfig({
  site,
  redirects: { "/": `./en/` },
  markdown: {
    remarkPlugins: [fixRelativeLinks],
  },
  integrations: [
    starlightLinksValidator(),
    starlight({
      title: "Cataclysm: Bright Nights",
      defaultLocale: "en",
      locales: {
        en: { label: "English" },
        de: { label: "Deutsch", lang: "de-DE" },
        ko: { label: "한국어", lang: "ko-KR" },
        ru: { label: "Русский", lang: "ru-RU" },
      },
      logo: { src: "./src/assets/icon-round.svg" },
      social: { github, discord },
      /* https://starlight.astro.build/guides/css-and-tailwind/#color-theme-editor */
      customCss: [
        "./src/styles/theme.css",
        "./src/styles/capitalize.css",
      ],
      editLink: { baseUrl: `${github}/edit/main/doc` },
      lastUpdated: true,
      navbar: {
        game: {
          label: "Play",
          link: "/game/new_player_guide",
          translations: { "ko-KR": "게임 가이드" },
          items: [{ label: "Game Guides", autogenerate: { directory: "game" } }],
        },
        mod: {
          label: "Mod",
          link: "/mod/json/tutorial/modding",
          translations: { "ko-KR": "모딩" },
          items: [
            { label: "JSON", items: docModes("mod/json") },
            { label: "Lua", items: docModes("mod/lua") },
          ],
        },
        dev: {
          label: "Develop",
          link: "/dev/guides/building/cmake",
          translations: { "ko-KR": "게임 개발" },
          items: docModes("dev"),
        },
        "i18n": {
          label: "I18n",
          link: "/i18n/tutorial/transifex",
          translations: { "ko-KR": "번역" },
          items: docModes("i18n"),
        },
        contribute: {
          label: "Contribute",
          link: "/contribute/contributing",
          translations: { "ko-KR": "기여하기" },
          items: [
            { label: "Contributing", autogenerate: { directory: "contribute" } },
          ],
        },
      },
    }),
  ],
})
