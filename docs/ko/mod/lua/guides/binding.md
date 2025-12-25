# 새로운 타입 바인딩

### 내부를 바인딩하지 않고 문서 생성기에 새 타입 추가하기

C++ 타입이 문서 생성기에 등록되지 않았다면 `<cppval: **gibberish** >`로 표시됩니다. 이 문제를 완화하려면 `catalua_luna_doc.h`에 `LUNA_VAL( your_type, "YourType" )`을 추가하면 생성기가 인수 타입에 `YourType` 문자열을 사용합니다.

### Lua에 새 타입 바인딩하기

먼저 바인딩 시스템에 새 타입을 등록해야 합니다. 문서 생성기가 이해하고, 런타임이 해당 타입을 포함하는 Lua 테이블을 JSON에서 역직렬화할 수 있도록 하는 등 여러 이유로 필요합니다. 이를 하지 않으면 `Type must implement luna_traits<T>` 컴파일 오류가 발생합니다.

1. `catala_luna_doc.h`에 타입 선언을 추가합니다. 예를 들어, 가상의 `horde` 타입(`struct`)을 바인딩한다면 파일 상단 근처에 한 줄만 추가하면 됩니다:
   ```cpp
   struct horde;
   ```
   복잡한 템플릿 타입은 실제로 관련 헤더를 포함해야 할 수도 있지만, 컴파일 시간에 큰 영향을 주므로 가능한 피하세요.

2. 같은 파일에서 문서 생성기에 타입을 등록합니다. `horde` 예제를 계속하면 다음과 같이 합니다:
   ```cpp
   LUNA_VAL( horde, "Horde" );
   ```
   C++ 타입은 이름에 다양한 스타일을 사용하지만, Lua 측에서는 모두 `CamelCase`여야 합니다.

이제 실제 세부 사항으로 들어갈 수 있습니다. 바인딩은 `catalua_bindings*.cpp` 파일에 구현됩니다. 컴파일 속도를 높이고 탐색을 쉽게 하기 위해 여러 `.cpp` 파일로 나뉘어 있으므로, 기존 `catalua_bindings*.cpp` 파일에 넣거나 유사한 새 파일을 만들 수 있습니다. 같은 이유로 함수로도 나뉩니다. `horde` 타입을 등록하고 새 파일과 새 함수에 넣어봅시다:

1. `catalua_bindings.h`에 새 함수 선언을 추가합니다:
   ```cpp
   void reg_horde( sol::state &lua );
   ```
2. `catalua_bindings.cpp`의 `reg_all_bindings`에서 함수를 호출합니다:
   ```cpp
   reg_horde( lua );
   ```
3. 다음 내용으로 새 파일 `catalua_bindings_horde.cpp`를 만듭니다:
   ```cpp
   #include "catalua_bindings.h"

   #include "horde.h" // 타입이 정의된 헤더로 교체

   void cata::detail::reg_horde( sol::state &lua )
   {
       sol::usertype<horde> ut =
           luna::new_usertype<horde>(
               lua,
               luna::no_bases,
               luna::constructors <
                   // 실제 생성자를 여기에 정의
                   horde(),
                   horde( const point & ),
                   horde( int, int )
                   > ()
               );

       // 필요한 모든 멤버 등록
       luna::set( ut, "pos", &horde::pos );
       luna::set( ut, "size", &horde::size );

       // 필요한 모든 메서드 등록
       luna::set_fx( ut, "move_to", &horde::move_to );
       luna::set_fx( ut, "update", &horde::update );
       luna::set_fx( ut, "get_printable_name", &horde::get_printable_name );

       // 저장/로드 경계를 넘어 horde를 전달할 수 있도록
       // (역)직렬화 함수 추가
       reg_serde_functions( ut );

       // 산술 연산자, to_string 연산자 등 추가
   }
   ```
4. 완료입니다. 타입이 이제 `Horde`라는 이름으로 Lua에 표시되며, 바인딩된 메서드와 멤버를 사용할 수 있습니다.

### Lua에 새 타입 바인딩하기 (Neovim의 정규식 사용)

