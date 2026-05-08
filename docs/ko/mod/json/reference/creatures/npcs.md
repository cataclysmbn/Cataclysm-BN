---
title: NPCs
tableOfContents:
  maxHeadingLevel: 4
---

## Writing Dialogues

TODO: NPC 템플릿을 로드할 때 사용하는 "npc" 구조 문서화

대화는 상태 기계처럼 동작합니다. 특정 토픽으로 시작하고(NPC가 말을 함), 플레이어 캐릭터가
응답하며(여러 응답 중 선택), 그 응답이 새 대화 토픽을 설정합니다. 이는 대화가 끝나거나 NPC가
적대적으로 변할 때까지 계속됩니다.

응답이 자기 자신 토픽으로 다시 전환되도록 구성해도 완전히 정상입니다.

NPC 미션은 별도의 관련 JSON 구조로 제어되며,
[the missions docs](./missions_json.md)에 문서화되어 있습니다.

다음 두 토픽은 특별합니다:

- `TALK_DONE`은 대화를 즉시 종료합니다.
- `TALK_NONE`은 직전에 이야기하던 토픽으로 돌아갑니다.

### Validating Dialogues

대화 토픽을 추적하고, 응답에서 참조한 모든 토픽이 정의되어 있는지, 그리고 정의된 모든 토픽이
응답 또는 NPC의 chat에서 참조되는지 확인하는 일은 매우 까다롭습니다. `tools/dialogue_validator.py`
파이썬 스크립트는 모든 토픽과 응답의 매핑을 양방향으로 확인해 줍니다. 다음처럼 실행합니다.

```sh
python tools/dialogue_validator.py data/json/npcs/* data/json/npcs/Backgrounds/* data/json/npcs/refugee_center/*
```

대화가 포함된 모드를 작성 중이라면 모드 대화 파일 경로를 추가하면 됩니다.

## Talk topics

각 토픽은 다음으로 구성됩니다:

1. 토픽 id (예: `TALK_ARSONIST`)
2. NPC가 말하는 동적 대사
3. NPC가 동적 대사를 말할 때 발생하는 선택적 효과 목록
4. 플레이어 캐릭터가 말할 수 있는 응답 목록
5. 플레이어나 NPC가 특정 아이템 목록을 가지고 있을 때 자동 생성되는 반복 응답 목록

JSON에서 새 토픽을 지정할 수 있습니다. 현재 시작 토픽을 직접 정의하는 것은 불가능하므로,
기본 토픽(예: `TALK_STRANGER_FRIENDLY`, `TALK_STRANGER_NEUTRAL`)이나 도달 가능한 토픽에
응답을 추가해야 합니다.

형식:

```json
{
  "type": "talk_topic",
  "id": "TALK_ARSONIST",
  "dynamic_line": "What now?",
  "responses": [
    {
      "text": "I don't know either",
      "topic": "TALK_DONE"
    }
  ],
  "replace_built_in_responses": true
}
```

### type

항상 있어야 하며 값은 항상 `"talk_topic"`이어야 합니다.

### id

토픽 id는 내장 토픽일 수도, 새 id일 수도 있습니다. 다만 _json 내에서_ 동일한 id를 가진 talk topic이
여럿 있으면 마지막 정의가 이전 정의를 덮어씁니다.

토픽 id는 문자열 배열일 수도 있습니다. 이 경우 `id` 배열의 각 원소 id에 동일한 내용을 가진 토픽이
여러 개 정의된 것처럼 로드됩니다. JSON 로딩 시 응답은 추가되며, json에 정의된 경우
`dynamic_line`과 `replace_built_in_responses` 설정은 덮어씁니다. 이를 이용하면 여러 토픽에
응답을 한 번에 추가할 수 있습니다.

이 예시는 나열된 모든 토픽에 "I'm going now!" 응답을 추가합니다.

```cpp
{
    "type": "talk_topic",
    "id": [ "TALK_ARSONIST", "TALK_STRANGER_FRIENDLY", "TALK_STRANGER_NEUTRAL" ],
    "dynamic_line": "What now?",
    "responses": [
        {
            "text": "I'm going now.",
            "topic": "TALK_DONE"
        }
    ]
}
```

### dynamic_line

`dynamic_line`은 NPC가 말하는 대사입니다. 선택 항목입니다. 정의되지 않았고 토픽 id가 내장 토픽과
같다면 내장 토픽의 `dynamic_line`이 사용됩니다. 그 외에는 NPC가 아무 말도 하지 않습니다.
자세한 내용은 아래 `dynamic_line` 장을 참고하세요.

### speaker_effect

`speaker_effect`는 NPC가 `dynamic_line`을 말한 뒤 플레이어가 어떤 응답을 선택하든 발생하는
효과 객체 또는 배열입니다. 자세한 내용은 아래 Speaker Effects 장을 참고하세요.

### response

`responses` 항목은 가능한 응답 배열입니다. 비어 있으면 안 됩니다. 각 항목은 응답 객체여야 합니다.
자세한 내용은 아래 Responses 장을 참고하세요.

### replace_built_in_responses

`replace_built_in_responses`는 해당 토픽의 내장 응답을 무시할지 정의하는 선택적 bool입니다
(기본 `false`). 내장 응답이 없으면 아무 영향이 없습니다. `true`면 내장 응답은 무시되고 현재 JSON
정의의 응답만 사용됩니다. `false`면 현재 JSON 응답과 내장 응답(있다면)을 함께 사용합니다.

---

## dynamic_line

dynamic line은 단순 문자열, 복합 객체, 또는 `dynamic_line` 항목 배열일 수 있습니다.
배열이면 NPC가 필요로 할 때마다 항목 하나가 무작위로 선택되며 각 항목 확률은 동일합니다.

예시:

```json
"dynamic_line": [
    "generic text",
    {
        "npc_female": [ "text1", "text2", "text3" ],
        "npc_male": { "u_female": "text a", "u_male": "text b" }
    }
]
```

복합 `dynamic_line`은 보통 여러 `dynamic_line` 항목과, 어떤 항목을 사용할지 결정하는 조건을
포함합니다. dynamic line이 중첩되지 않은 경우 아래 항목 순서대로 처리됩니다.

모든 경우에서 `npc_`는 NPC를, `u_`는 플레이어를 의미합니다. 선택 라인은 정의하지 않아도 되지만
NPC는 항상 말할 내용이 있는 것이 좋습니다. 항목은 항상 `dynamic_line`으로 파싱되며 중첩될 수 있습니다.

#### Several lines joined together

dynamic line을 dynamic line 목록으로 두고, 목록의 모든 항목을 표시합니다.
목록 내 각 dynamic line은 일반 규칙대로 처리됩니다.

```json
{
  "and": [
    {
      "npc_male": true,
      "yes": "I'm a man.",
      "no": "I'm a woman."
    },
    "  ",
    {
      "u_female": true,
      "no": "You're a man.",
      "yes": "You're a woman."
    }
  ]
}
```

#### A line to be translated with gender context

NPC, 플레이어, 또는 둘 모두에 대해 성별 컨텍스트를 부여해 번역을 돕는 라인입니다.
성별 구분이 중요한 언어에서 유용합니다. 예:

```json
{
  "gendered_line": "Thank you.",
  "relevant_genders": ["npc"]
}
```

(예: 포르투갈어에서는 화자 성별에 따라 "Thank you"가 달라짐)

`"relevant_genders"` 목록의 유효 값은 `"npc"`, `"u"`입니다.

#### A randomly selected hint

dynamic line이 hints snippets에서 무작위로 선택됩니다.

```json
{
  "give_hint": true
}
```

#### Based on a previously generated reason

이전에 effect가 생성해 둔 reason에서 dynamic line을 선택합니다. reason은 사용 후 지워집니다.
`"has_reason"` 조건으로 사용 가능성을 제어해야 합니다.

