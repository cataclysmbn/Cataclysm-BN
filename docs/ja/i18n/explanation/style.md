# 言語別スタイルガイド

> [!NOTE]
>
> #### 本セクションはテンプレートです。
>
> このページは言語固有の注記を記載するためのものであるため、このセクション自体を翻訳すべきではありません。代わりに、このページはテンプレートとして使用されます。
> [ご自身の言語の記述スタイルガイド例については、こちらを確認してください](#writing-style-guide-for-your-language)。

## ご自身の言語用のスタイルガイドを作成する方法

1. [ご自身の言語コードを見つけてください](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes)。
2. `/docs/{your-language-code}/i18n/explanation/style.md`を作成してください。
3. 言語がまだ設定されていない場合は、`doc/astro.config.ts` ファイルにご自身の言
   語を追加してください。例：

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

この例では、ロシア語が言語セレクターで利用可能になります。
[starlight](https://starlight.astro.build/guides/i18n/) の詳細を確認してください。

## ご自身の言語の記述スタイルガイド

このファイルに何をどのように記述するかについて、制限はありません。たとえば、以下のような記述が可能です。

- 他の翻訳者が読むための、言語固有の注記を記述する。
- 外部リソースやツールへのリンクを貼る。

参考として、既存のスタイルガイドを確認できます:

- [Deutsch](../../../de/i18n/explanation/style.md)
- [Russian](../../../ru/i18n/explanation/style.md)
