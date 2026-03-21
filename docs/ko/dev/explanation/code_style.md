# C++ 코드 스타일 가이드

프로젝트의 모든 C++ 코드는 스타일이 지정되어 있으므로, 풀 리퀘스트를 푸시하기 전에 astyle을 실행해야 합니다.

astyle 버전 3.1을 사용합니다. 버전 3.0.1은 몇 군데만 차이가 있지만, 버전 3.6.6은 거의 모든 파일에서 차이가 발생합니다.

코드 블록을 astyle에 통과시켜 형식이 올바른지 확인할 수 있습니다:

```sh
astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

이러한 옵션은 `.astylerc`, `doc/CODE_STYLE.txt` 및 `msvc-full-features/AStyleExtension-Cataclysm-BN.cfg`에 반영되어 있습니다.

예를 들어 `vi`에서 블록 주위에 a와 b 마크를 설정한 다음:

```sh
:'a,'b ! astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

다른 환경에 대해서는 [DEVELOPER_TOOLING.md](../reference/tooling)를 참조하세요.

## 코드 예제

다음은 가장 일반적인 스타일 포인트를 보여주는 예제입니다:

```cpp
int foo( int arg1, int *arg2 )
{
    if( arg1 < 5 ) {
        switch( *arg2 ) {
            case 0:
                return arg1 + 5;
                break;
            case 1:
                return arg1 + 7;
                break;
            default:
                return 0;
                break;
        }
    } else if( arg1 > 17 ) {
        int i = 0;
        while( i < arg1 ) {
            printf( _( "Really long message that's pointless except for the number %d and for its "
                       "length as it's illustrative of how to break strings properly.\n" ), i );
        }
    }
    return 0;
}
```

## 코드 가이드라인

다음은 일반적인 가이드라인이라기보다는 시간이 지나면서 마주친 문제점들입니다.

- 불변 값을 선호하고 변수를 `const`로 선언하세요. 변경 가능한 부분이 적을수록 코드 흐름을 예측하기 쉽습니다.
- `int`를 선호하세요.
  - 특히 `long`은 문제가 있습니다. 일부 플랫폼에서는 int보다 큰 타입이 _아닙니다_.
  - 32비트보다 큰 정수 값 사용은 피해야 합니다. 정말 필요하다면 `int64_t`를 사용하세요.
  - `uint`도 문제가 있습니다. 오버플로 시 바람직하지 않은 동작을 하므로 범용 프로그래밍에서는 피해야 합니다.
    - 바이너리 데이터가 필요하다면 `unsigned int`나 `unsigned char`를 사용해도 되지만, `std::bitset`을 사용하는 것이 좋습니다.
  - `float`는 피해야 하지만 유효한 사용 사례가 있습니다.
- 적절한 경우 [`auto` 키워드](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp?view=msvc-170)를 사용하세요. 예를 들어:
  - 함수 선언에서 [후행 반환 타입](https://en.wikipedia.org/wiki/Trailing_return_type)을 선호하세요. 긴 반환 타입은 함수 이름을 가리고 클래스 메서드 읽기를 어렵게 만듭니다.
  ```cpp
  class Bar;
  auto foo( int a ) -> int
  {
      const Bar &bar = some_function();

      return is_bar_ok( bar ) ? 42 : 404;
  }
  ```
  - `decltype` 스타일 제네릭 함수에 사용
  ```diff
  template<typename A, typename B>
  - decltype(std::declval<A&>() * std::declval<B&>()) multiply(A a, B b)
  + auto multiply( A a, B b ) -> decltype( a * b )
  {
      return a*b;
  }
  ```
  - 긴 이터레이터 선언에 대한 별칭
  ```diff
    std::map<int, std::map<std::string, some_long_typename>> some_map;

  - std::map<int, std::map<std::string, some_long_typename>>::iterator iter = some_map.begin();
  + auto iter = some_map.begin();
  ```
  - 람다 선언에 필수
  ```cpp
  auto two_times = []( int a ) { return a * 2; };
  ```
  - 편의를 위해 가독성을 희생하지 않는 경우. [vscode](https://github.com/clangd/vscode-clangd)와 같은 인기 있는 코드 에디터에서 인라인 타입 힌트 옵션을 사용할 수 있습니다.

- 표준 네임스페이스에 `using namespace` 사용을 피하세요.
- 필요하지 않다면 클래스에 새 멤버 메서드를 추가하지 마세요.
  ```diff
  // 이 함수는 클래스의 비공개 데이터 멤버나 멤버 메서드에 접근하지 않으므로 자유 함수로 만들 수 있습니다
  - std::string Character::profession_description() const
  - {
  -     return prof->description( male );
  - }
  + auto profession_description( const Character &c ) -> std::string
  + {
  +     return c.prof->description( c.male );
  + }
  ```
