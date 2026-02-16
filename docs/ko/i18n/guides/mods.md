# 유저 제작 모드 번역하기

> 역주:
>
> 타이틀 원문: Translate third-party mods

## 들어가며

이 문서는 Cataclysm: Bright Night 의 모드 번역 워크플로우를 설정하고 운영하는 방법에 대해 간략하게
설명하는 것을 목표로 합니다.

모드 현지화를 위해 게임에서는 [GNU gettext](https://www.gnu.org/software/gettext/) 와 유사한 커스텀
현지화 시스템을 사용하며, GNU gettext MO 파일과 호환됩니다.

transifex 또는 gettext를 지원하는 다른 플랫폼이나 소프트웨어를 사용할 수 있지만, 이 문서에서는
[Poedit](https://poedit.net/) 및 명령줄
[GNU gettext 유틸리티](https://www.gnu.org/software/gettext/)로 작업하는 방법에 대한 예제만
제공합니다.

PO/POT/MO 파일에 대한 자세한 설명이나 GNU gettext 유틸리티를 사용하여 작업하는 방법에 대한 자세한
설명은 [GNU gettext 매뉴얼](https://www.gnu.org/software/gettext/manual/gettext.html)을 참조하세요.

대격변의 밝은 밤과 모드의 문자열 번역에 대한 일반적인 팁을 얻으려면
[translation API](../reference/translation) 를 참조하세요.

## 간단한 용어 해설

### POT 파일

Portable Object Template (`.pot`).

이 파일은 모드의 JSON 및 Lua 소스 파일에서 추출한 원본(영어) 문자열이 포함된 텍스트 파일입니다. POT
파일은 모든 언어의 기존 PO 파일을 비워두거나 업데이트하는 데 사용되는 템플릿입니다.

### PO 파일

Portable Object (`.po`).

한 언어에 대한 번역된 문자열이 포함된 텍스트 파일입니다. PO 파일은 번역가가 작업하는 파일이며, MO
파일로 컴파일될 파일입니다.

### MO 파일

Machine Object (`.mo`).

한 언어에 대한 번역된 문자열이 포함된 바이너리 파일입니다. MO 파일은 게임을 로드하고 번역된 문자열을
가져오는 곳입니다.

## 워크플로우 개요

The first translation workflow is as follows: 첫 번째 번역 워크플로우는 다음과 같습니다:

1. 모드 JSON 및 Lua 소스 파일에서 문자열을 POT 파일로 추출합니다.
2. 이 POT에서 대상 언어에 대한 PO 파일을 생성합니다.
3. 번역된 문자열로 PO 파일을 채웁니다.
4. PO를 MO로 컴파일합니다.
5. MO를 모드 파일에 넣습니다.

시간이 지남에 따라 모드가 변경되면 문자열도 변경될 수 있습니다. 기존 번역을 업데이트하는 방법은
다음과 같습니다:

1. 모드 JSON 및 Lua 소스 파일에서 문자열을 새 POT 파일로 추출합니다.
2. 새 POT 파일에서 기존 PO 파일을 업데이트합니다.
3. PO 파일에서 새 문자열을 추가하거나 기존 번역된 문자열을 편집합니다.
4. PO를 MO로 컴파일합니다.
5. 모드 파일에서 이전 MO를 새 버전으로 교체합니다.

두 워크플로우의 1단계 모두 문자열 추출을 위한 환경을 설정해야 합니다(아래 참조).

2~4단계는 번역 소프트웨어를 사용하여 모드 작성자/관리자 또는 번역가가 수행할 수 있습니다.

## 문자열 추출을 위한 환경 설정

`polib` 및 `luaparser` 모듈이 설치된 Python 3이 필요합니다(`pip`을 통해 제공).

문자열 추출을 위한 스크립트는 리포지토리의 `lang` 하위 디렉터리에서 찾을 수 있습니다:

- `extract_json_strings.py` - 기본 문자열 추출 루틴
- `dedup_pot_file.py` - 첫 번째 스크립트에서 생성된 POT 파일의 오류 수정
- `extract_mod_strings.bat` (Linux/MacOS의 경우 `extract_mod_strings.sh`) - 나머지 2개의 스크립트를
  자동화합니다.

## 문자열 추출

이 3개의 스크립트를 모드 폴더에 복사합니다:

- Windows의 경우, `extract_mod_strings.bat`을 더블 클릭합니다.
- Linux/MacOS의 경우 터미널을 열고 `./extract_mod_strings.sh`를 실행합니다.

프로세스가 오류 없이 완료되면 `extracted_strings.pot` 파일이 들어 있는 새 `lang` 폴더가 표시됩니다.

## 새 PO 생성

PO 파일을 생성하기 전에 언어 ID를 선택해야 합니다.

데이터/raw/languages.json`을 열어 게임에서 지원하는 언어 목록을 확인합니다.

이 목록에서 각 항목은 `ln_LN` 형식의 고유한 ID를 가지며, 여기서 `ln`은 언어를, `LN`은 방언을
나타냅니다. 언어+방언을 정확히 일치시키려면 전체 `ln_LN`을 사용하거나, 방언에 관계없이 게임에서
자신의 MO를 사용하도록 하려면 `ln`을 사용하면 됩니다.

### Poedit 으로 하기

1. Poedit으로 POT 파일을 엽니다.
2. "새 번역 만들기" 버튼을 누릅니다(하단 근처에 표시되어야 함).
3. 언어 선택 대화 상자에서 선택한 언어 ID를 입력합니다.
4. 파일을 '경로/to/mod/lang/LANG.po'로 저장합니다. 여기서 'LANG'은 동일한 언어 아이디입니다.

### msginit 으로 하기

```bash
msginit -i lang/index.pot -o lang/LANG.po -l LANG.UTF-8 --no-translator
```

여기서 `LANG`은 선택한 언어 ID입니다.

## 기존 PO 업데이트하기

### Poedit 으로 하기

1. Poedit으로 PO 파일 엽니다.
2. `Catalog->Update from POT file...`를 고르고 새 POT 파일을 선택합니다.
3. 파일을 저장합니다.

### msgmerge 으로 하기

```bash
msgmerge lang/LANG.po lang/index.pot
```

## PO를 MO로 컴파일하기

### Poedit 으로 하기

1. Poedit으로 PO 파일을 엽니다.
2. MO 파일이 UTF-8을 사용하여 인코딩되었는지 확인합니다(기본적으로 인코딩되어 있어야 하며,
   `Catalog->Properties->"Translation properties" 탭->Charset`에서 다시 확인할 수 있습니다).
3. 기본적으로 PO 파일을 저장할 때마다 Poedit은 자동으로 MO로 컴파일하지만,
   `File->Compile to MO...`을 통해 명시적으로도 동일한 작업을 수행할 수 있습니다.

### msgfmt 으로 하기

```
msgfmt -o lang/LANG.mo lang/LANG.po
```

## 모드에 MO 파일 추가하기

모드 파일 디렉토리에 `lang` 디렉토리를 생성하고 거기에 MO를 넣습니다:

```
mods/
    YourMod/
        modinfo.json
        lang/
            es.mo
            pt_BR.mo
            zh_CN.mo
```

**참고:** POT/PO 파일을 같은 `lang` 하위 디렉토리에 저장하면 파일을 추적하기가 더 쉬워질 수
있습니다. 게임에서 이러한 파일은 무시되며, 모드 폴더 구조는 다음과 같이 표시됩니다:

```
mods/
    YourMod/
        modinfo.json
        lang/
            extracted_strings.pot
            es.po
            es.mo
            pt_BR.po
            pt_BR.mo
            zh_CN.po
            zh_CN.mo
```

## 기타 참고 사항

### JSON과 같이 MO 파일의 위치나 이름을 임의로 지정할 수 있나요?

아니오. 이 게임은 `modinfo.json` 에 지정된 모드의 `path` 경로의 `lang` 하위 디렉토리에 있는 특정
이름의 MO 파일을 찾습니다(지정되지 않은 경우 `path`는 모드의 디렉토리와 일치합니다).

그러나 모든 모드는 자동으로 문자열 번역에 다른 모드의 번역 파일을 사용하려고 시도합니다. 따라서 다른
모드 (또는 모드 컬렉션)를 위해 순전히 "번역 팩"인 모드를 만들 수 있습니다.

### 실행 중인 게임에서 번역 다시 로드하기

디버그 메뉴를 열고 `정보...->번역 다시 불러오기`를 선택하면 게임이 디스크에서 모든 MO 파일을 다시
로드합니다.

이렇게 하면 번역가가 MO 파일을 컴파일할 수 있는 경우 번역된 문자열이 게임에서 어떻게 보이는지 쉽게
확인할 수 있습니다.

Poedit을 사용한 워크플로우 예시:

1. 문자열 번역하기
2. Ctrl+S 누르기
3. 게임에서 Alt+Tab을 누릅니다.
4. 디버그 메뉴를 통해 번역 파일을 다시 불러옵니다.
5. 이제 게임에 번역된 문자열이 표시됩니다.

### MO 로드 순서

MO 로드 순서는 다음과 같습니다:

1. UI, 하드코딩된 기능, 기본 "모드"( `data/json/`) 및 자체 제공 모드에 대한 번역 문자열이 포함된
   기본 게임의 MO 파일이 항상 첫 번째로 로드됩니다.
2. 그런 다음 모드의 MO 파일이 모드 로드 순서와 동일한 순서로 로드됩니다.

### 방언

MO 파일을 불러올 때 게임에서는 먼저 이름에 언어와 방언이 정확히 일치하는 파일을 찾습니다. 그런
파일이 없으면 방언이 없는 파일을 찾습니다.

예를 들어 '스페인어(에스파냐)`를 사용하는 경우 선택 순서는 다음과 같습니다.

1. `es_ES.mo`
2. `es.mo`

그리고 `Español (아르헨티나)`를 사용하는 경우 선택 순서는 다음과 같습니다.

1. `es_AR.mo`
2. `es.mo`

따라서 정확한 번역 파일이 없는 경우 스페인어 방언 중 하나에 대해 `es.mo`가 로드됩니다.

### 두 개 이상의 모드가 동일한 문자열에 대해 서로 다른 번역을 제공하는 경우 어떻게 하나요?

그러면 게임은 다음 규칙에 따라 어떤 것을 사용할지 선택합니다:

1. 문자열 A의 번역에는 복수형이 있지만 문자열 B의 번역에는 복수형이 없는 경우, 단수형과 복수형
   모두에 번역 A가 사용됩니다.
2. 번역 A와 B에 모두 복수형이 있거나 둘 다 복수형이 없는 경우 첫 번째로 로드된 번역이 사용됩니다(MO
   로드 순서 참조).

기본 게임과 다른 번역을 원하거나 다른 모드의 문자열과 충돌하지 않게 하려면 해당 JSON 객체의 문자열에
번역 컨텍스트를 추가하세요(번역 컨텍스트를 지원하는 필드는 [여기](../reference/translation) 참조).

### 참고: 모드 번역을 구현하는 PR

https://github.com/cataclysmbn/Cataclysm-BN/pull/505

### [모드 번역 예시](https://github.com/Kenan2000/Bright-Nights-Kenan-Mod-Pack/pull/36)
