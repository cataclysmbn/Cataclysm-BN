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

### Supported JSON values

- Effect names
- Item action names
- Item category names
- Activity verbs
- Gate action messages
- Spell names and descriptions
- Terrain/furniture descriptions
- Monster melee attack messages
- Morale effect descriptions
- Mutation names/descriptions
- NPC class names/descriptions
- Tool quality names
- Score descriptions
- Skill names/descriptions
- Bionic names/descriptions
- Terrain bash sound descriptions
- Trap-vehicle collision sound descriptions
- Vehicle part names/descriptions
- Skill display type names
- NPC dialogue u_buy_monster unique names
- Spell messages and monster spell messages
- Martial art names and descriptions
- Mission names and descriptions
- Fault names and descriptions
- Plant names in item seed data
- Transform use action messages and menu text
- Template NPC names and name suffixes
- NPC talk response text
- Relic name overrides
- Relic recharge messages
- Speech text
- Tutorial messages
- Vitamin names
- Recipe blueprint names
- Recipe group recipe descriptions
- Item names (plural supported) and descriptions
- Recipe descriptions
- Inscribe use action verbs/gerunds
- Monster names (plural supported) and descriptions
- Snippets
- Bodypart names
- Keybinding action names
- Field level names

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