클래스/구조체를 Lua에 수동으로 바인딩하는 것은 꽤 지루할 수 있으므로, 헤더 파일을 변환하여 클래스를 바인딩하는 다른 방법도 있습니다. [이전](#lua에-새-타입-바인딩하기)의 두 번째 부분의 세 번째 단계에서는 Neovim의 내장 정규식과 C++ 매크로를 사용하여 클래스를 바인딩할 수 있습니다.

1. 클래스 정의의 사본을 만듭니다.
2. 둘 다 적용: `%s@class \([^{]\)\+\n*{@private:@` `%s@struct \([^{]\)\+\n*{@public:@`
3. 시작 부분의 생성자/원하지 않는 메서드를 수동으로 제거합니다.
4. 모든 `private`/`protected` 메서드 삭제: `%s@\(private:\|protected:\)\_.\{-}\(public:\|};\)@\2`
5. 클래스 정의 끝의 `};` 제거.
6. `public` 레이블 삭제: `%s@ *public:\n@`
7. 주석 삭제: `%s@\( *\/\*\_.\{-}\*\/\n\{,1\}\)\|\( *\/\/\_.\{-}\(\n\)\)@\3@g`
8. 기본 들여쓰기가 0이 될 때까지 들여쓰기 해제.
9. 메서드 정의를 선언으로 변환: `%s@ *{\(}\|\_.\{-}\n^}\)@;`
10. 대부분의 메서드 선언을 한 줄로 푸시: `%s@\((\|,\)\n *@\1@g`
11. 기본값 제거: `%s@ *= *\_.\{-}\( )\|;\|,\)@\1@g`
12. `overriden`/`static` 메서드/멤버와 `using`s 제거:
    `%s@.*\(override\|static\|using\).*\n@@g`
13. `template`s 제거: `%s@^template<.*>\n.*\n@@g`
14. `virtual` 태그 제거: `%s@^virtual *@`
15. 모든 줄이 세미콜론으로 끝나는지 확인: `%s@\([^;]\)\n@\0@gn`
16. 함수 개수 세기: `%s@\(.*(.*).*\)@@nc`
17. 첫 번째로 찾은 함수를 끝으로 푸시: `%s@\(.*(.*).*\)\n\(\n*\)\(\_.*\)@\3\1\2`
18. 이제 15단계의 일치 수에서 1을 뺀 만큼 16단계를 반복하려고 합니다. Neovim의 경우 일치 수에서 1을 뺀 값을 입력하고 '@', 그 다음 ':'를 입력합니다. 예: '217@:'는 마지막 명령을 217번 반복합니다.
19. 새 줄 정리: `%s@\n\{3,}@\r\r`
20. 메서드를 매크로로 래핑: `%s@\(.*\) \+\([^ ]\+\)\((.*\);@SET_FX_T( \2, \1\3 );`
21. 멤버를 매크로로 래핑; 먼저 영향을 받을 줄을 선택하세요:
    `s@.\{-}\([^ ]\+\);@SET_MEMB( \1 );`
22. 이전에 여러 줄이었던 메서드 선언을 다시 여러 줄로 만들기:
    `%s@\(,\)\([^ ]\)@\1\r        \2@g`

이제 남은 것은 텍스트 덩어리를 가져와서 Lua 바인딩에 사용하는 것입니다. horde 예제를 계속하면, 이 매크로를 사용한 코드는 다음과 같습니다:

```cpp
#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"

#include "horde.h" // 타입이 정의된 헤더로 교체

void cata::detail::reg_horde( sol::state &lua )
{
    #define UT_TYPE horde
    sol::usertype<UT_TYPE> ut =
    luna::new_usertype<UT_TYPE>(
        lua,
        luna::no_bases,
        luna::constructors <
            // 실제 생성자를 여기에 정의
            UT_TYPE(),
            UT_TYPE( const point & ),
            UT_TYPE( int, int )
            > ()
       );

    // 필요한 모든 멤버 등록
    SET_MEMB( pos );
    SET_MEMB( size );

    // 필요한 모든 메서드 등록
    SET_FX_T( move_to, ... ); // ... 대신 메서드의 타입 선언이 들어갑니다.
    SET_FX_T( update, ... );
    SET_FX_T( get_printable_name, ... );

    // 저장/로드 경계를 넘어 horde를 전달할 수 있도록
    // (역)직렬화 함수 추가
    reg_serde_functions( ut );

    // 산술 연산자, to_string 연산자 등 추가
    // ...
    #undef UT_TYPE // #define UT_TYPE horde
}
```

이 Lua 바인딩 방법은 템플릿 메서드 바인딩이 없으며 깨질 수 있습니다: 컴파일러 오류, 링커 프리즈 등. 따라서 이 바인딩이 기본적으로 깨져 있다고 가정하고, 약간의 수정/수동 추가만 필요하다고 생각하는 것이 좋습니다.

### Lua에 새 enum 바인딩하기

enum 바인딩은 타입 바인딩과 유사합니다. 가상의 `horde_type` enum을 바인딩해봅시다:

1. enum에 명시적으로 정의된 컨테이너(헤더에서 정의된 `enum name` 뒤의 `: type` 부분)가 없다면 먼저 컨테이너를 지정해야 합니다. 예:
   ```diff
     // hordes.h
   - enum class horde_type {
   + enum class horde_type : int {
       animals,
       robots,
       zombies
     }
   ```
2. `catalua_luna_doc.h`에 선언 추가
   ```cpp
   enum horde_type : int;
   ```
3. `catalua_luna_doc.h`에 다음으로 등록
   ```cpp
   LUNA_ENUM( horde_type, "HordeType" )
   ```
4. enum이 `std::string`으로/에서 자동 변환을 구현하는지 확인하세요. 자세한 내용은 `enum_conversions.h`를 참조하세요. 일부 enum에는 이미 있지만 대부분은 없습니다. 보통 헤더에서 enum `T`에 대해 `enum_traits<T>`를 특수화한 다음, `.cpp` 파일에서 enum -> 문자열 변환으로 `io::enum_to_string<T>`를 정의하는 것입니다. 일부 enum에는 `enum_traits<T>`에 필요한 "last" 값이 없습니다. 이 경우 하나를 추가해야 합니다:
   ```diff
     enum class horde_type : int {
       animals,
       robots,
   -   zombies
   +   zombies,
   +   num_horde_types
     }
   ```
   이것은 "단조" enum, 즉 0으로 시작하고 값을 건너뛰지 않는 enum에만 작동합니다. 위 예제에서 `animals`는 암묵적으로 `0`, robots는 암묵적으로 `1`, `zombies`는 암묵적으로 `2`이므로 올바르고 예상되는 암묵적 값 `3`을 가질 `num_horde_types`를 쉽게 추가할 수 있습니다.
5. `catalua_bindings.cpp`의 `reg_enums` 함수에서 enum 필드 바인딩:
   ```cpp
   reg_enum<horde_type>( lua );
   ```
   이것은 4단계의 자동 변환을 사용하므로 JSON과 Lua 사이에 동일한 이름을 가집니다.

### Lua에 새 `string_id<T>` 또는 `int_id<T>` 바인딩하기

이것들은 `T` 자체를 바인딩하는 것과 별도로 수행할 수 있습니다.

1. 아직 하지 않았다면 문서 생성기에 타입 `T`를 등록하세요 ([관련 문서](#내부를-바인딩하지-않고-문서-생성기에-새-타입-추가하기) 참조).
2. 1단계의 `LUNA_VAL`을 `LUNA_ID`로 교체합니다.
3. 타입 `T`가 연산자 `<`와 `==`를 구현하는지 확인하세요. 보통 수동으로 구현하기 쉬우며, `catalua_type_operators.h`에 있는 `LUA_TYPE_OPS` 매크로로 반자동으로 수행할 수 있습니다.
4. 타입 `T`에 null `string_id`가 있는지 확인하세요. 존재하지 않으면 `string_id_null_ids.cpp`에 추가할 수 있습니다. `T`가 class로 정의된 경우 `MAKE_CLASS_NULL_ID` 매크로를, 그렇지 않으면 `MAKE_STRUCT_NULL_ID` 매크로를 사용하세요.
5. 타입 `T`의 `string_id`에 `obj()` 및 `is_valid()` 메서드가 구현되어 있는지 확인하세요. 이 메서드들은 케이스별로 구현됩니다. 다른 `string_id`를 예제로 확인하는 것을 권장합니다.
6. `catalua_bindings_ids.cpp`에 타입 T가 정의된 헤더를 추가합니다:
   ```cpp
   #include "your_type_definition.h"
   ```
7. `reg_game_ids` 함수에서 다음과 같이 등록합니다:
   ```cpp
   reg_id<T, true>( lua );
   ```

`true`는 `string_id<T>`만 바인딩하고 `int_id<T>`에 관심이 없거나 구현할 수 없는 경우 `false`로 교체할 수 있습니다.

이 단계에서 `is_valid()` 또는 `NULL_ID()` 메서드에 대한 링커 오류가 발생할 수 있습니다. 이는 다양한 이유로 모든 string 또는 int id에 대해 구현되지 않았기 때문입니다. 이 경우 수동으로 정의해야 하며, 자세한 내용은 `string_id` 및 `int_id`에 대한 관련 문서를 참조하세요.

그게 전부입니다. 이제 타입 `T`는 `Raw` 접미사로, `string_id<T>`는 `Id` 접미사로, `int_id<T>`는 `IntId` 접미사로 Lua에 표시됩니다. 예를 들어 `LUNA_ID( horde, "Horde" )`의 경우 다음을 얻습니다:

- `horde` -> `HordeRaw`
- `string_id<horde>` -> `HordeId`
- `int_id<horde>` -> `HordeIntId`

세 가지 간의 모든 타입 변환은 시스템에 의해 자동으로 구현됩니다. `T`의 실제 필드와 메서드는 평소와 같은 방식으로 Lua에 바인딩할 수 있습니다.
