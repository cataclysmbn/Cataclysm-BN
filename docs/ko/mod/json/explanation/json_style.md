---
title: JSON 스타일 가이드
sidebar:
  badge:
    text: 불안정
    variant: caution
---

[C++ 코드 스타일](../../../dev/explanation/code_style.md)과 마찬가지로 JSON 스타일 정책은 개발에 불필요한 혼란을 주지 않도록 JSON을 추가하거나 편집할 때, 그렇지 않으면 비교적 작은 단위로 JSON을 업데이트하는 것입니다.

## 자체 JSON 포매터를 사용하는 이유

DDA는 자체 JSON 파서를 작성했습니다. 파서는 `tools/format/format.cpp`에 있으며 `src/json.cpp`을 이용해 JSON을 파싱하고 출력합니다.

이 방식은 기존 JSON 포매터(예: `deno fmt`)를 사용할 수 없게 하므로 최적의 해법은 아니지만, [이전 시도](https://github.com/cataclysmbn/Cataclysm-BN/pull/3118)에서 단점이 장점보다 큰 것으로 확인되었습니다.

## JSON 예시

다음 예시는 대부분의 스타일 기능을 보여 줍니다.

```json
[
  {
    "type": "foo",
    "id": "example",
    "short_array": [1, 2, 3, 4, 5],
    "short_object": {
      "item_a": "a",
      "item_b": "b"
    },
    "long_array": [
      "a really long string to illustrate line wrapping, ",
      "which occurs if the line is longer than 120 characters"
    ],
    "nested_array": [
      [
        ["item1", "value1"],
        ["item2", "value2"],
        ["item3", "value3"],
        ["item4", "value4"],
        ["item5", "value5"],
        ["item6", "value6"]
      ]
    ]
  }
]
```

들여쓰기는 2칸입니다. 쉼표와 콜론을 제외한 모든 JSON 구분자는 공백(스페이스 또는 줄바꿈)으로 둘러쌉니다. 쉼표와 콜론 뒤에는 공백이 옵니다. 객체 항목은 항상 줄바꿈으로 구분합니다. 배열 항목은 들여쓰기를 포함한 결과가 120자를 넘는 경우 줄바꿈으로 구분합니다. 여는 괄호, 닫는 괄호 또는 항목 뒤에 줄바꿈이 옵니다.

## 포매터

포매터는 CMake의 `style-json` 타깃, 직접 실행하는 `tools/format/json_formatter.cgi`, 또는 CGI <http://dev.narc.ro/cataclysm/format.html>로 호출할 수 있습니다.

Visual Studio 솔루션을 사용한다면 프로젝트의 모든 JSON을 포맷하도록 Visual Studio 명령을 설정할 수 있습니다.

1. 전체 솔루션 또는 JsonFormatter 프로젝트만 빌드하여 `tools/format/json_formatter.exe` 바이너리를 만듭니다.
2. 외부 도구 항목(`Tools` > `External Tools..` > `Add`)을 추가하고 다음처럼 설정합니다.
   - 제목: `Lint All JSON`
   - 명령: `C:\windows\system32\windowspowershell\v1.0\powershell.exe`
   - 인수: `-file $(SolutionDir)\style-json.ps1`
   - 초기 디렉토리: `$(SolutionDir)`
   - 출력 창 사용: 선택

이제 `Tools` > `Lint All JSON`에서 명령을 실행하고 출력 창에서 결과를 볼 수 있습니다. `Tools` > `Options` > `Environment` > `Keyboard`에서 `Tools.ExternalCommand`가 포함된 명령을 검색하면 목록에서 해당 위치에 맞는 명령(예: 맨 위라면 `Tools.ExternalCommand1`)을 선택해 키 바인딩을 지정할 수도 있습니다.

### 단일 파일

`json_formatter.exe path/to/file.json`을 실행하면 JSON 파일 하나를 포맷할 수 있습니다. `needs linting`이 출력되면 파일이 포맷되지 않았다는 뜻이며 이제 포맷된 상태가 됩니다. `json_formatter` 아이콘에 JSON 파일을 드래그 앤 드롭해도 됩니다.

### *nix

저장소의 기본 디렉토리에서 `just fmt-json`를 실행합니다. `just`를 사용할 수 없다면 대신 `build-scripts/format-json.sh`를 실행합니다.