```json
{
  "has_reason": { "use_reason": true },
  "no": "What is it, boss?"
}
```

#### Based on any Dialogue condition

단일 dialogue condition의 참/거짓에 따라 dynamic line을 선택합니다.
Dialogue conditions는 `"and"`, `"or"`, `"not"`으로 체인할 수 없습니다.
조건이 참이면 `"yes"`, 거짓이면 `"no"` 응답이 선택됩니다.
`"yes"`, `"no"`는 모두 선택 항목입니다. 단순 문자열 조건은 `"true"`를 뒤따라 dynamic line
딕셔너리 필드로 쓰거나, 참일 때 사용할 응답을 바로 붙여 `"yes"`를 생략할 수 있습니다.

```json
{
    "npc_need": "fatigue",
    "level": "TIRED",
    "no": "Just few minutes more...",
    "yes": "Make it quick, I want to go back to sleep."
}
{
    "npc_aim_rule": "AIM_PRECISE",
    "no": "*will not bother to aim at all.",
    "yes": "*will take time and aim carefully."
}
{
    "u_has_item": "india_pale_ale",
    "yes": "<noticedbooze>",
    "no": "<neutralchitchat>"
}
{
    "days_since_cataclysm": 30,
    "yes": "Now, we've got a moment, I was just thinking it's been a month or so since... since all this, how are you coping with it all?",
    "no": "<neutralchitchat>"
}
{
    "is_day": "Sure is bright out.",
    "no": {
        "u_male": true,
        "yes": "Want a beer?",
        "no": "Want a cocktail?"
    }
}
```

---

## Speaker Effects

`speaker_effect` 항목은 NPC가 `dynamic_line`을 말한 직후, 플레이어 응답 전에,
그리고 플레이어 응답과 무관하게 발생하는 대화 효과를 담습니다.
각 효과에는 선택적 조건을 둘 수 있으며 조건이 참일 때만 적용됩니다.
또한 각 `speaker_effect`에는 선택적 `sentinel`을 둘 수 있고,
이 경우 해당 효과는 한 번만 실행됩니다.

형식:

```json
"speaker_effect": {
  "sentinel": "...",
  "condition": "...",
  "effect": "..."
}
```

또는:

```json
"speaker_effect": [
  {
    "sentinel": "...",
    "condition": "...",
    "effect": "..."
  },
  {
    "sentinel": "...",
    "condition": "...",
    "effect": "..."
  }
]
```

`sentinel`은 임의 문자열이 될 수 있지만, 유일성 범위는 각 `TALK_TOPIC` 내부입니다.
같은 `TALK_TOPIC` 안에 `speaker_effect`가 여러 개 있으면 서로 다른 sentinel을 써야 합니다.
필수는 아니지만, 대화가 `TALK_TOPIC`으로 돌아올 때마다 `speaker_effect`가 다시 실행되므로,
같은 효과가 의도치 않게 반복되지 않도록 strongly 권장됩니다.

`effect`는 아래 설명된 모든 유효 effect를 사용할 수 있습니다.
일반 객체 필드처럼 단순 문자열, 객체, 문자열/객체 배열 모두 가능합니다.

선택 항목인 `condition`도 아래 설명된 모든 유효 condition을 사용할 수 있습니다.
`condition`이 있으면 조건이 참일 때만 `effect`가 실행됩니다.

Speaker effect는 응답마다 effect 변수를 복잡하게 넣지 않고,
플레이어가 NPC와 대화했는지 상태 변수를 설정할 때 유용합니다.
또한 sentinel과 함께 사용해 플레이어가 특정 대사를 처음 들었을 때
`mapgen_update` effect를 한 번만 실행하는 데도 사용할 수 있습니다.

---

## Responses

응답은 최소한 사용자에게 표시되는 텍스트(게임 로직상 의미 없음)와,
대화가 전환될 토픽을 포함합니다.
또한 NPC를 속이기/설득하기/위협하기 위한 trial 객체를 가질 수 있으며(아래 상세),
trial 성공/실패에 따라 서로 다른 결과를 지정할 수 있습니다.

형식:

```json
{
  "text": "I, the player, say to you...",
  "condition": "...something...",
  "trial": {
    "type": "PERSUADE",
    "difficulty": 10
  },
  "success": {
    "topic": "TALK_DONE",
    "effect": "...",
    "opinion": {
      "trust": 0,
      "fear": 0,
      "value": 0,
      "anger": 0,
      "owed": 0,
      "favors": 0
    }
  },
  "failure": {
    "topic": "TALK_DONE"
  }
}
```

또는 짧은 형식:

```json
{
  "text": "I, the player, say to you...",
  "effect": "...",
  "topic": "TALK_WHATEVER"
}
```

짧은 형식은 다음과 동등합니다(무조건 토픽 전환, `effect`는 선택):

```json
{
  "text": "I, the player, say to you...",
  "trial": {
    "type": "NONE"
  },
  "success": {
    "effect": "...",
    "topic": "TALK_WHATEVER"
  }
}
```

선택적 bool 키 `"switch"`, `"default"`의 기본값은 false입니다.
`"switch": true`, `"default": false`, 조건 유효를 만족하는 응답 중 첫 번째만 표시되고,
다른 `"switch": true` 응답은 표시되지 않습니다.
그런 응답이 하나도 없으면 `"switch": true`, `"default": true`인 응답들이 표시됩니다.
두 경우 모두 `"switch": false`인 응답은(`"default": true` 여부와 무관하게)
조건만 만족하면 표시됩니다.

#### switch and default Example

```json
"responses": [
  { "text": "You know what, never mind.", "topic": "TALK_NONE" },
  { "text": "How does 5 Ben Franklins sound?",
    "topic": "TALK_BIG_BRIBE", "condition": { "u_has_items": { "item": "100_usd", "count": 5 } }, "switch": true },
   { "text": "I could give you a big Grant.",
    "topic": "TALK_BRIBE", "condition": { "u_has_item": "50_usd" }, "switch": true },
  { "text": "Lincoln liberated the slaves, what can he do for me?",
    "topic": "TALK_TINY_BRIBE", "condition": { "u_has_item": "5_usd" }, "switch": true, "default": true },
  { "text": "Maybe we can work something else out?", "topic": "TALK_BRIBE_OTHER",
    "switch": true, "default": true },
  { "text": "Gotta go!", "topic": "TALK_DONE" }
]
```

플레이어는 항상 이전 토픽으로 돌아가거나 대화를 종료할 수 있으며,
자금이 있으면 $500/$50/$5 뇌물을 제시하는 옵션이 나타납니다.
최소 $50이 없으면 다른 형태의 뇌물 옵션도 표시됩니다.

### truefalsetext

조건 참/거짓에 따라 응답 텍스트를 다르게 보여주되, 두 줄 모두 같은 trial을 사용합니다.
`condition`, `true`, `false`는 모두 필수입니다.

```json
{
  "truefalsetext": {
    "condition": { "u_has_item": "FMCNote" },
    "true": "I may have the money, I'm not giving you any.",
    "false": "I don't have that money."
  },
  "topic": "TALK_WONT_PAY"
}
```

### text

사용자에게 표시되며, 그 외 의미는 없습니다.

### trial

선택 항목이며, 정의하지 않으면 `"NONE"`이 사용됩니다.
정의할 경우 `"NONE"`, `"LIE"`, `"PERSUADE"`, `"INTIMIDATE"`, `"CONDITION"` 중 하나입니다.
`"NONE"`이면 `failure` 객체는 읽지 않으며, 그 외 타입에서는 `failure`가 필수입니다.

