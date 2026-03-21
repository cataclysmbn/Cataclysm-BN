# Cataclysm 테스트

소스에서 `make`로 Cataclysm을 빌드하면 `tests/` 디렉토리의 테스트 케이스로부터 실행 파일 `tests/cata_test`가 빌드됩니다. 테스트는 [Catch2 프레임워크](https://github.com/catchorg/Catch2)로 작성되었습니다.

`tests/cata_test --help`를 실행하면 사용 가능한 명령줄 옵션을 확인할 수 있고, [Catch2 튜토리얼](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)에서 더 자세한 내용을 볼 수 있습니다.

## 가이드라인

테스트를 작성할 때는 (직간접적으로) 사용되는 모든 객체가 테스트 전에 완전히 초기화되도록 해야 합니다. 여러 테스트가 랜덤 생성된 객체의 속성이나 전역 객체 (주로 플레이어 객체)를 통한 테스트 간 상호작용으로 인해 불안정해졌습니다. 일반적으로 테스트 케이스는 독립적이어야 합니다 (한 테스트가 다른 테스트의 출력에 의존하지 않아야 함).

json 정의가 있는 객체를 생성할 때는 REQUIRE 문으로 테스트에 필요한 객체 속성을 검증하세요. 이렇게 하면 json 정의가 변경될 때 어떤 속성 변경으로 테스트가 깨졌는지 명확히 알 수 있어 테스트를 보호할 수 있습니다.

## 테스트 케이스 작성

테스트를 구성하고 표현하는 여러 방법이 있지만, 기본 단위는 `TEST_CASE`입니다. 각 테스트 `.cpp` 파일은 이름과 선택적 (하지만 강력히 권장되는) 태그 목록을 가진 최소 하나의 테스트 케이스를 정의해야 합니다:

```cpp
TEST_CASE( "sweet junk food", "[food][junk][sweet]" )
{
    // ...
}
```

`TEST_CASE` 내에서 Catch2 프레임워크는 테스트의 관련 부분을 논리적으로 그룹화하는 여러 매크로를 제공합니다. 높은 가독성을 제공하는 방법 중 하나는 `GIVEN`, `WHEN`, `THEN` 섹션을 사용하는 [BDD](https://en.wikipedia.org/wiki/Behavior-driven-development) (행동 주도 개발) 스타일입니다. 다음은 이를 사용한 테스트 개요입니다:

```cpp
    TEST_CASE( "sweet junk food", "[food][junk][sweet]" )
    {
        GIVEN( "character has a sweet tooth" ) {

            WHEN( "they eat some junk food" ) {

                THEN( "they get a morale bonus from its sweetness" ) {
                }
            }
        }
    }
```

이러한 관점에서 생각하면 테스트 설정 및 테스트 데이터 초기화 (보통 `GIVEN` 부분), 테스트하려는 결과를 생성하는 작업 수행 (`WHEN` 부분), 결과가 예상과 일치하는지 검증 (`THEN` 부분)의 논리적 진행을 이해하는 데 도움이 됩니다.

위 내용을 실제 테스트 코드로 채우면 다음과 같습니다:

```cpp
    TEST_CASE( "sweet junk food", "[food][junk][sweet]" )
    {
        avatar dummy;
        dummy.clear_morale();

        GIVEN( "character has a sweet tooth" ) {
            dummy.toggle_trait( trait_PROJUNK );

            WHEN( "they eat some junk food" ) {
                item necco( "neccowafers" );
                dummy.eat( necco );

                THEN( "they get a morale bonus from its sweetness" ) {
                    CHECK( dummy.has_morale( MORALE_SWEETTOOTH ) >= 5 );
                }
            }
        }
    }
```

각 부분을 순서대로 살펴보겠습니다. 먼저 캐릭터 또는 플레이어를 나타내는 `avatar`를 선언합니다. 이 테스트는 플레이어의 사기를 확인할 것이므로 깨끗한 상태를 보장하기 위해 초기화합니다:

```cpp
avatar dummy;
dummy.clear_morale();
```

`GIVEN` 내부에는 캐릭터가 단 것을 좋아한다는 것을 구현하는 코드가 있습니다. 게임 코드에서 이것은 `PROJUNK` 특성으로 표현되므로 `toggle_trait`을 사용하여 설정할 수 있습니다:

```cpp
GIVEN( "character has a sweet tooth" ) {
    dummy.toggle_trait( trait_PROJUNK );
```

이제 `GIVEN`의 스코프 내부에 있습니다 - 이 `GIVEN`의 스코프 나머지 동안 `dummy`는 이 특성을 가지게 됩니다. 이 간단한 테스트에서는 몇 줄만 영향을 받지만, 테스트가 더 크고 복잡해지면 (그렇게 될 것입니다) 이러한 중첩 스코프와 테스트 간 오염을 피하는 방법을 인식해야 합니다.

이제 `dummy`가 단 것을 좋아하므로 단 것을 먹게 하겠습니다. `neccowafers` 아이템을 생성하고 먹도록 합니다:

```cpp
WHEN( "they eat some junk food" ) {
    dummy.eat( item( "neccowafers" ) );
```

이 시점에서 호출하는 함수가 종종 테스트의 초점이 됩니다. 목표는 코드가 실행되고 테스트로 커버되도록 해당 함수를 통과하는 경로를 실행하는 것입니다. `eat` 함수는 여기서 예제로 사용되지만, 그 자체로 많은 동작과 하위 동작을 가진 상위 수준의 복잡한 함수입니다. 이 테스트 케이스는 사기 효과에만 관심이 있으므로 더 나은 테스트는 `eat`이 호출하는 `modify_morale`과 같은 하위 수준 함수를 호출할 것입니다.

`dummy`가 `neccowafers`를 먹었지만 무언가 일어났을까요? 단 것을 좋아하기 때문에 `MORALE_SWEETTOOTH`라는 특정 사기 보너스를 받아야 하고, 그 크기는 최소 `5`여야 합니다:

```cpp
THEN( "they get a morale bonus from its sweetness" ) {
    CHECK( dummy.has_morale( MORALE_SWEETTOOTH ) >= 5 );
}
```

이 `CHECK` 매크로는 불린 표현식을 받아 false이면 테스트를 실패시킵니다. 마찬가지로 `CHECK_FALSE`를 사용할 수 있으며, 표현식이 true이면 실패합니다.

## Requiring vs Checking

`CHECK`와 `CHECK_FALSE` 매크로는 표현식의 참/거짓을 검증하지만 실패해도 테스트를 계속 진행합니다. 이를 통해 여러 `CHECK`를 수행하고 하나 이상이 예상과 다를 때 알림을 받을 수 있습니다.

다른 종류의 검증은 `REQUIRE` (및 대응 `REQUIRE_FALSE`)입니다. `CHECK` 검증과 달리 `REQUIRE`는 실패하면 계속 진행하지 않습니다 - 이 검증은 테스트를 계속하는 데 필수적입니다.

`REQUIRE`는 시스템 상태를 변경한 후 가정을 재확인하고 싶을 때 유용합니다. 예를 들어 단 것을 좋아하는 테스트에 몇 가지 `REQUIRE`를 추가하여 `dummy`가 실제로 원하는 특성을 가지고 있고 `neccowafers`가 실제로 정크 푸드인지 확인합니다:

```cpp
    GIVEN( "character has a sweet tooth" ) {
        dummy.toggle_trait( trait_PROJUNK );
        REQUIRE( dummy.has_trait( trait_PROJUNK ) );

        WHEN( "they eat some junk food" ) {
            item necco( "neccowafers" );
            REQUIRE( necco.has_flag( "ALLERGEN_JUNK" ) );

            dummy.eat( necco );

            THEN( "they get a morale bonus from its sweetness" ) {
                CHECK( dummy.has_morale( MORALE_SWEETTOOTH ) >= 5 );
            }
        }
    }
```

여기서 `REQUIRE`를 사용하는 이유는 이것들이 실패하면 테스트를 계속할 이유가 없기 때문입니다. 가정이 틀리면 그 이후의 모든 것이 유효하지 않습니다. 명확히 말하자면, `toggle_trait`이 캐릭터에게 `PROJUNK` 특성을 주지 못했거나 `neccowafers`가 실제로 설탕으로 만들어지지 않았다면 사기 보너스 테스트는 의미가 없습니다.

`REQUIRE`는 테스트의 전제 조건으로, `CHECK`는 테스트의 결과를 확인하는 것으로 생각할 수 있습니다.
