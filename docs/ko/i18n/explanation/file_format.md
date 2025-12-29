# 번역 파일 포맷 (.po)

번역은 각 언어 및 국가별 언어 코드로 명명된 [`".po"` 파일(Portable Object)][po]에 저장됩니다. 예를
들어 스페인에서 사용되는 스페인어에 대한 번역은 `es_ES.po`에, 멕시코에서 사용되는 스페인어에 대한
번역은 `es_MX.po`에 저장됩니다.

[po]: https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html

평범한 텍스트 파일이므로 아무 텍스트 편집기를 써도 상관없으나 보통은
[Poedit](https://poedit.net)같은 전용 번역 편집기나 <https://translations.launchpad.net> 웹 기반
번역 도구를 쓸 수도 있습니다.

`".po"` 파일의 형식은 번역할 영어 문구와 그 뒤에 로컬 번역이 있는 항목 목록입니다. 영어 문구는
`msgid`로 시작하는 줄에 있고, 번역된 구문은 `msgstr`로 시작하는 줄에 있습니다.

`msgid` 줄 앞에는 해당 단어나 문구가 소스 코드에서 어디에서 왔는지를 나타내는 주석 줄이 있습니다.
원문의 의미가 명확하지 않을 때 맥락을 파악하는데 도움을 줍니다. 또한 번역을 더 쉽게 하기 위해
개발자가 남긴 주석이 있을 수도 있습니다.

대부분의 항목은 다음과 같이 표시됩니다:

```
#: action.cpp:421
msgid "Construct Terrain"
msgstr "niarreT tcurtsnoC"
```

여기서 영어 문구는 "Construct Terrain" 이고, `action.cpp` 파일의 421 번째 줄에 나와 있습니다. 이
예제는 그저 영어 문자를 뒤집은 것입니다. 이렇게 하면 "Construct Terrain" 대신 "niarreT tcurtsnoC"가
표시됩니다.

또 다른 예제로는:

```
#: action.cpp:425 defense.cpp:635 defense.cpp:701 npcmove.cpp:2049
msgid "Sleep"
msgstr "pleeS"
```

방금의 예제와 비슷하지만, 더 많은 곳에서 쓰이는 문구를 번역했습니다. 이렇게 하면 `action.cpp`,
`defense.cpp`(두 번), `npcmove.cpp` 총 네 곳에서 문구 "Sleep"이 "pleeS"로 표시됩니다.

## 파일 헤더

".po"` 파일 상단의 헤더는 주석/msgid/msgstr 형식과 다른 유일한 부분입니다.

이미 설정된 번역을 작업하는 경우에는 수정할 필요가 없습니다.

새로 번역할 경우, 사용 중인 편집기, 또는 번역을 초기화할 때 권장되는 방식인 `msginit` 프로그램을
통해서 대부분 설정이 완료되어야 합니다(TRANSLATING.md 참조).

다른 번역 파일에서 시작하는 경우에도 몇 가지 사항을 변경해야 할 수 있습니다. 가능한 한 최선을 다해
입력하세요.

헤더는 다음과 같이 표시됩니다:

```
# French translations for Cataclysm-DDA package.
# Copyright (C) 2013 CleverRaven and Cataclysm-DDA contributors.
# This file is distributed under the same license as the Cataclysm-DDA package.
# Administrator <EMAIL@ADDRESS>, 2013.
#
msgid ""
msgstr ""
"Project-Id-Version: 0.7-git\n"
"Report-Msgid-Bugs-To: http://github.com/CleverRaven/Cataclysm-DDA\n"
"POT-Creation-Date: 2013-08-01 13:44+0800\n"
"PO-Revision-Date: 2013-08-01 14:02+0800\n"
"Last-Translator: YOUR NAME <your@email.address>\n"
"Language-Team: French\n"
"Language: fr\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"
```

새로 번역을 시작하거나 기존 번역을 담당하고 있는 경우, 번역과 관련된 질문이나 문제가 있을 때 연락을
받을 수 있도록 이름과 이메일 주소를 기재하면 도움이 됩니다.

수동으로 쉽게 작성할 수 없는 유일한 중요한 부분은 [`복수형 양식`](#복수형-양식) 항목입니다. 이
섹션은 귀하의 언어에서 다양한 수의 사물이 처리되는 방식을 결정합니다. 이에 대해서는 나중에 자세히
설명하겠습니다.

## 문자열 및 줄 바꿈 서식 지정

일부 문자열에는 `%s`, `%2$d` 및 `\n`과 같은 특수 용어가 있습니다.

`\n`은 줄 바꿈을 나타냅니다. 코드에서 줄 바꿈이 가능한 경우 줄을 감싸기 때문에 대부분 불필요하지만,
때로는 다른 줄에 내용을 배치할 때 사용되기도 합니다. 번역에서 새 줄이 있어야 할 곳에 `\n`을 사용하면
됩니다.

`%s` 및 기타 유사한 용어는 게임이 실행 중일 때 다른 용어로 대체됩니다. 번역에 따라 이러한 용어를
이동해야 할 수도 있습니다. 번역에서 `%`로 시작하는 모든 용어는 그대로 유지하는 것이 중요합니다.

다음은 `%d`를 숫자로 대체한 예입니다:

```
#: addiction.cpp:224
#, c-format
msgid ""
"Strength - %d;   Perception - 1;   Dexterity - 1;\n"
"Depression and physical pain to some degree.  Frequent cravings.  Vomiting."
msgstr ""
";1 - ytiretxeD   ;1 - noitpecreP   ;%d - htgnertS\n"
".gnitimoV  .sgnivarc tneuqerF  .eerged emos ot niap lacisyhp dna noisserpeD"
```

여기서 `%d`가 반전되지 않고 `\n`이 줄 끝에 남아 있는 것이 중요합니다. 이 경우 메시지가 표시될 때
`%d`는 캐릭터의 힘 ( STR ) 수정자로 대체됩니다.

경우에 따라 용어의 순서를 변경해야 할 수도 있습니다. 게임에 혼란을 줄 수 있으니까요. `%` 용어의
순서를 변경되면 모든 용어에 숫자를 추가해야 게임에서 어떤 것이 어떤 것인지 알 수 있습니다. 일부
문자열에는 이미 이러한 숫자가 있지만 그렇지 않은 문자열도 있을 수 있습니다.

예를 들어 `%s가 %s를 쐈다!`가 들어 있는 문자열이 있으면 번역할 때 바뀔 수 있습니다. 아마도
`%s가 %s에게 쏘였다!`와 같이 될 것입니다. 하지만 지금은 거꾸로, 쏘는 사람이 맞는 사람과 바뀐
것입니다.

이 경우 각 `%s`는 숫자(1~9)로 번호를 매긴 다음 `%`와 `s` 사이에 달러 기호( `$` )를 넣어야 합니다.
예를 들어 `%1$s가 %2$s를 쐈다!`는 `%s가 %s를 쐈다!`와 동일합니다. 따라서 위의 번역 예제는
`%2$s가 %1$s에게 쏘였다!`가 될 수 있으며, 이 번역은 올바르게 작동합니다.

게임에서 이러한 `%1$s` `%2$s` 매개변수를 자동으로 알아낼 수 있지만, (A): 번역의 모든 `%` 용어에
번호가 매겨져 있는지, (B): 영어 텍스트의 원래 순서에 따라 번호가 올바른지, 그 두 가지를 확인해야
합니다.

예를 들자면:

```
#: map.cpp:680
#, c-format
msgid "%s loses control of the %s."
msgstr "%2$s eht fo lortnoc sesol %1$s"
```

게임 내에서 `아비게일`이 `트럭`을 운전하고 있다고 가정할 때 `kcurt eht fo lortnoc sesol liagibA`로
표시됩니다.

## 문자열에 포함된 특수 태그

번역할 때 일부 문자열의 앞이나 안쪽에 특수 태그가 있을 수 있습니다. 이러한 태그는 그대로 두고 나머지
문자열만 번역해야 합니다.

예를 들어, "data/raw/names.json"의 NPC 및 도시 이름 앞에는 단어와의 충돌을 피하기 위해 `<name>`이
붙습니다(예: 재료의 `Wood`, 성의 `Wood`). 이러한 경우 `<name>` 부분을 남겨 두어야 합니다.

이하는 그 예시입니다:

```
#. ~ proper name; gender=female; usage=given
#: lang/json/json_names.py:6
msgid "<name>Abigail"
msgstr "<name>liagibA"
```

또한 이름 위에는 해당 이름이 게임 내에서 어떤 용도로 사용되는지 알려주는 주석이 표시됩니다. 예시의
경우 '아비게일'은 여성 NPC의 이름으로 사용할 수 있는 이름입니다.

## 복수형 양식

많은 언어가 사물의 수에 따라 다른 용어를 사용합니다. 이러한 용어는 `".po"` 파일 헤더의 `Plural-Form`
줄에 정의된 복수형을 사용하여 지원됩니다.

이 경우 수에 따라 다른 형식을 위한 여러 개의 `msgstr` 줄이 있습니다. 게임은 개수에 따라 자동으로
올바른 형태를 선택합니다.

이하는 그 예시입니다:

```
#: melee.cpp:913
#, c-format
msgid "%d enemy hit!"
msgid_plural "%d enemies hit!"
msgstr[0] "!tih ymene %d"
msgstr[1] "!tih seimene %d"
```

여기서 첫 번째 항목은 `enemy`가 하나만 있을 때, 두 번째 항목은 `enemies`이 하나보다 더 있을 때를
나타냅니다. 이 규칙은 언어마다 크게 다릅니다.

> 역주:
>
> 한국어는 해당사항이 없습니다.