`difficulty`는 타입이 `"NONE"`/`"CONDITION"`이 아닐 때만 필요하며 성공 확률(%)을 지정합니다
(돌연변이 등으로 수정될 수 있음). 값이 높을수록 통과하기 쉽습니다.

선택적 `mod` 배열은 다음 modifier를 사용할 수 있으며,
각 modifier 값에 NPC가 플레이어에게 갖는 의견/성향 값을 곱해 difficulty에 더합니다:
`"ANGER"`, `"FEAR"`, `"TRUST"`, `"VALUE"`, `"AGRESSION"`, `"ALTRUISM"`, `"BRAVERY"`, `"COLLECTOR"`.
특수 `"POS_FEAR"`는 NPC의 공포가 0 미만이면 0으로 취급합니다.
특수 `"TOTAL"`은 앞선 modifier 합을 다시 자신의 값으로 곱하며 owed 값을 설정할 때 사용됩니다.

`"CONDITION"` trial은 `difficulty` 대신 필수 `condition`을 사용합니다.
조건이 참이면 `success`, 거짓이면 `failure`가 선택됩니다.

### success and failure

두 객체 구조는 동일합니다. `topic`은 대화가 전환될 토픽을 지정합니다.
`opinion`은 선택이며, 있으면 NPC 의견이 어떻게 바뀌는지 정의합니다.
값은 NPC 의견에 _가산_되며, 각 필드는 선택이고 기본값 0입니다.
`effect`는 응답 선택 후 실행되는 함수입니다(아래 참고).

NPC의 의견은 NPC 상호작용 여러 측면에 영향을 줍니다:

- trust가 높을수록 거짓말/설득이 쉬워지고 대체로 긍정적입니다.
- fear가 높을수록 위협이 쉬워지지만 NPC가 도망가 대화하지 않을 수 있습니다.
- value가 높을수록 설득 및 명령이 쉬워지며 우호도 지표 역할을 합니다.
- anger가 높으면(대략 fear보다 20 이상, 성향에 따라 다름) NPC가 적대적으로 변합니다.
  fear/trust 조합과 성향은 초기 토픽(`"TALK_MUG"`, `"TALK_STRANGER_AGGRESSIVE"`,
  `"TALK_STRANGER_SCARED"`, `"TALK_STRANGER_WARY"`, `"TALK_STRANGER_FRIENDLY"`,
  `"TALK_STRANGER_NEUTRAL"`) 결정에도 영향을 줍니다.

실제 사용은 소스 코드에서 `"op_of_u"`를 검색해 확인하세요.

trial 실패 시 `failure`, 그 외에는 `success` 객체가 사용됩니다.

### Sample trials

```json
"trial": { "type": "PERSUADE", "difficulty": 0, "mod": [ [ "TRUST", 3 ], [ "VALUE", 3 ], [ "ANGER", -3 ] ] }
"trial": { "type": "INTIMIDATE", "difficulty": 20, "mod": [ [ "FEAR", 8 ], [ "VALUE", 2 ], [ "TRUST", 2 ], [ "BRAVERY", -2 ] ] }
"trial": { "type": "CONDITION", "condition": { "npc_has_trait": "FARMER" } }
```

`topic`은 단일 topic 객체일 수도 있습니다(여기서는 `type` 멤버 불필요):

```json
"success": {
    "topic": {
        "id": "TALK_NEXT",
        "dynamic_line": "...",
        "responses": [
        ]
    }
}
```

### condition

응답을 특정 상황에서 숨기기 위한 선택 조건입니다.
정의하지 않으면 기본값은 항상 `true`입니다.
조건을 만족하지 않으면 가능한 응답 목록에 포함되지 않습니다.
가능한 내용은 아래 Dialogue Conditions를 참고하세요.

---

## Talk Tags

Talk tag는 '<' 와 '>' 사이에 들어가는 특수 문자열로, 런타임에 동적으로 치환됩니다.
세 가지 종류가 있습니다.

### Special

하드코딩되어 동적으로 채워지는 태그들입니다.

|
|----------------------------------------------------------------| ---------------------------------------------------------------------------------------------------------------- |
| '<yrwp>' | Name of your primary weapon, with 'none' as default if none found. |
| '<mywp>' | Name of the npcs' primary weapon, with 'fists' as default if none found. |
| '<ammo>' | Name of your primary weapons' ammo, with 'BADAMMO' as default if not a gun. |
| '<current_activity>' | Npcs' current activity as a verb, with 'doing this and that' if not doing anything. |
| '<mypronoun>' | Npcs' pronoun, uppercase. eg. 'He' / 'She' |
| '<topic_item>' | Item from a repeat response. |
| '<topic_item_price>' | Price of item from a repeat response. |
| '<topic_item_my_total_price>' | Price of all items of this type from a repeat response, using the npcs' inventory. |
| '<topic_item_your_total_price>' | Price of all items of this type from a repeat response, using the players' inventory. |
| '<interval>' | The time until this npc restocks their item shop |

### Snippets

무작위로 고를 수 있는 문구 목록입니다. 예시는 talk_tags.json에 있습니다.

아래는 해당 파일의 예시입니다:

```json
{
	"type": "snippet",
	"category": "<lets_talk>",
	"//": "NPCs shout these things while approaching the avatar for the first time",
	"text": [
		"Wait up, let's talk!",
		"Hey, I <really> want to talk to you!",
		"Come on, talk to me!",
		"Hey <name_g>, let's talk!",
		"<name_g>, we <really> need to talk!",
		"Hey, we should talk, <okay>?",
		"Hey, can we talk for a bit?",
		"<name_g>!  Wait up!",
		"Wait up, <okay>?",
		"Let's talk, <name_g>!",
		"Look, <name_g><punc> let's talk!",
		"Hey, what's the rush?  Let's chat a tad."
	]
},
```

보듯이 다른 talk tag를 다시 참조할 수도 있습니다. 태그가 남지 않을 때까지 문자열을 반복 처리하기
때문입니다. 루프가 생기지 않게 주의하세요.

### Talk Variables

이는 lua를 사용하거나 대화 시스템 자체로 변수를 설정해 정의합니다.
lua에서는 호환성을 위해 키 앞에 `'npctalk_var_'`를 붙여야 한다는 점을 기억하세요.
대화에서는 해당 set_var 함수를 호출하면 됩니다.

예시:

```json
{
	"effect": [
		{ "u_add_var": "player_val", "value": "testing string" },
		{ "npc_add_var": "npc_test_var", "value": "npc testing string" },
	],
},
```

이후 '<utalk_var_player_val>'와 '<npctalk_var_npc_test_var>'로 각각
'testing string', 'npc testing string'을 표시할 수 있습니다.

---

## Repeat Responses

Repeat response는 아이템 인스턴스마다 한 번씩 응답 목록에 반복 추가되어야 하는 응답입니다.

형식:

```json
{
  "for_item": [
    "jerky",
    "meat_smoked",
    "fish_smoked",
    "cooking_oil",
    "cooking_oil2",
    "cornmeal",
    "flour",
    "fruit_wine",
    "beer",
    "sugar"
  ],
  "response": { "text": "Delivering <topic_item>.", "topic": "TALK_DELIVER_ASK" }
}
```

`"response"`는 필수이며 위에서 설명한 표준 대화 응답이어야 합니다.
`"switch"`는 repeat response에서도 허용되며 일반적으로 동작합니다.

`"for_item"` 또는 `"for_category"` 중 하나를 사용하며,
각 항목은 단일 문자열 또는 아이템/카테고리 목록이 될 수 있습니다.
`response`는 플레이어 또는 NPC 인벤토리의 목록 항목 각각에 대해 생성됩니다.

`"is_npc"`는 선택 bool이며, 있으면 NPC 인벤토리 목록을 검사합니다.
기본은 플레이어 인벤토리 검사입니다.

