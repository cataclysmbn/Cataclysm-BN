# 번역 파일 포맷 (.po)

번역 데이터는 각 언어 및 국가별 언어 코드로 명명된 [`".po"` 파일(Portable Object)][po]에 저장됩니다. 예를 들어 스페인 표준어 번역은 `es_ES.po`에, 멕시코 스페인어 번역은 `es_MX.po`에 저장되며, 한국어 번역은 `ko.po`에 저장됩니다.

[po]: https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html

이 파일은 평범한 텍스트 파일이므로 메모장 등 아무 텍스트 편집기를 써도 상관없으나, 보통은 [Poedit](https://poedit.net) 같은 전용 번역 편집기나 <https://translations.launchpad.net> 같은 웹 기반 번역 도구를 사용하면 훨씬 편리합니다.

`".po"` 파일의 구조는 '번역할 영어 원문'과 그에 대응하는 '로컬 번역문'이 쌍을 이루는 목록으로 구성됩니다. 영어 원문은 `msgid`로 시작하는 줄에, 번역된 문장은 `msgstr`로 시작하는 줄에 위치합니다.

`msgid` 줄 바로 위에는 해당 단어나 문구가 소스 코드의 어느 파일, 몇 번째 줄에서 왔는지를 알려주는 주석(`xxx.cpp:123` 형태)이 있습니다. 이는 원문의 의미가 명확하지 않을 때 문맥을 파악하는 데 큰 도움을 줍니다. 또한 개발자가 번역자를 위해 남겨둔 별도의 주석이 있을 수도 있습니다.

대부분의 항목은 다음과 같은 형태를 띱니다:

```po
#: action.cpp:421
msgid "Construct Terrain"
msgstr "지형 건설"
```

위 예제에서 영어 원문은 "Construct Terrain"이며, 이는 `action.cpp` 파일의 421번째 줄에 있는 내용입니다. `msgstr`에 입력된 "지형 건설"이라는 문구가 게임 내에서 "Construct Terrain" 대신 표시되게 됩니다.

또 다른 예제는 다음과 같습니다:

```po
#: action.cpp:425 defense.cpp:635 defense.cpp:701 npcmove.cpp:2049
msgid "Sleep"
msgstr "잠자기"
```

앞선 예제와 비슷하지만, 이 단어는 더 많은 곳에서 사용되고 있습니다. 이렇게 한 번만 번역해두면 `action.cpp`, `defense.cpp`(두 곳), `npcmove.cpp`까지 총 네 군데의 코드에서 "Sleep"이라는 단어가 모두 "잠자기"로 표시됩니다.

## 파일 헤더

`".po"` 파일의 맨 윗부분에 있는 헤더는 일반적인 `msgid`/`msgstr` 형식과 조금 다른 유일한 부분입니다.

이미 설정이 완료된 번역 파일을 수정하는 경우라면 이 부분을 건드릴 필요가 없습니다.

새로 번역을 시작하는 경우, 사용 중인 편집기나 `msginit` 프로그램(번역 초기화 권장 도구)이 대부분의 설정을 자동으로 완료해 줍니다(자세한 내용은 TRANSLATING.md 참조).

다른 언어의 번역 파일을 복사해서 시작하는 경우 몇 가지 항목을 수정해야 할 수 있습니다. 가능한 한 정확하게 정보를 입력해 주세요.

헤더는 보통 다음과 같이 생겼습니다:

```po
# Korean translations for Cataclysm-BN package.
# Copyright (C) 2023 Cataclysm-BN contributors.
# This file is distributed under the same license as the Cataclysm-BN package.
# Translator Name <EMAIL@ADDRESS>, 2023.
#
msgid ""
msgstr ""
"Project-Id-Version: 0.7-git\n"
"Report-Msgid-Bugs-To: http://github.com/CleverRaven/Cataclysm-BN\n"
"POT-Creation-Date: 2023-01-01 13:44+0900\n"
"PO-Revision-Date: 2023-01-02 14:02+0900\n"
"Last-Translator: 홍길동 <gildong@example.com>\n"
"Language-Team: Korean\n"
"Language: ko\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=1; plural=0;\n"
```

새로 번역을 시작하거나 관리자가 되는 경우, 번역과 관련된 문의나 문제를 전달받을 수 있도록 이름과 이메일 주소를 기재하는 것이 좋습니다.

수동으로 작성하기 가장 까다로운 부분은 [`복수형 양식(Plural-Forms)`](#복수형-양식-plural-forms) 항목입니다. 이 부분은 해당 언어에서 '개수'에 따라 문법이 어떻게 변하는지를 정의합니다. 이에 대해서는 뒤에서 다시 설명하겠습니다.

## 문자열 및 줄 바꿈 서식 지정

일부 문자열에는 `%s`, `%2$d`, `\n`과 같은 특수 기호가 포함되어 있습니다.

`\n`은 줄 바꿈(Enter)을 의미합니다. 게임 코드에서 자동으로 줄을 바꿔주는 경우가 많아 자주 쓰이지는 않지만, 강제로 줄을 바꿔야 할 때 사용됩니다. 번역문에서도 줄을 바꾸고 싶은 위치에 `\n`을 넣으면 됩니다.

`%s`, `%d` 같은 기호는 게임이 실행될 때 실제 단어나 숫자로 교체되는 '빈칸'입니다. 번역할 때 어순에 맞춰 이 기호들의 위치를 옮길 수는 있지만, **`%`로 시작하는 기호 자체는 절대 삭제하거나 변경하면 안 됩니다.**

다음은 `%d`가 숫자로 대체되는 예시입니다:

```po
#: addiction.cpp:224
#, c-format
msgid ""
"Strength - %d;   Perception - 1;   Dexterity - 1;\n"
"Depression and physical pain to some degree.  Frequent cravings.  Vomiting."
msgstr ""
"힘 - %d;   감각 - 1;   민첩 - 1;\n"
"약간의 우울증과 신체적 통증. 잦은 갈망. 구토 증세."
```

여기서 중요한 점은 `%d`는 그대로 두고, `\n`도 원문과 동일하게 줄 끝에 유지했다는 것입니다. 게임 내에서 이 메시지가 표시될 때 `%d` 자리에 캐릭터의 힘(Strength) 수치가 자동으로 들어갑니다.

### 변수 순서 변경 (%1$s)

한국어는 영어와 어순이 다르기 때문에, 종종 문장 내 변수의 순서를 바꿔야 할 때가 있습니다. 단순히 `%s`의 순서만 바꾸면 게임은 어떤 숫자를 어디에 넣어야 할지 모르게 됩니다.

이때는 각 `%` 기호에 **번호**를 매겨야 합니다. `%`와 `s`(또는 d) 사이에 `1$`, `2$`와 같이 번호와 달러 기호를 넣어 순서를 지정해 줍니다.

예를 들어 원문이 `"%s shoots %s!"` (누가 누구를 쐈다!)라고 가정해 봅시다.
여기서 첫 번째 `%s`는 쏘는 사람(A), 두 번째 `%s`는 맞는 사람(B)입니다.

- 영어 원문: `%s shoots %s!` -> A shoots B!
- 한국어 직역: `%s(이)가 %s(을)를 쐈습니다!` (순서 변경 없음)

하지만 만약 피동형으로 번역하고 싶어서 **"B가 A에게 맞았다"**라고 쓰고 싶다면 어떨까요?

- 잘못된 번역: `%s가 %s에게 맞았습니다!` -> 이렇게 쓰면 **A가 B에게** 맞은 것이 되어 뜻이 반대가 됩니다.
- 올바른 번역: `%2$s(이)가 %1$s에게 맞았습니다!`

이렇게 `%1$s`, `%2$s`를 사용하여 명확히 지정해주면, 문장 구조가 바뀌어도 게임은 "아, 여기에 두 번째 변수(B)를 넣고, 저기에 첫 번째 변수(A)를 넣으라는 뜻이구나"라고 이해합니다.

게임 시스템상 이런 매개변수를 자동으로 인식하기도 하지만, 번역 후에는 다음 두 가지를 꼭 확인해야 합니다.

1. 번역문에 포함된 모든 `%` 변수에 번호가 매겨져 있는가?
2. 그 번호가 영어 원문의 논리에 맞게 올바른 대상을 가리키고 있는가?

실제 예시를 봅시다:

```po
#: map.cpp:680
#, c-format
msgid "%s loses control of the %s."
msgstr "%1$s(이)가 %2$s의 제어권을 잃었습니다."
```

게임 내에서 '아비게일(Abigail)'이 '트럭(Truck)'을 운전하다 사고가 났다면, 위 번역은 `아비게일(이)가 트럭의 제어권을 잃었습니다.`라고 올바르게 표시될 것입니다.

## 문자열에 포함된 특수 태그

번역할 때 문자열의 맨 앞이나 중간에 특수 태그가 붙어 있는 경우가 있습니다. 이런 태그는 번역하지 말고 그대로 둬야 합니다.

예를 들어, `data/raw/names.json`에 있는 NPC나 도시 이름 앞에는 `<name>`이라는 태그가 붙어 있습니다. 이는 'Wood' 같은 단어가 '나무(재료)'인지 '우드(성씨)'인지 구별하기 위함입니다. 번역할 때 이 `<name>` 부분은 반드시 남겨둬야 합니다.

예시:

```po
#. ~ proper name; gender=female; usage=given
#: lang/json/json_names.py:6
msgid "<name>Abigail"
msgstr "<name>아비게일"
```

이름 위에는 해당 이름이 성별이 무엇인지, 성인지 이름인지 알려주는 주석이 달려 있어 번역을 돕습니다. 위 예시의 'Abigail'은 여성 이름으로 사용된다는 것을 알 수 있습니다.

## 복수형 양식 (Plural Forms)

많은 언어(특히 영어)는 사물의 개수에 따라 단어의 형태가 변합니다(예: apple vs apples). `".po"` 파일 헤더의 `Plural-Forms` 줄은 해당 언어가 복수형을 어떻게 처리하는지 정의합니다.

영어나 러시아어 등 복수형 변화가 뚜렷한 언어는 `msgstr[0]`, `msgstr[1]` 등으로 나뉘어 개수에 따른 번역을 따로 입력합니다.

예시 (영어의 경우):

```po
#: melee.cpp:913
#, c-format
msgid "%d enemy hit!"
msgid_plural "%d enemies hit!"
msgstr[0] "%d enemy hit!"  # 적이 1명일 때
msgstr[1] "%d enemies hit!" # 적이 여러 명일 때
```

게임은 상황에 맞는 숫자에 따라 알맞은 문장을 자동으로 선택합니다.

> **참고:**
> 한국어는 문법적으로 엄격한 복수형 구분을 요구하지 않으므로(예: 사과 1개, 사과 2개 - 단어 변화 없음), 보통 `Plural-Forms: nplurals=1; plural=0;`으로 설정하고 `msgstr[0]` 한 줄에만 번역을 입력하여 모든 경우를 처리합니다.
