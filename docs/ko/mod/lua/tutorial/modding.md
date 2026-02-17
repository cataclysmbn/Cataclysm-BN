# Lua로 모딩하기

## 유용한 링크

- [Lua 5.3 참조 매뉴얼](https://www.lua.org/manual/5.3/)
- [Sol2 문서](https://sol2.readthedocs.io/en/latest/)
- [Programming in Lua (1판)](https://www.lua.org/pil/contents.html)

## 예제 모드

여기에 설명된 Lua API를 사용하는 주석이 많이 달린 예제 모드가 `data/mods/`에 몇 개 있습니다:

- `smart_house_remotes` - 차고 문과 창문 커튼을 제어하기 위한 리모컨 추가.
- `saveload_lua_test` - Lua 저장/로드 API 테스트용 모드.

## 게임 내 Lua 콘솔

게임 내 Lua 콘솔은 디버그 메뉴를 통해 또는 `Lua Console` 단축키(기본적으로 할당되지 않음)를 통해 사용할 수 있습니다.

다소 간단하지만 입력 기록을 유지하고 Lua 스크립트의 출력과 오류를 표시하며 Lua 스니펫을 실행하고 반환된 값을 출력할 수 있습니다.

`gdebug.set_log_capacity( num )`를 실행하여 콘솔 로그 용량을 조정하거나(기본값은 100개 항목) `gdebug.clear_lua_log()`를 실행하여 지울 수 있습니다.

## Lua 핫 리로드

모드 개발 프로세스를 가속화하기 위해 BN은 Lua 핫 리로드 기능을 지원합니다.

파일 시스템 감시자가 없으므로 핫 리로드는 해당하는 `Reload Lua Code` 단축키(기본적으로 할당되지 않음)를 통해 수동으로 트리거해야 합니다. 핫 리로드는 해당 단축키를 눌러 콘솔 창에서도 트리거하거나 `gdebug.reload_lua_code()` 명령을 실행하여 트리거할 수 있습니다. 일반 Lua 스크립트에서 명령을 실행하면 의도하지 않은 결과가 발생할 수 있으므로 자신의 책임하에 사용하세요!

모든 코드를 핫 리로드할 수 있는 것은 아니며 이후 섹션에서 설명됩니다.

## 게임 데이터 로딩

월드가 로드될 때 게임은 대략 다음 단계를 수행합니다:

1. 월드 관련 내부 상태를 초기화하고 월드를 활성으로 설정
2. 월드의 아티팩트 아이템 타입 로드(아티팩트는 해킹이며 유물 + Lua를 위해 곧 제거될 가능성이 높음)
3. 월드에서 사용하는 모드 목록 검색
4. 목록에 따라 모드 로드
5. 아바타 관련 내부 상태 초기화
6. 저장 디렉토리에서 실제 오버맵 데이터, 아바타 데이터 및 현실 버블 데이터 로드

여기서 중요한 것은 모드 로딩 단계입니다. 여러 하위 단계가 있습니다:

1. 로딩 함수가 월드 모드 목록을 받음
2. 누락된 것을 버리고 각각에 대해 디버그 메시지 출력
3. 목록의 나머지 모드를 확인하고 모드가 Lua를 필요로 하지만 게임 빌드가 Lua를 지원하지 않으면 오류 발생
4. 게임의 Lua API 버전이 모드에서 사용하는 버전과 다른 경우에도 경고 발생
5. Lua를 사용하는 목록의 모든 모드에 대해 모드의 [`preload.lua`](#preloadlua) 스크립트 실행(있는 경우)
6. 목록과 동일한 순서로 모든 모드를 검토하고 각 모드 폴더에서 JSON 정의 로드
7. 로드된 데이터 최종화(copy-from 해결, 복잡한 상태를 가진 일부 타입을 사용할 수 있도록 준비)
8. Lua를 사용하는 목록의 모든 모드에 대해 모드의 [`finalize.lua`](#finalizelua) 스크립트 실행(있는 경우)
9. 로드된 데이터의 일관성 확인(값 검증, 이상한 값 조합에 대한 경고 등)
10. (R) Lua를 사용하는 목록의 모든 모드에 대해 모드의 [`main.lua`](#mainlua) 스크립트 실행(있는 경우)

따라서 모드의 Lua 코드를 배치할 수 있는 스크립트는 [`preload.lua`](#preloadlua), [`finalize.lua`](#finalizelua), [`main.lua`](#mainlua) 3개뿐입니다. 이 3개의 차이점과 의도된 사용 사례는 아래에 설명됩니다.

필요에 따라 하나, 둘 또는 세 개의 스크립트만 사용할 수 있습니다.

핫 리로드를 실행할 때 게임은 (R)로 표시된 단계를 반복합니다. 즉, 작업 중인 코드를 핫 리로드 가능하게 하려면 [`main.lua`](#mainlua)에 넣어야 합니다.

### `preload.lua`

이 스크립트는 이벤트 훅을 등록하고 게임 JSON 로딩 시스템이 참조할 정의를 설정해야 합니다(예: 아이템 사용 액션). 여기에 등록하고 나중 단계(예: [`main.lua`](#mainlua)에서 정의하여 핫 리로드가 훅에 영향을 미치도록 할 수 있습니다.

### `finalize.lua`

이 스크립트는 copy-from이 해결된 후 JSON에서 로드된 정의를 모드가 수정할 수 있도록 해야 하지만 아직 API가 없습니다.

TODO: 최종화를 위한 api

### `main.lua`

이 스크립트는 모드의 주요 로직을 구현해야 합니다. 여기에는 다음이 포함되지만 이에 국한되지 않습니다:

1. 모드 런타임 상태
2. 게임 시작 시 모드 초기화
3. 필요한 경우 모드 저장/로드 코드
4. [`preload.lua`](#preloadlua)에서 설정된 훅의 구현

## Lua API 세부 사항

바닐라 Lua로 많은 흥미로운 것을 할 수 있지만 통합에는 잠재적 버그를 방지하기 위한 몇 가지 제한이 있습니다:

- 패키지(또는 Lua 모듈) 로딩은 `data/lua/` 및 `data/mods/<mod_id>/` 디렉토리로 제한됩니다.
- 현재 모드 id는 `game.current_mod` 변수에 저장됩니다
- 모드의 런타임 상태는 `game.mod_runtime[ game.current_mod ]` 테이블에 있어야 합니다. `game.mod_runtime[ that_other_mod_id ]`로 다른 모드의 런타임 상태에 액세스하여 해당 id를 알고 있으면 다른 모드와 인터페이스할 수도 있습니다
- 전역 상태에 대한 변경 사항은 스크립트 간에 사용할 수 없습니다. 이것은 함수 이름과 변수 이름 간의 우발적인 충돌을 방지하기 위한 것입니다. 여전히 전역 변수와 함수를 정의할 수 있지만 모드에만 표시됩니다.

### `require`로 모듈 로딩

`require` 함수는 모드 코드를 구성하기 위한 여러 가져오기 패턴을 지원합니다:

#### 상대 가져오기

현재 파일을 기준으로 모듈 로드:

```lua
-- data/mods/my_mod/main.lua에서
local utils = require("./lib/utils")          -- lib/utils.lua 로드
local config = require("./config")            -- 같은 디렉토리에 있는 config.lua 로드

-- data/mods/my_mod/lib/foo.lua에서
local helper = require("./helper")            -- lib/helper.lua 로드
local parent_mod = require("../parent")       -- 모드 루트에서 parent.lua 로드
```

#### 표준 라이브러리 가져오기

`lib.` 또는 `bn.lib.` 접두사를 사용하여 `data/lua/lib/`에서 공유 라이브러리 로드:

```lua
-- penlight가 data/lua/lib/pl/에 설치되어 있다고 가정
local pl_utils = require("lib.pl.utils")      -- data/lua/lib/pl/utils.lua 로드
local pl_path = require("bn.lib.pl.path")     -- 동일, bn.lib. 접두사도 작동
```

#### 모드 로컬 절대 가져오기

점 표기법(접두사 없음)을 사용하여 모드 디렉토리에서 모듈 로드:

```lua
-- data/mods/my_mod/main.lua에서
require("foo.bar.baz")  -- 다음을 검색:
                        -- 1. <mod>/foo/bar/baz.lua
                        -- 2. <mod>/foo/bar/baz/init.lua
```

#### 검색 순서

- **상대 (`./`, `../`)**: 현재 파일의 디렉토리
- **표준 라이브러리 (`lib.*`, `bn.lib.*`)**: `data/lua/lib/`만
- **모드 로컬 (접두사 없음)**: 현재 모드 디렉토리만

> [!CAUTION]
> 네임스페이스 충돌을 방지하기 위해 예약된 접두사(`lib`, `bn`)로 모드 모듈 이름을 지정하지 마세요.
> 대신 설명적인 모드별 이름을 사용하세요(예: `mymod.core`, `utils`).

#### 모듈 구조

모듈은 테이블을 반환해야 합니다:

```lua
-- lib/math_helper.lua
local M = {}

M.add = function(a, b)
  return a + b
end

return M
```

그런 다음 다른 파일에서 사용:

```lua
local math_helper = require("./lib/math_helper")
local result = math_helper.add(2, 3)
```

### Lua 라이브러리 및 함수

스크립트가 호출되면 일부 표준 Lua 라이브러리가 미리 로드됩니다:

| 라이브러리 | 설명                            |
| ---------- | ------------------------------- |
| `base`     | print, assert 및 기타 기본 함수 |
| `math`     | 모든 수학 관련                  |
| `string`   | 문자열 라이브러리               |
| `table`    | 테이블 조작자 및 관찰자 함수    |
| `package`  | `require`로 모듈 로드           |

자세한 내용은 Lua 매뉴얼의 `Standard Libraries` 섹션을 참조하세요.

여기의 일부 함수는 BN에 의해 오버로드됩니다. 자세한 내용은 [전역 오버라이드](#전역-오버라이드)를 참조하세요.

### 전역 상태

필요한 대부분의 데이터와 게임 런타임 상태는 전역 `game` 테이블을 통해 사용할 수 있습니다. 다음 멤버가 있습니다:

| 변수                        | 설명                                                                   |
| --------------------------- | ---------------------------------------------------------------------- |
| `game.current_mod`          | 로드 중인 모드의 Id (스크립트가 실행될 때만 사용 가능)                 |
| `game.active_mods`          | 로드 순서대로 활성 월드 모드 목록                                      |
| `game.mod_runtime.<mod_id>` | 모드의 런타임 데이터 (각 모드는 해당 id로 명명된 자체 테이블을 가져옴) |
| `game.mod_storage.<mod_id>` | 게임 저장/로드 시 자동으로 저장/로드되는 모드별 저장소                 |
| `game.cata_internal`        | 내부 게임 목적용, 사용하지 마세요                                      |
| `game.hooks.<hook_id>`      | Lua 스크립트에 노출된 훅, 해당 이벤트에서 호출됨                       |
| `game.iuse.<iuse_id>`       | 아이템 팩토리가 인식하고 아이템 사용 시 호출하는 아이템 사용 함수      |

### 게임 바인딩

게임은 다양한 함수, 상수 및 타입을 Lua에 노출합니다. 함수와 상수는 조직적 목적을 위해 "라이브러리"로 구성됩니다. 타입은 전역적으로 사용할 수 있으며 멤버 함수와 필드를 가질 수 있습니다.

전체 함수, 상수 및 타입 목록을 보려면 `--lua-doc` 명령줄 인수로 게임을 실행하세요. 이렇게 하면 `config` 폴더에 배치될 문서 파일 `lua_doc.md`가 생성됩니다.

#### 전역 오버라이드

일부 함수는 게임과의 통합을 개선하기 위해 전역적으로 오버라이드되었습니다.

| 함수       | 설명                                                                              |
| ---------- | --------------------------------------------------------------------------------- |
| print      | debug.log에 `INFO LUA`로 출력 (기본 Lua print 오버라이드)                         |
| require    | 상대(`./foo`, `../bar`) 및 절대(`pl.utils`) 가져오기 지원                         |
| package    | 사용자 정의 검색기가 보안 검사를 통해 `data/lua/`, `data/mods/<mod_id>/`에서 로드 |
| dofile     | 비활성화됨                                                                        |
| loadfile   | 비활성화됨                                                                        |
| load       | 비활성화됨                                                                        |
| loadstring | 비활성화됨                                                                        |

TODO: dofile 등의 대안

#### 훅

훅 목록을 보려면 자동 생성된 문서 파일의 `hooks_doc` 섹션을 확인하세요. 거기에서 훅 id 목록과 예상하는 함수 시그니처를 볼 수 있습니다.

참조: [`hooks.md`](../hooks.md) (우선순위 순서, 새 등록 형식, 체인).

`game.add_hook`을 사용하여 새 훅을 등록할 수 있습니다:

```lua
-- preload.lua에서
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_game_save", function(...)
  -- 이것은 본질적으로 전방 선언입니다.
  -- 훅이 존재한다고 선언하고 game_save 이벤트에서 호출되어야 하지만
  -- 모든 가능한 인수를 전달하고(없는 경우에도) 나중에 선언할 함수에서 값을 반환합니다.
  return mod.my_awesome_hook(...)
end)

-- main.lua에서
local mod = game.mod_runtime[game.current_mod]

mod.my_awesome_hook = function()
  -- 여기서 실제 작업 수행
end
```

#### 아이템 사용 함수

아이템 사용 함수는 고유 id를 사용하여 아이템 팩토리에 자신을 등록합니다. 아이템 활성화 시 아래 예시에 설명된 여러 인수를 받습니다.

```lua
-- preload.lua에서
local mod = game.mod_runtime[ game.current_mod ]
game.iuse_functions[ "SMART_HOUSE_REMOTE" ] = function(...)
  -- 이것은 단지 전방 선언이지만
  -- JSON에서 SMART_HOUSE_REMOTE iuse를 사용할 수 있게 해줍니다.
  return mod.my_awesome_iuse_function(...)
end

-- main.lua에서
local mod = game.mod_runtime[ game.current_mod ]
mod.my_awesome_iuse_function = function( who, item, pos )
  -- 여기서 실제 활성화 효과 수행
  -- `who`는 아이템을 활성화한 캐릭터
  -- `item`은 아이템 자체
  -- `pos`는 아이템의 위치 (캐릭터가 아이템을 가지고 있으면 캐릭터 위치와 같음)
end
```

#### 번역 함수

모드를 다른 언어로 번역 가능하게 하려면 `locale` 라이브러리에 바인딩된 함수를 통해 텍스트를 가져오세요. C++ 대응에 대한 자세한 설명은 [번역 API](../explanation/lua_integration.md)를 참조하세요.

사용 예시는 아래에 나와 있습니다:

```lua
-- 간단한 문자열.
--
-- "Experimental Lab" 텍스트는 스크립트에 의해 이 코드에서 추출되며
-- 번역자가 사용할 수 있습니다.
-- Lua 스크립트가 실행되면 이 함수는 "Experimental Lab" 문자열의 번역을 검색하고
-- 번역이 발견되면 번역된 문자열을 반환하고, 번역이 없으면 원래 문자열을 반환합니다.
local location_name_translated = locale.gettext( "Experimental Lab" )

-- 오류: 문자열 리터럴로 `gettext`를 호출해야 합니다.
-- 이렇게 호출하면 "Experimental Lab"이 추출되지 않으며
-- 번역자는 텍스트를 번역할 때 이것을 보지 못합니다.
local location_name_original = "Experimental Lab"
local location_name_translated = locale.gettext( location_name_original )

-- 오류: 함수를 다른 이름으로 별칭하지 마세요.
-- 이렇게 호출하면 "Experimental Lab"이 추출되지 않으며
-- 번역자는 텍스트를 번역할 때 이것을 보지 못합니다.
local gettext_alt = locale.gettext
local location_name_translated = gettext_alt( "Experimental Lab" )

-- 그러나 이것은 괜찮습니다.
local gettext = locale.gettext
local location_name_translated = gettext( "Experimental Lab" )

-- 가능한 복수형이 있는 문자열.
-- 많은 언어에는 사용할 것을 결정하는 복잡한 규칙과 함께 2개 이상의 복수형이 있습니다.
local item_display_name = locale.vgettext( "X-37 Prototype", "X-37 Prototypes", num_of_prototypes )

-- 컨텍스트가 있는 문자열
local text_1 = locale.pgettext("the one made of metal", "Spring")
local text_2 = locale.pgettext("the one that makes water", "Spring")
local text_3 = locale.pgettext("time of the year", "Spring")

-- 컨텍스트와 복수형이 모두 있는 문자열.
local item_display_name = locale.vpgettext("the one made of metal", "Spring", "Springs", num_of_springs)

--[[
  어떤 텍스트가 까다롭고 설명이 필요할 때
  번역자를 돕기 위해 `~`로 표시된 특별한 주석을 배치하는 것이 일반적입니다.
  주석은 함수 호출 바로 위에 있어야 합니다.
]]

--~ 이 주석은 좋으며 번역자에게 표시됩니다.
local ok = locale.gettext("Confusing text that needs explanation.")

--~ 오류: 이 주석은 gettext 호출에서 너무 멀리 떨어져 있어 추출되지 않습니다!
local not_ok = locale.
                gettext("Confusing text that needs explanation.")

local not_ok = locale.gettext(
                  --~ 오류: 이 주석은 잘못된 위치에 있어 추출되지 않습니다!
                  "Confusing text that needs explanation."
                )

--[[~
  오류: 여러 줄 Lua 주석은 번역자 주석으로 사용할 수 없습니다!
  이 주석은 추출되지 않습니다!
]] 
local ok = locale.gettext("Confusing text that needs explanation.")

--~ 여러 줄 번역자 주석이 필요한 경우
--~ 2개 이상의 한 줄 주석을 사용하세요.
--~ 연결되어 단일 여러 줄 주석으로 표시됩니다.
local ok = locale.gettext("Confusing text that needs explanation.")
```