`"include_containers"`는 선택 bool이며, 있으면 아이템을 담고 있는 컨테이너 아이템이
내용물 아이템과 별개 응답을 생성합니다.

---

## Dialogue Effects

`speaker_effect` 또는 `response`의 `effect` 필드는 아래 effect 중 어느 것이든 될 수 있습니다.
여러 effect는 목록으로 작성하고, 나열 순서대로 처리됩니다.

### Missions

| Effect                                                           | Description                                                                      |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| `assign_mission`                                                 | 미리 선택된 미션을 플레이어 캐릭터에게 할당합니다.                               |
| `mission_success`                                                | 현재 미션을 성공으로 처리합니다.                                                 |
| `mission_failure`                                                | 현재 미션을 실패로 처리합니다.                                                   |
| `clear_mission`                                                  | 플레이어 캐릭터에게 할당된 미션에서 해당 미션을 제거합니다.                      |
| `mission_reward`                                                 | 플레이어에게 미션 보상을 지급합니다.                                             |
| `assign_mission: mission_type_id string`                         | 플레이어에게 `mission_type_id` 미션을 할당합니다.                                |
| `finish_mission: mission_type_id string`,`success: success_bool` | `mission_type_id` 미션을 `success`가 true면 성공, 아니면 실패로 완료 처리합니다. |

### Stats / Morale

| Effect               | Description                                                                                                                                      |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `give_aid`           | 캐릭터의 물림/감염/출혈을 모두 제거하고 각 신체 부위 부상을 10-25 HP 회복합니다. 30분 소요. 시작 시 NPC는 `currently_busy` 30분 효과를 받습니다. |
| `give_all_aid`       | 플레이어 캐릭터와 제작 범위 내 NPC 동료 모두에게 `give_aid`를 수행합니다. 1시간 소요. 시작 시 NPC는 `currently_busy` 1시간 효과를 받습니다.      |
| `buy_haircut`        | 캐릭터에게 12시간 지속되는 이발 사기 보너스를 부여합니다.                                                                                        |
| `buy_shave`          | 캐릭터에게 6시간 지속되는 면도 사기 보너스를 부여합니다.                                                                                         |
| `morale_chat`        | 캐릭터에게 6시간 지속되는 기분 좋은 대화 사기 보너스를 부여합니다.                                                                               |
| `player_weapon_away` | 플레이어 캐릭터가 무기를 집어넣도록(손에서 내려놓도록) 만듭니다.                                                                                 |
| `player_weapon_drop` | 플레이어 캐릭터가 무기를 떨어뜨리게 만듭니다.                                                                                                    |

### Character effects / Mutations

| Effect                                                                                                                                                                                                 | Description                                                                                                                                                                                                                         |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `u_add_effect: effect_string`, (_one of_ `duration: duration_string`, `duration: duration_int`)<br/> `npc_add_effect: effect_string`, (_one of_ `duration: duration_string`, `duration: duration_int`) | 플레이어 캐릭터 또는 NPC가 해당 효과를 `duration_string` 또는 `duration_int` 턴 동안 얻습니다. `duration_string`이 `"PERMANENT"`면 영구 적용됩니다.                                                                                 |
| `u_add_trait: trait_string`<br/> `npc_add_trait: trait_string`                                                                                                                                         | 플레이어 캐릭터 또는 NPC가 해당 trait를 얻습니다.                                                                                                                                                                                   |
| `u_lose_effect: effect_string`<br/> `npc_lose_effect: effect_string`                                                                                                                                   | 플레이어 캐릭터 또는 NPC가 해당 효과를 가지고 있다면 제거합니다.                                                                                                                                                                    |
| `u_lose_trait: trait_string`<br/> `npc_lose_trait: trait_string`                                                                                                                                       | 플레이어 캐릭터 또는 NPC의 해당 trait를 제거합니다.                                                                                                                                                                                 |
| `u_add_var, npc_add_var`: `var_name, type: type_str`, `context: context_str`, `value: value_str`                                                                                                       | 플레이어 캐릭터 또는 NPC가 나중에 `u_has_var`/`npc_has_var`로 조회 가능한 변수로 `value_str`를 저장합니다. `npc_add_var`는 임의의 로컬 변수 저장, `u_add_var`는 임의의 "전역" 변수 저장에 사용되며 effect 설정보다 우선 권장됩니다. |
| `u_lose_var`, `npc_lose_var`: `var_name`, `type: type_str`, `context: context_str`                                                                                                                     | 플레이어 캐릭터 또는 NPC가 동일한 `var_name`, `type_str`, `context_str` 조합의 저장 변수를 지웁니다.                                                                                                                                |
| `u_adjust_var, npc_adjust_var`: `var_name, type: type_str`, `context: context_str`, `adjustment: adjustment_num`                                                                                       | 플레이어 캐릭터 또는 NPC가 저장 변수를 `adjustment_num`만큼 조정합니다.                                                                                                                                                             |
| `barber_hair`                                                                                                                                                                                          | 플레이어가 새 헤어스타일을 고르는 메뉴를 엽니다.                                                                                                                                                                                    |
| `barber_beard`                                                                                                                                                                                         | 플레이어가 새 수염 스타일을 고르는 메뉴를 엽니다.                                                                                                                                                                                   |
| `u_learn_recipe: recipe_string`                                                                                                                                                                        | 플레이어 캐릭터가 `recipe_string` 레시피를 학습 및 암기합니다.                                                                                                                                                                      |
| `npc_first_topic: topic_string`                                                                                                                                                                        | NPC의 첫 대화 토픽을 `topic_string`으로 영구 변경합니다.                                                                                                                                                                            |

### Trade / Items

