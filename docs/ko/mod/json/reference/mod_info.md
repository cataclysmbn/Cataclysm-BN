# MOD_INFO

[MODDING.md](../tutorial/modding.md)도 참조하세요.

`MOD_INFO` 타입의 객체는 모드 자체를 설명합니다. 각 모드는 정확히 하나의 `MOD_INFO`를 가져야 하며,
다른 타입의 모드 객체들과 달리 게임 실행 시 타이틀 화면이 표시되기 전에 로드됩니다.
따라서 이와 관련된 모든 오류는 타이틀 화면이 표시되기 전에 나타납니다.

현재 관례는 모드의 루트 디렉토리 내에 있는 `mod_info.json` 파일에 `MOD_INFO`를 넣는 것입니다.

예시:

```json
[
  {
    "type": "MOD_INFO",

    // 모드의 고유 식별자, 명확성을 위해 ASCII 문자, 숫자, 밑줄만 사용하는 것을 권장합니다.
    "id": "better_zeds",
    // 모드의 카테고리, 지원되는 값 목록은 MODDING.md를 참조하세요.
    "category": "content",
    // 모드의 표시 이름, 영어로 작성.
    "name": "Better Zombies",
    // 모드의 설명, 영어로 작성.
    "description": "Reworks all base game zombies and adds 100+ new variants.",
    // 모드의 라이선스. 비-FOSS는 (일반적으로) "All Rights Reserved" 또는 "Source Available"을 사용해야 합니다.
    "license": "CC-BY-SA 4.0 Unported",
    // 모드의 원 제작자.
    "authors": ["That Guy", "His Friend"],
    // 제작자가 어떤 이유로 모드를 포기한 경우, 이 항목에 현재 유지보수자를 나열합니다.
    "maintainers": ["Mr. BugFixer", "BugFixer Jr."],
    // 모드 버전 문자열. 이것은 사용자와 유지보수자의 편의를 위한 것이므로, 가장 편리한 형식(예: 날짜)을 사용할 수 있습니다.
    "version": "2021-12-02",
    // 모드의 의존성 목록. 의존성은 모드가 로드되기 전에 로드되는 것이 보장됩니다.
    "dependencies": ["bn", "zed_templates"],
    // 이 모드와 호환되지 않는 모드의 목록.
    "conflicts": ["worse_zeds"],
    // 핵심 게임 데이터를 위한 특별한 플래그, 전체 개편 모드만 사용할 수 있습니다. 한 번에 하나의 핵심 모드만 로드할 수 있습니다.
    "core": false,
    // 모드를 구식으로 표시합니다. 구식 모드는 기본적으로 모드 선택 목록에 표시되지 않으며, 경고가 표시됩니다.
    "obsolete": false,
    // modinfo.json 파일을 기준으로 한 모드 파일의 경로. 게임은 자동으로 modinfo.json이 있는 폴더와
    // 모든 하위 폴더의 파일을 로드하므로, 이 필드는 어떤 이유로 modinfo.json을 모드의 하위 폴더에 넣고 싶을 때만 유용합니다.
    "path": "../common-data/"
  }
]
```
