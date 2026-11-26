# 언어별 스타일 가이드

> [!NOTE]
>
> #### 이 절은 템플릿입니다.
>
> 이 페이지는 언어별 참고 사항을 기록하기 위한 것이므로, 이 절 자체를 번역할 필요는 없습니다. 대신 이
> 페이지를 템플릿으로 사용하세요. [언어별 작성 스타일 가이드의 예시는 여기에서 확인할 수 있습니다](#해당-언어의-작성-스타일-가이드).

## 언어별 스타일 가이드를 만드는 방법

1. [언어 코드를 확인합니다](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes).
2. `/docs/{your-language-code}/i18n/explanation/style.md`를 만듭니다.
3. 언어가 아직 설정되지 않았다면 `docs/plugins/languages.ts` 파일에 언어 코드를 추가합니다. 예:

```diff
-export const languages = ["en", "ko", "ja", "ru", "de"]
+export const languages = ["en", "ko", "ja", "ru", "de", "fr"]
```

이 예에서는 프랑스어가 문서 사이트에서 사용할 수 있게 됩니다.

## 해당 언어의 작성 스타일 가이드

이 파일에 무엇을 어떻게 작성할지에 대한 제한은 없습니다. 예를 들면 다음과 같은 내용을 작성할 수 있습니다:

- 다른 번역자가 읽을 수 있는 언어별 참고 사항
- 외부 자료나 도구 링크

참고로 기존 스타일 가이드를 확인할 수 있습니다:

- [Deutsch](../../../de/i18n/explanation/style.md)
- [Русский](../../../ru/i18n/explanation/style.md)