| Effect                                                                                                                                                                   | Description                                                                                                                                                                                                                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `start_trade`                                                                                                                                                            | 거래 화면을 열고 NPC와 거래할 수 있게 합니다.                                                                                                                                                                                                                                                                                                                                         |
| `buy_10_logs`                                                                                                                                                            | 목장 차고에 통나무 10개를 배치하고 NPC를 1일간 이용 불가로 만듭니다.                                                                                                                                                                                                                                                                                                                  |
| `buy_100_logs`                                                                                                                                                           | 목장 차고에 통나무 100개를 배치하고 NPC를 7일간 이용 불가로 만듭니다.                                                                                                                                                                                                                                                                                                                 |
| `give_equipment`                                                                                                                                                         | 플레이어가 NPC 인벤토리에서 아이템을 선택해 자신의 인벤토리로 옮기게 합니다.                                                                                                                                                                                                                                                                                                          |
| `npc_gets_item`                                                                                                                                                          | 플레이어가 자신의 인벤토리에서 아이템을 선택해 NPC 인벤토리로 옮기게 합니다. NPC가 공간/중량 여유가 없으면 거절하고, 이후 dynamic line에서 `"use_reason"`으로 참조 가능한 reason을 설정합니다.                                                                                                                                                                                        |
| `npc_gets_item_to_use`                                                                                                                                                   | 플레이어가 자신의 인벤토리에서 아이템을 선택해 NPC 인벤토리로 옮기게 합니다. NPC는 장비 착용을 시도하며, 너무 무겁거나 현재 무기보다 열등하면 거절하고, 이후 dynamic line에서 `"use_reason"`으로 참조 가능한 reason을 설정합니다.                                                                                                                                                     |
| `u_buy_item: item_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`, _optional_ `container: container_string`)                                        | NPC가 플레이어에게 아이템(또는 `count_num`개)을 컨테이너에 담아 주고, 지정 시 `op_of_u.owed`에서 `cost_num`을 차감합니다. `op_o_u.owed`가 `cost_num`보다 적으면 거래 창이 열리고 차액을 거래로 메워야 하며, `cost_num`을 충족하기 전에는 아이템을 주지 않습니다.<br/>cost가 없으면 무료로 제공합니다.                                                                                 |
| `u_sell_item: item_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`)                                                                                 | 플레이어가 NPC에게 아이템(또는 `count_num`개)을 주고, 지정 시 NPC의 `op_of_u.owed`에 `cost_num`을 더합니다.<br/>cost가 없으면 무료로 줍니다.<br/>이 effect는 해당 아이템이 최소 `count_num`개 없으면 실패하므로 `u_has_items`로 확인해야 합니다.                                                                                                                                      |
| `u_bulk_trade_accept`<br/> `npc_bulk_trade_accept`                                                                                                                       | `repeat_response` 이후에만 유효. 해당 `repeat_response` 아이템의 모든 인스턴스를 일괄 거래합니다. `u_bulk_trade_accept`는 플레이어가 아이템을 잃고 같은 가치의 NPC faction 통화를 얻습니다. `npc_bulk_trade_accept`는 반대로 플레이어가 아이템을 얻고 같은 가치의 faction 통화를 잃습니다. 잔여 가치가 있거나 NPC가 faction 통화가 없으면 잔액은 NPC `op_of_u.owed`로 들어갑니다.     |
| `u_bulk_donate`<br/> `npc_bulk_donate`                                                                                                                                   | `repeat_response` 이후에만 유효. 플레이어 또는 NPC가 해당 `repeat_response` 아이템의 모든 인스턴스를 이전합니다. `u_bulk_donate`는 플레이어 인벤토리에서 NPC로 이동, `npc_bulk_donate`는 NPC 인벤토리에서 플레이어로 이동합니다.                                                                                                                                                      |
| `u_spend_ecash: amount`                                                                                                                                                  | 플레이어의 재앙 이전 은행 계좌에서 `amount`를 차감합니다. 음수면 e-cash를 획득합니다. NPC는 **e-cash를 다루면 안 되며** 개인 부채와 아이템(파벌 통화 포함)만 다뤄야 합니다.                                                                                                                                                                                                           |
| `add_debt: mod_list`                                                                                                                                                     | NPC의 플레이어에 대한 부채를 `mod_list` 값만큼 증가시킵니다.<br/>예: `{ "effect": { "add_debt": [ [ "ALTRUISM", 3 ], [ "VALUE", 2 ], [ "TOTAL", 500 ] ] } }`는 NPC의 이타성 1500배 + 플레이어 가치 평가 1000배만큼 부채를 증가시킵니다.                                                                                                                                               |
| `u_consume_item`, `npc_consume_item: item_string`, (_optional_ `count: count_num`)                                                                                       | 플레이어 또는 NPC가 인벤토리에서 해당 아이템(또는 `count_num`개)을 삭제합니다.<br/>최소 `count_num`개가 없으면 실패하므로 `u_has_items` 또는 `npc_has_items`로 확인해야 합니다.                                                                                                                                                                                                       |
| `u_remove_item_with`, `npc_remove_item_with: item_string`                                                                                                                | 플레이어 또는 NPC 인벤토리의 해당 아이템 인스턴스를 모두 삭제합니다.<br/>무조건 삭제이며, 아이템이 없어도 실패하지 않습니다.                                                                                                                                                                                                                                                          |
| `u_buy_monster: monster_type_string`, (_optional_ `cost: cost_num`, _optional_ `count: count_num`, _optional_ `name: name_string`, _optional_ `pacified: pacified_bool`) | NPC가 플레이어에게 몬스터를 반려동물로 `count_num`마리(기본 1) 제공하고, 지정 시 `op_of_u.owed`에서 `cost_num`을 차감합니다. `op_o_u.owed`가 `cost_num`보다 적으면 거래 창이 열려 차액을 메워야 하며, `cost_num` 충족 전에는 제공되지 않습니다.<br/>cost가 없으면 무료입니다.<br/>`name_string`이 있으면 지정 이름을 부여합니다. `pacified_bool`이 true면 pacified 효과가 적용됩니다. |

#### Behavior / AI

| Effect                                 | Description                                                                                                   |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| `assign_guard`                         | NPC를 경비로 지정합니다. 동료이고 캠프에 있으면 해당 캠프에 배정됩니다.                                       |
| `stop_guard`                           | 경비 임무를 해제합니다(`assign_guard` 참고). 우호 NPC는 다시 추종 상태로 돌아갑니다.                          |
| `wake_up`                              | 수면 중(단, 진정 상태 아님)인 NPC를 깨웁니다.                                                                 |
| `reveal_stats`                         | 플레이어의 평가 스킬에 따라 NPC 능력치를 공개합니다.                                                          |
| `end_conversation`                     | 대화를 끝내고 이후 NPC가 플레이어를 무시하게 합니다.                                                          |
| `insult_combat`                        | 대화를 끝내고 NPC를 적대화하며, 플레이어가 싸움을 건다는 메시지를 추가합니다.                                 |
| `hostile`                              | NPC를 적대화하고 대화를 종료합니다.                                                                           |
| `flee`                                 | NPC가 플레이어로부터 도망치게 합니다.                                                                         |
| `follow`                               | NPC가 플레이어를 추종하게 하며 "Your Followers" 파벌에 합류시킵니다.                                          |
| `leave`                                | NPC를 "Your Followers" 파벌에서 탈퇴시키고 추종을 중단시킵니다.                                               |
| `follow_only`                          | 파벌 변경 없이 NPC가 플레이어를 추종하게 합니다.                                                              |
| `stop_following`                       | 파벌 변경 없이 NPC 추종을 중단시킵니다.                                                                       |
| `npc_thankful`                         | NPC가 플레이어에게 우호적으로 기울도록 만듭니다.                                                              |
| `drop_weapon`                          | NPC가 무기를 떨어뜨리게 합니다.                                                                               |
| `stranger_neutral`                     | NPC 태도를 중립으로 바꿉니다.                                                                                 |
| `start_mugging`                        | NPC가 플레이어에게 접근해 강탈을 시도하고, 저항하면 공격합니다.                                               |
| `lead_to_safety`                       | NPC가 LEAD 태도를 얻고 플레이어에게 안전 지대로 이동 미션을 부여합니다.                                       |
| `start_training`                       | NPC가 플레이어에게 스킬 또는 무술을 훈련시킵니다.                                                             |
| `companion_mission: role_string`       | NPC 역할에 따라 동료 NPC 미션 목록을 제시합니다.                                                              |
| `basecamp_mission`                     | 지역 베이스캠프에 따라 동료 NPC 미션 목록을 제시합니다.                                                       |
| `bionic_install`                       | NPC가 매우 높은 스킬로 플레이어 인벤토리의 바이오닉을 플레이어에게 이식하고, 난이도에 따라 비용을 청구합니다. |
| `bionic_remove`                        | NPC가 매우 높은 스킬로 플레이어의 바이오닉을 제거하고, 난이도에 따라 비용을 청구합니다.                       |
| `npc_class_change: class_string`       | NPC 파벌을 `class_string`으로 변경합니다.                                                                     |
| `npc_faction_change: faction_string`   | NPC의 파벌 소속을 `faction_string`으로 변경합니다.                                                            |
| `u_faction_rep: rep_num`               | NPC 현재 파벌에 대한 플레이어 평판을 증가시키며, `rep_num`이 음수면 감소시킵니다.                             |
| `toggle_npc_rule: rule_string`         | `"use_silent"`, `"allow_bash"` 같은 bool NPC follower AI rule 값을 토글합니다.                                |
| `set_npc_rule: rule_string`            | `"use_silent"`, `"allow_bash"` 같은 bool NPC follower AI rule 값을 설정합니다.                                |
| `clear_npc_rule: rule_string`          | `"use_silent"`, `"allow_bash"` 같은 bool NPC follower AI rule 값을 해제합니다.                                |
| `set_npc_engagement_rule: rule_string` | NPC follower AI의 교전 거리 규칙을 `rule_string`으로 설정합니다.                                              |
| `set_npc_aim_rule: rule_string`        | NPC follower AI의 조준 속도 규칙을 `rule_string`으로 설정합니다.                                              |
| `npc_die`                              | 대화 종료 시 NPC가 사망합니다.                                                                                |

