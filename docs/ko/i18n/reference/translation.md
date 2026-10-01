# 번역 API

Cataclysm: BN 은 번역된 텍스트를 표시하기 위해 [GNU gettext][gettext]와 유사하게 작동하는 커스텀
런타임 라이브러리를 사용합니다.

`gettext`를 사용하려면 두 가지 작업이 필요합니다:

- 소스 코드에서 번역해야 하는 문자열을 표시하기.
- 런타임 중에 [번역 함수](#번역-함수) 불러오기.

번역 가능한 문자열을 표시하면 해당 문자열을 자동으로 추출할 수 있습니다. 이 프로세스는 소스 코드에
표시된 원본 문자열(보통 영어로 되어 있음)을 번역된 문자열에 매핑하는 파일을 생성합니다. 이러한
매핑은 런타임 중에 번역 함수에 의해 사용됩니다.

참고로 원본 문자열이 번역을 요청하는 데 사용되는 식별자 역할을 하므로 추출된 문자열만 번역할 수
있습니다. 번역 함수가 번역을 찾을 수 없는 경우 원래 문자열을 반환합니다.

## 번역 함수

번역할 문자열을 표시하고 런타임 중에 해당 번역을 얻으려면 다음 함수 및 클래스 중 하나를 사용해야
합니다.

이러한 함수 중 하나에 사용되는 _리터럴_ 문자열(따옴표로 감싼 문자열)은 자동으로 추출됩니다. 리터럴이
아닌 문자열은 런타임 중에 여전히 번역되지만 추출되지는 않습니다.

### `_()`

이 함수는 간단한 문자열 등에 사용하기에 적합하며, 예를 들면 다음과 같습니다:

```cpp
const char *translated = _( "text marked for translation" )
```

직접 작동하기도 합니다:

```cpp
add_msg( _( "You drop the %s." ), the_item_name );
```

JSON 파일의 문자열은 `lang/extract_json_strings.py` 스크립트에 의해 추출되며, `_()`를 사용하여
런타임 중에 번역할 수 있습니다. JSON 문자열에 대한 번역 컨텍스트가 필요한 경우, 아래에 설명된
`class translation`을 대신 사용할 수 있습니다.

### `pgettext()`

이 함수는 원래 문자열의 의미만으로는 모호한 경우에 유용합니다. 예를 들어 "blue"라는 단어는 색상이나
감정을 의미할 수 있습니다.

또한 `pgettext`는 번역 가능한 문자열 외에도, 번역가에게 제공되지만 번역된 문자열 자체의 일부가 아닌
컨텍스트를 받습니다. 이하 함수의 첫 번째 매개 변수는 컨텍스트이고 두 번째 매개 변수는 번역할
문자열입니다:

```cpp
const char *translated = pgettext( "The color", "blue" );
```

### `vgettext()`

일부 언어에는 복수형에 대한 복잡한 규칙이 있습니다. `vgettext`는 이러한 복수형을 올바르게 번역하는
데 사용할 수 있습니다. 이하의 예시에서 첫 번째 매개변수는 번역되지 않은 단수형 문자열이고, 두 번째
매개변수는 번역되지 않은 복수형 문자열이며, 세 번째 매개변수는 런타임에 처음 두 개 중 어느 것을
사용할지 결정하는 데 사용됩니다:

```cpp
const char *translated = vgettext( "%d zombie", "%d zombies", num_of_zombies );
```

### `vpgettext()`

`vgettext`와 동일하지만 번역 컨텍스트를 지정할 수 있습니다.

```cpp
const char *translated = vpgettext( "water source, not time of year", "%d spring", "%d springs", num_of_springs );
```

## `translation`

번역 컨텍스트와 함께 번역을 위해 문자열을 저장하고 싶을 때가 있습니다; 때로는 번역이 필요 없거나
복수형인 문자열을 저장하고 싶을 수도 있습니다. `translations.h|cpp`의 `class translation`은 이러한
기능을 단일 래퍼에 제공합니다:

```cpp
const translation text = to_translation( "Context", "Text" );
```

```cpp
const translation text = to_translation( "Text without context" );
```

```cpp
const translation text = pl_translation( "Singular", "Plural" );
```

```cpp
const translation text = pl_translation( "Context", "Singular", "Plural" );
```

```cpp
const translation text = no_translation( "This string will not be translated" );
```

그런 다음 다음 코드를 사용하여 문자열을 번역/검색할 수 있습니다.

```cpp
const std::string translated = text.translated();
```

```cpp
// 숫자 2에 해당하는 텍스트의 복수형을 번역합니다.
const std::string translated = text.translated( 2 );
```

`class translation`은 JSON에서 읽을 수도 있습니다. `translation::deserialize()` 메서드는 `JsonIn`
객체에서 역직렬화를 처리하므로 적절한 JSON 함수를 사용하여 JSON에서 번역을 읽을 수 있습니다.\
JSON 구문은 다음과 같습니다:

```json
"name": "bar"
```

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

또는

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

위 코드에서 `"ctxt"`와 `"str_pl"`는 모두 선택 사항이지만, `"str_sp"`는 동일한 문자열로 `"str"`와
`"str_pl"`을 지정하는 것과 같습니다. 또한 `"str_pl"` 및 `"str_sp"`는 번역 객체가 `plural_tag` 또는
`pl_translation()`을 사용하여 생성되거나 `make_plural()`을 사용하여 변환된 경우에만 읽혀집니다.
다음은 예제입니다:

```cpp
translation name{ translation::plural_tag() };
jsobj.read( "name", name );
```

"str_pl"`또는`"str_sp"`를 지정하지 않으면 기본적으로 복수형이 단수형 + "s"로 지정됩니다.

아래와 같이 작성하여 번역가를 위한 코멘트를 추가할 수도 있습니다(입력 순서는 중요하지 않음):

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

현재 JSON 구문은 아래에 나열된 일부 JSON 값에 대해서만 지원된다는 점에 유의하세요. 다른 json
문자열에 이 형식을 사용하려면 `translations.h|cpp`를 참조하여 해당 코드를 마이그레이션하세요. 그런
다음 `update_pot.sh`를 테스트하여 번역을 위해 문자열이 올바르게 추출되는지 확인하고
[단위 테스트](https://ko.wikipedia.org/wiki/%EC%9C%A0%EB%8B%9B_%ED%85%8C%EC%8A%A4%ED%8A%B8)를
실행하여 `translation` 클래스에서 보고된 텍스트 스타일 문제를 수정할 수도 있습니다.

<a id="supported-json-values"></a>

### 지원되는 JSON 값

- 효과 이름
- 아이템 동작 이름
- 아이템 카테고리 이름
- 활동을 나타내는 동사
- 출입문 동작 메시지
- 주문 이름과 설명
- 지형/가구 설명
- 몬스터 근접 공격 메시지
- 사기 효과 설명
- 돌연변이 이름과 설명
- NPC 클래스 이름과 설명
- 도구 품질 이름
- 점수 설명
- 스킬 이름과 설명
- 바이오닉 이름과 설명
- 지형 강타 소리 설명
- 함정과 차량의 충돌 소리 설명
- 차량 부품 이름과 설명
- 스킬 표시 유형 이름
- NPC 대화의 `u_buy_monster` 고유 이름
- 주문 메시지와 몬스터 주문 메시지
- 무술 이름과 설명
- 임무 이름과 설명
- 결함 이름과 설명
- 아이템 씨앗 데이터의 식물 이름
- 변환 사용 동작의 메시지와 메뉴 텍스트
- 템플릿 NPC 이름과 이름 접미사
- NPC 대화 응답 텍스트
- 유물 이름 재정의
- 유물 재충전 메시지
- 발언 텍스트
- 튜토리얼 메시지
- 비타민 이름
- 제작법 설계도 이름
- 제작법 그룹의 제작법 설명
- 아이템 이름(복수형 지원)과 설명
- 제작법 설명
- 새기기 사용 동작의 동사/동명사
- 몬스터 이름(복수형 지원)과 설명
- 문구 모음
- 신체 부위 이름
- 키 바인딩 동작 이름
- 필드 단계 이름

### Lua

[4가지 번역 함수가 Lua 코드에 공개되어 있습니다.](../../mod/lua/tutorial/modding.md#translation-functions).

### 추천

Cataclysm: BN 에서 `itype` 및 `mtype`과 같은 일부 클래스는 `nname`이라는 번역 함수를 위한
래퍼(wrapper)를 제공합니다.

빈 문자열이 번역을 위해 표시되면 항상 빈 문자열이 아닌 디버그 정보로 번역됩니다. 대부분의 경우,
문자열은 절대 비어 있지 않으므로 번역용으로 표시해도 항상 안전합니다. 그러나 비어 있을 수 있고 번역
후에도 비어 _있어야 하는_ 문자열을 처리할 때는, 문자열이 비어 있는지 검사하고 비어 있지 않은
경우에만 번역 함수에 전달해야 합니다.

오류 및 디버그 메시지는 번역을 위해 표시해서는 안 됩니다. 이러한 메시지가 나타나면 플레이어는
게임에서 출력되는 그대로 _정확하게_ 보고해야 합니다.

자세한 내용은 [gettext 매뉴얼][manual]을 참조하세요.

[gettext]: https://www.gnu.org/software/gettext/
[manual]: https://www.gnu.org/software/gettext/manual/index.html
