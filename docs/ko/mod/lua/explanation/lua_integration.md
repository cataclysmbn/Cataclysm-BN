# C++ Lua 통합

이 문서는 Cataclysm: Bright Nights에서 Lua 통합의 구현 세부 사항을 설명합니다.

BN은 스크립트 실행을 위해 Lua 5.3.6을 사용하며, C++ 측 바인딩은 sol2 v3.3.0에 의존합니다.

## C++ 레이아웃

### Lua 소스 파일

빌드 설정을 단순화하고 이식성을 향상시키기 위해 `Lua 5.3.6` 소스 코드를 `src/lua/` 디렉토리에 번들로 포함하고, 빌드 시스템이 이를 컴파일하여 게임 실행 파일과 테스트용 라이브러리에 링크하도록 합니다.

### Sol2 소스 파일

Sol2는 번들링이 쉽습니다. `sol2 v3.3.0` 단일 헤더 통합 버전이 `src/sol/`에 있으며 필요에 따라 포함하기만 하면 됩니다. 헤더가 상당히 크므로, 포함하는 소스 파일이 적을수록 좋습니다.

- `sol/config.hpp` - 설정 헤더, 몇 가지 옵션이 정의되어 있습니다.
- `sol/forward.hpp` - 전방 선언, `sol/sol.hpp` 대신 게임 헤더에 포함해야 하는 경량 헤더입니다.
- `sol/sol.hpp` - 메인 sol2 헤더 파일, 상당히 크므로 게임 헤더에 포함하는 것을 피하세요.

### 게임 소스 파일

모든 Lua 관련 게임 소스 파일은 `catalua` 접두사를 가집니다.

새로운 바인딩을 추가하려면 `src/catalua_bindings.cpp`의 기존 예제를 살펴보고 Sol2 문서의 관련 부분을 읽어보세요.

- `catalua.h` (및 `catalua.cpp`) - 메인 Lua 인터페이스. 코드베이스 대부분이 포함해야 하는 유일한 헤더이며, 공용 인터페이스를 제공합니다.
- `catalua_sol.h` 및 `catalua_sol_fwd.h` - 컴파일되도록 커스텀 프라그마를 포함한 `sol/sol.hpp` 및 `sol/forward.hpp`의 래퍼입니다.
- `catalua_bindings*` - 게임 Lua 바인딩이 여기에 있습니다.
- `catalua_console.h`(`.cpp`) - 게임 내 Lua 콘솔.
- `catalua_impl.h`(`.cpp`) - `catalua.h`(`.cpp`)의 구현 세부 사항.
- `catalua_iuse_actor.h`(`.cpp`) - Lua 기반 `iuse_actor`.
- `catalua_log.h`(`.cpp`) - 콘솔용 메모리 내 로깅.
- `catalua_luna.h` - 자동 문서 생성이 포함된 유저타입 등록 인터페이스, 일명 `luna`.
- `catalua_luna_doc.h` - `luna`를 통해 등록되거나 문서 생성기에 노출된 타입 목록.
- `catalua_readonly.h`(`.cpp`) - Lua 테이블을 읽기 전용으로 표시하는 함수.
- `catalua_serde.h`(`.cpp`) - Lua 테이블을/에서 JSON (역)직렬화.
- `catalua_type_operators.h` - string_id 바인딩 구현을 돕는 매크로.