### Map Updates

`mapgen_update: mapgen_update_id_string`<br/> `mapgen_update:` _list of `mapgen_update_id_string`s_,
(optional `assign_mission_target` parameters) | 추가 매개변수가 없으면 플레이어 현재 위치의 overmap tile에
`mapgen_update_id`의 변경사항(또는 목록의 각 `mapgen_update_id`)을 적용합니다.
`assign_mission_target` 매개변수로 업데이트 대상 overmap tile 위치를 바꿀 수 있습니다.
`assign_mission_target`은 [the missions docs](missions_json), `mapgen_update`는
[the mapgen docs](../map/mapgen)를 참고하세요.

### Deprecated

| Effect                                                                     | Description                                                                                                                 |
| -------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `deny_follow`<br/> `deny_lead`<br/> `deny_train`<br/> `deny_personal_info` | NPC에 몇 시간 동안 해당 effect를 설정합니다.<br/>위에서 설명한 더 유연한 `npc_add_effect` 사용으로 _deprecated_ 되었습니다. |

### Sample effects

```json
{ "topic": "TALK_EVAC_GUARD3_HOSTILE", "effect": [ { "u_faction_rep": -15 }, { "npc_change_faction": "hells_raiders" } ] }
{ "text": "Let's trade then.", "effect": "start_trade", "topic": "TALK_EVAC_MERCHANT" },
{ "text": "What needs to be done?", "topic": "TALK_CAMP_OVERSEER", "effect": { "companion_mission": "FACTION_CAMP" } }
{ "text": "Do you like it?", "topic": "TALK_CAMP_OVERSEER", "effect": [ { "u_add_effect": "concerned", "duration": 600 }, { "npc_add_effect": "touched", "duration": "3600" }, { "u_add_effect": "empathetic", "duration": "PERMANENT" } ] }
```

---

### opinion changes

특수 effect로 NPC가 플레이어 캐릭터를 어떻게 보는지(opinion)를 바꿀 수 있습니다.
다음을 사용하세요.

#### opinion: { }

`opinion` 객체 안에서 trust, value, fear, anger는 모두 선택 키워드입니다.
각 키워드 뒤에는 숫자 값이 와야 하며, NPC 의견이 해당 값만큼 수정됩니다.

#### Sample opinions

```json
{ "effect": "follow", "opinion": { "trust": 1, "value": 1 }, "topic": "TALK_DONE" }
{ "topic": "TALK_DENY_FOLLOW", "effect": "deny_follow", "opinion": { "fear": -1, "value": -1, "anger": 1 } }
```

#### mission_opinion: { }

`mission_opinion` 객체 안에서 trust, value, fear, anger는 모두 선택 키워드입니다.
각 키워드는 숫자 값을 받아 NPC 의견을 해당 값만큼 수정합니다.

---

## Dialogue conditions

조건은 값 없는 단순 문자열, 키+int, 키+문자열, 키+배열, 키+객체 형태가 될 수 있습니다.
배열과 객체는 서로 중첩될 수 있으며, 다른 모든 조건을 포함할 수 있습니다.

다음 키와 단순 문자열을 사용할 수 있습니다.

#### Boolean logic

| Condition | Type   | Description                                                                                                                                                                                    |
| --------- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"and"`   | array  | 배열의 모든 조건이 참이면 `true`. `"[INTELLIGENCE 10+][PERCEPTION 12+] Your jacket is torn. Did you leave that scrap of fabric behind?"` 같은 복합 테스트를 만들 수 있습니다.                  |
| `"or"`    | array  | 배열의 조건 중 하나라도 참이면 `true`. `"[STRENGTH 9+] or [DEXTERITY 9+] I'm sure I can handle one zombie."` 같은 복합 테스트를 만들 수 있습니다.                                              |
| `"not"`   | object | 객체(또는 문자열) 조건이 거짓이면 `true`. 다른 조건을 부정해 복합 테스트를 만들 수 있습니다. 예:<br/>`"[INTELLIGENCE 7-] Hitting the reactor with a hammer should shut it off safely, right?"` |

#### Player or NPC conditions

이 조건들은 플레이어에는 `"u_"` 형태, NPC에는 `"npc_"` 형태로 검사할 수 있습니다.

| Condition                                              | Type          | Description                                                                                                                                                                                                                                                                                              |
| ------------------------------------------------------ | ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"u_male"`<br/> `"npc_male"`                           | simple string | 플레이어 캐릭터 또는 NPC가 남성이면 `true`.                                                                                                                                                                                                                                                              |
| `"u_female"`<br/> `"npc_female"`                       | simple string | 플레이어 캐릭터 또는 NPC가 여성이면 `true`.                                                                                                                                                                                                                                                              |
| `"u_at_om_location"`<br/> `"npc_at_om_location"`       | string        | 플레이어 캐릭터 또는 NPC가 `u_at_om_location` id를 가진 overmap tile 위에 서 있으면 `true`. 특수 문자열 `"FACTION_CAMP_ANY"`는 파벌 캠프 overmap tile 위에 있으면 참이 되도록 바꿉니다. 특수 문자열 `"FACTION_CAMP_START"`는 현재 서 있는 overmap tile을 파벌 캠프로 전환 가능하면 참이 되도록 바꿉니다. |
| `"u_has_trait"`<br/> `"npc_has_trait"`                 | string        | 플레이어 캐릭터 또는 NPC가 특정 trait를 가지면 `true`. 하나의 trait만 검사하는 `u_has_any_trait`/`npc_has_any_trait`의 단순 버전입니다.                                                                                                                                                                  |
| `"u_has_trait_flag"`<br/> `"npc_has_trait_flag"`       | string        | 플레이어 캐릭터 또는 NPC가 특정 trait flag를 가진 trait를 하나라도 가지면 `true`. `u_has_any_trait`/`npc_has_any_trait`의 더 견고한 버전입니다. 특수 trait flag `"MUTATION_THRESHOLD"`는 돌연변이 임계치를 넘었는지 검사합니다.                                                                          |
| `"u_has_any_trait"`<br/> `"npc_has_any_trait"`         | array         | 플레이어 캐릭터 또는 NPC가 배열의 trait/돌연변이 중 하나라도 가지면 `true`. 여러 특정 trait를 검사할 때 사용합니다.                                                                                                                                                                                      |
| `"u_has_var"`, `"npc_has_var"`                         | string        | `"u_has_var"` 또는 `"npc_has_var"`와 같은 딕셔너리 안에 `"type": type_str`, `"context": context_str`, `"value": value_str`가 필수입니다.<br/>`"u_add_var"`/`"npc_add_var"`로 설정된 변수에서 문자열, `type_str`, `context_str`, `value_str`가 일치하면 `true`입니다.                                     |
| `"u_compare_var"`, `"npc_compare_var"`                 | dictionary    | `"type": type_str`, `"context": context_str`, `"op": op_str`, `"value": value_num` 필드가 필수이며, `"u_add_var"`/`"npc_add_var"`와 동일 방식의 var를 참조합니다.<br/>저장 변수가 연산자 `op_str`(`==`, `!=`, `<`, `>`, `<=`, `>=`)와 값 비교를 만족하면 `true`입니다.                                   |
| `"u_has_strength"`<br/> `"npc_has_strength"`           | int           | 플레이어 캐릭터 또는 NPC의 strength가 해당 값 이상이면 `true`.                                                                                                                                                                                                                                           |
| `"u_has_dexterity"`<br/> `"npc_has_dexterity"`         | int           | 플레이어 캐릭터 또는 NPC의 dexterity가 해당 값 이상이면 `true`.                                                                                                                                                                                                                                          |
| `"u_has_intelligence"`<br/> `"npc_has_intelligence"`   | int           | 플레이어 캐릭터 또는 NPC의 intelligence가 해당 값 이상이면 `true`.                                                                                                                                                                                                                                       |
| `"u_has_perception"`<br/> `"npc_has_perception"`       | int           | 플레이어 캐릭터 또는 NPC의 perception이 해당 값 이상이면 `true`.                                                                                                                                                                                                                                         |
| `"u_has_item"`<br/> `"npc_has_item"`                   | string        | 플레이어 캐릭터 또는 NPC 인벤토리에 `u_has_item`/`npc_has_item`의 `item_id`가 있으면 `true`.                                                                                                                                                                                                             |
| `"u_has_items"`<br/> `"npc_has_item"`                  | dictionary    | `u_has_items` 또는 `npc_has_items`는 `item` 문자열과 `count` int를 가진 딕셔너리여야 합니다.<br/>인벤토리에 `item`의 charge/개수가 최소 `count`이면 `true`.                                                                                                                                              |
| `"u_has_item_category"`<br/> `"npc_has_item_category"` | string        | 같은 딕셔너리에 있는 선택 필드 `"count": item_count`는 미지정 시 1입니다. 플레이어/NPC가 `u_has_item_category` 또는 `npc_has_item_category`와 같은 카테고리 아이템을 `item_count`개 가지고 있으면 `true`.                                                                                                |
| `"u_has_bionics"`<br/> `"npc_has_bionics"`             | string        | 플레이어 또는 NPC가 `"u_has_bionics"`/`"npc_has_bionics"`와 일치하는 `bionic_id`의 설치 바이오닉을 가지면 `true`. 특수 문자열 "ANY"는 설치 바이오닉이 하나라도 있으면 참입니다.                                                                                                                          |
| `"u_has_effect"`<br/> `"npc_has_effect"`               | string        | 플레이어 캐릭터 또는 NPC가 해당 `effect_id` 효과 상태이면 `true`.                                                                                                                                                                                                                                        |
| `"u_can_stow_weapon"`<br/> `"npc_can_stow_weapon"`     | simple string | 플레이어 캐릭터 또는 NPC가 무기를 들고 있고 치울 공간이 충분하면 `true`.                                                                                                                                                                                                                                 |
| `"u_has_weapon"`<br/> `"npc_has_weapon"`               | simple string | 플레이어 캐릭터 또는 NPC가 무기를 들고 있으면 `true`.                                                                                                                                                                                                                                                    |
| `"u_driving"`<br/> `"npc_driving"`                     | simple string | 플레이어 캐릭터 또는 NPC가 차량을 조작 중이면 `true`. <b>Note</b> 현재 NPC는 차량을 조작할 수 없습니다.                                                                                                                                                                                                  |
| `"u_has_skill"`<br/> `"npc_has_skill"`                 | dictionary    | `u_has_skill` 또는 `npc_has_skill`은 `skill` 문자열과 `level` int를 가진 딕셔너리여야 합니다.<br/>플레이어 캐릭터 또는 NPC가 해당 `skill`의 `level` 이상이면 `true`.                                                                                                                                     |
| `"u_know_recipe"`                                      | string        | 플레이어 캐릭터가 `u_know_recipe`에 지정된 레시피를 알면 `true`. 실제로 암기한 경우만 인정되며, 레시피가 있는 책을 들고 있는 것만으로는 인정되지 않습니다.                                                                                                                                               |

