# Language Specific Style Guide

> [!NOTE]
>
> #### This section is a template.
>
> As this page is for language-specific notes, it shouldn't be translated. Instead, it's used as a
> template. [Check here for examples.](#writing-style-guide-for-your-language)

## How to write one for your language

1. [Find your language code.](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes)
2. Create `/docs/{your-language-code}/i18n/explanation/style.md`.
3. If it doesn't, add your language to `doc/astro.config.ts` file, for example:

```diff
diff --git a/doc/astro.config.ts b/doc/astro.config.ts
index da91c2e0014..3dafa248a06 100644
--- a/doc/astro.config.ts
+++ b/doc/astro.config.ts
@@ -41,6 +41,7 @@ export default defineConfig({
       locales: {
         en: { label: "English" },
         ko: { label: "한국어", lang: "ko-KR" },
+        ru: { label: "Русский", lang: "ru-RU" },
       },
       logo: { src: "./src/assets/icon-round.svg" },
       social: { github, discord },
```

In this example, it will allow russian language to be availble in language selector. Check
[starlight](https://starlight.astro.build/guides/i18n/) for details.

## Writing style guide for your language

There's no restrictions on how or what to write in this file. For example, you could:

- write language specific notes for other translaotrs to read
- link to external resources or tools

You could check out existing style guides for reference:

- [Deutsch](../../../de/i18n/explanation/style.md)
- [Russian](../../../ru/i18n/explanation/style.md)