#### Player Only conditions

`"u_has_mission"` | string | 미션이 플레이어 캐릭터에게 할당되어 있으면 `true`.
`"u_has_ecash"` | int | 플레이어 캐릭터의 재앙 이전 은행 계좌에 `u_has_ecash` 이상 e-cash가 있으면
`true`. NPC는 **e-cash를 다루면 안 되며**, 개인 부채와 아이템(파벌 통화 포함)만 다뤄야 합니다.
`"u_are_owed"` | int | NPC의 `op_of_u.owed`가 `u_are_owed` 이상이면 `true`.
플레이어가 물물교환 없이 NPC에게서 구매 가능한지 확인하는 데 사용할 수 있습니다.
`"u_has_camp"` | simple string | 플레이어가 하나 이상의 활성 베이스캠프를 가지고 있으면 `true`.

#### Player and NPC interaction conditions

| Condition                       | Type          | Description                                                                                                                                                                                                        |
| ------------------------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `"at_safe_space"`               | simple string | NPC의 현재 overmap 위치가 `is_safe()` 테스트를 통과하면 `true`.                                                                                                                                                    |
| `"has_assigned_mission"`        | simple string | 플레이어 캐릭터가 해당 NPC로부터 정확히 1개 미션을 가지고 있으면 `true`. "About that job..." 같은 텍스트에 사용 가능.                                                                                              |
| `"has_many_assigned_missions"`  | simple string | 플레이어 캐릭터가 해당 NPC 미션을 여러 개(1개 초과) 가지고 있으면 `true`. "About one of those jobs..." 같은 텍스트 및 `"TALK_MISSION_LIST_ASSIGNED"` 전환에 사용 가능.                                             |
| `"has_no_available_mission"`    | simple string | 해당 NPC가 플레이어 캐릭터에게 줄 수 있는 일이 없으면 `true`.                                                                                                                                                      |
| `"has_available_mission"`       | simple string | 해당 NPC가 플레이어 캐릭터에게 줄 수 있는 일이 1개면 `true`.                                                                                                                                                       |
| `"has_many_available_missions"` | simple string | 해당 NPC가 플레이어 캐릭터에게 줄 수 있는 일이 여러 개면 `true`.                                                                                                                                                   |
| `"mission_goal"`                | string        | NPC의 현재 미션 목표가 `mission_goal`과 같으면 `true`.                                                                                                                                                             |
| `"mission_complete"`            | simple string | 플레이어가 NPC 현재 미션을 완료했으면 `true`.                                                                                                                                                                      |
| `"mission_incomplete"`          | simple string | 플레이어가 NPC 현재 미션을 완료하지 않았으면 `true`.                                                                                                                                                               |
| `"mission_has_generic_rewards"` | simple string | NPC 현재 미션이 generic rewards 플래그를 가지면 `true`.                                                                                                                                                            |
| `"npc_service"`                 | simple string | NPC가 `"currently_busy"` 효과를 가지지 않으면 `true`. 시간이 걸리는 작업을 고용 가능한지 확인할 때 유용합니다. 기능적으로 `not": { "npc_has_effect": "currently_busy" }`와 동일합니다. `npc_available`과 같습니다. |
| `"npc_allies"`                  | int           | 플레이어 캐릭터가 `npc_allies`명 이상의 NPC 동료를 가지고 있으면 `true`.                                                                                                                                           |
| `"npc_following"`               | simple string | NPC가 플레이어 캐릭터를 추종 중이면 `true`.                                                                                                                                                                        |
| `"is_by_radio"`                 | simple string | 플레이어가 라디오로 NPC와 대화 중이면 `true`.                                                                                                                                                                      |

#### NPC only conditions

| Condition            | Type          | Description                                                                           |
| -------------------- | ------------- | ------------------------------------------------------------------------------------- |
| `"npc_available"`    | simple string | NPC가 `"currently_busy"` 효과를 가지지 않으면 `true`.                                 |
| `"npc_following"`    | simple string | NPC가 플레이어 캐릭터를 추종 중이면 `true`.                                           |
| `"npc_friend"`       | simple string | NPC가 플레이어 캐릭터에게 우호적이면 `true`.                                          |
| `"npc_hostile"`      | simple string | NPC가 플레이어 캐릭터의 적이면 `true`.                                                |
| `"npc_train_skills"` | simple string | NPC가 플레이어보다 높은 레벨의 스킬을 하나 이상 가지고 있으면 `true`.                 |
| `"npc_train_styles"` | simple string | NPC가 플레이어가 모르는 무술 스타일을 하나 이상 알고 있으면 `true`.                   |
| `"npc_has_class"`    | array         | NPC가 특정 NPC class 구성원이면 `true`.                                               |
| `"npc_role_nearby"`  | string        | 100타일 이내에 `npc_role_nearby`와 같은 companion mission role의 NPC가 있으면 `true`. |
| `"has_reason"`       | simple string | 이전 effect가 effect를 완료할 수 없는 이유(reason)를 설정해 두었으면 `true`.          |

#### NPC Follower AI rules

| Condition               | Type   | Description                                                  |
| ----------------------- | ------ | ------------------------------------------------------------ |
| `"npc_aim_rule"`        | string | NPC follower AI의 조준 규칙이 해당 문자열과 일치하면 `true`. |
| `"npc_engagement_rule"` | string | NPC follower AI의 교전 규칙이 해당 문자열과 일치하면 `true`. |
| `"npc_rule"`            | string | 해당 문자열의 NPC follower AI 규칙이 설정되어 있으면 `true`. |

#### Environment

| Condition                | Type          | Description                                                                                                        |
| ------------------------ | ------------- | ------------------------------------------------------------------------------------------------------------------ |
| `"days_since_cataclysm"` | int           | 재앙 이후 `days_since_cataclysm`일 이상 지났으면 `true`.                                                           |
| `"is_season"`            | string        | 현재 계절이 `is_season`과 일치하면 `true`. 값은 `"spring"`, `"summer"`, `"autumn"`, `"winter"` 중 하나여야 합니다. |
| `"is_day"`               | simple string | 현재 낮 시간이면 `true`.                                                                                           |
| `"is_outside"`           | simple string | NPC가 지붕 없는 타일 위에 있으면 `true`.                                                                           |

#### Sample responses with conditions and effects

```json
{
  "text": "Understood.  I'll get those antibiotics.",
  "topic": "TALK_NONE",
  "condition": { "npc_has_effect": "infected" }
},
{
  "text": "I'm sorry for offending you.  I predict you will feel better in exactly one hour.",
  "topic": "TALK_NONE",
  "effect": { "npc_add_effect": "deeply_offended", "duration": 600 }
},
{
  "text": "Nice to meet you too.",
  "topic": "TALK_NONE",
  "effect": { "u_add_effect": "has_met_example_NPC", "duration": "PERMANENT" }
},
{
  "text": "Nice to meet you too.",
  "topic": "TALK_NONE",
  "condition": {
    "not": {
      "npc_has_var": "has_met_PC", "type": "general", "context": "examples", "value": "yes"
    }
  },
  "effect": {
    "npc_add_var": "has_met_PC", "type": "general", "context": "examples", "value": "yes" }
  }
},
{
  "text": "[INT 11] I'm sure I can organize salvage operations to increase the bounty scavengers bring in!",
  "topic": "TALK_EVAC_MERCHANT_NO",
  "condition": { "u_has_intelligence": 11 }
},
{
  "text": "[STR 11] I punch things in face real good!",
  "topic": "TALK_EVAC_MERCHANT_NO",
  "condition": { "and": [ { "not": { "u_has_intelligence": 7 } }, { "u_has_strength": 11 } ] }
},
{ "text": "[100 Merch, 1d] 10 logs", "topic": "TALK_DONE", "effect": "buy_10_logs", "condition":
{ "and": [ "npc_service", { "u_has_items": { "item": "FMCNote", "count": 100 } } ] } },
{ "text": "Maybe later.", "topic": "TALK_RANCH_WOODCUTTER", "condition": "npc_available" },
{
  "text": "[1 Merch] I'll take a beer",
  "topic": "TALK_DONE",
  "condition": { "u_has_item": "FMCNote" },
  "effect": [{ "u_sell_item": "FMCNote" }, { "u_buy_item": "beer", "container": "bottle_glass", "count": 2 }]
},
{
  "text": "Okay.  Lead the way.",
  "topic": "TALK_DONE",
  "condition": { "not": "at_safe_space" },
  "effect": "lead_to_safety"
},
{
  "text": "About one of those missions...",
  "topic": "TALK_MISSION_LIST_ASSIGNED",
  "condition": { "and": [ "has_many_assigned_missions", { "u_is_wearing": "badge_marshal" } ] }
},
{
  "text": "[MISSION] The captain sent me to get a frequency list from you.",
  "topic": "TALK_OLD_GUARD_NEC_COMMO_FREQ",
  "condition": {
    "and": [
      { "u_is_wearing": "badge_marshal" },
      { "u_has_mission": "MISSION_OLD_GUARD_NEC_1" },
      { "not": { "u_has_effect": "has_og_comm_freq" } }
    ]
  }
},
{
    "text": "I killed them.  All of them.",
    "topic": "TALK_MISSION_SUCCESS",
    "condition": {
        "and": [ { "or": [ { "mission_goal": "KILL_MONSTER_SPEC" }, { "mission_goal": "KILL_MONSTER_TYPE" } ] }, "mission_complete" ]
    },
    "switch": true
},
{
    "text": "Glad to help.  I need no payment.",
    "topic": "TALK_NONE",
    "effect": "clear_mission",
    "mission_opinion": { "trust": 4, "value": 3 },
    "opinion": { "fear": -1, "anger": -1 }
},
{
    "text": "Maybe you can teach me something as payment?",
    "topic": "TALK_TRAIN",
    "condition": { "or": [ "npc_train_skills", "npc_train_styles" ] },
    "effect": "mission_reward"
},
{
    "truefalsetext": {
        "true": "I killed him.",
        "false": "I killed it.",
        "condition": { "mission_goal": "ASSASSINATE" }
    },
    "condition": {
        "and": [
            "mission_incomplete",
            {
                "or": [
                    { "mission_goal": "ASSASSINATE" },
                    { "mission_goal": "KILL_MONSTER" },
                    { "mission_goal": "KILL_MONSTER_SPEC" },
                    { "mission_goal": "KILL_MONSTER_TYPE" }
                ]
            }
        ]
    },
    "trial": { "type": "LIE", "difficulty": 10, "mod": [ [ "TRUST", 3 ] ] },
    "success": { "topic": "TALK_NONE" },
    "failure": { "topic": "TALK_MISSION_FAILURE" }
},
{
  "text": "Didn't you say you knew where the Vault was?",
  "topic": "TALK_VAULT_INFO",
  "condition": { "not": { "u_has_var": "asked_about_vault", "value": "yes", "type": "sentinel", "context": "old_guard_rep" } },
  "effect": [
      { "u_add_var": "asked_about_vault", "value": "yes", "type": "sentinel", "context": "old_guard" },
      { "mapgen_update": "hulk_hairstyling", "om_terrain": "necropolis_a_13", "om_special": "Necropolis", "om_terrain_replace": "field", "z": 0 }
  ]
},
{
  "text": "Why do zombies keep attacking every time I talk to you?",
  "topic": "TALK_RUN_AWAY_MORE_ZOMBIES",
  "condition": { "u_has_var": "even_more_zombies", "value": "yes", "type": "trigger", "context": "learning_experience" },
  "effect": [
    { "mapgen_update": [ "even_more_zombies", "more zombies" ], "origin_npc": true },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_x": 1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_x": -1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_y": 1 },
    { "mapgen_update": "more zombies", "origin_npc": true, "offset_y": -1 }
  ]
}
```
