# C++20 Range 호환성

`std::ranges` 알고리즘을 사용하려면 이터레이터가 `std::default_initializable`, `std::copyable`(대입 가능)을 만족하고 후위 증가 연산을 제공해야 합니다.

## 가이드라인

- **복사 대입 활성화**: `const Container&` 멤버를 `const Container*`로 바꾸세요. 참조는 대입 연산자를 삭제하지만 포인터는 그렇지 않습니다.
- **기본 생성 활성화**: `iterator() = default`를 제공하고 멤버 포인터를 `nullptr`로 초기화하세요.
- **개념 정의**: `using iterator_concept = std::forward_iterator_tag;`를 추가하세요.
- **후행 반환 타입 적용**: 시그니처를 `auto fn() -> Type`으로 바꾸세요.
- **후위 증가 구현**: 표준 개념(`it++`)에 필요합니다.
- **비교 현대화**: `operator==`를 구현하세요. `operator!=`는 자동으로 합성되므로 제거하세요. Random Access 이터레이터에만 `operator<=>`를 구현하세요.
- **뷰 인터페이스 활용**: `std::ranges::view_interface`를 상속하여 `empty()`, `front()`, `back()`, `data()`를 자동으로 생성하세요.

## 리팩터링 diff

```diff
- template<typename Tripoint>
- class tripoint_range
+ template<typename Tripoint>
+ class tripoint_range : public std::ranges::view_interface<tripoint_range<Tripoint>>
  {
-         const tripoint_range &range;
+         const tripoint_range *range = nullptr;

      public:
+         using iterator_concept = std::forward_iterator_tag;

-         point_generator( const Tripoint &_p, const tripoint_range &_range )
-             : p( _p ), range( _range ) {
-         }
+         point_generator() = default;
+         point_generator( const Tripoint &_p, const tripoint_range *_range )
+             : p( _p ), range( _range ) {}

-         point_generator &operator++() {
+         auto operator++() -> point_generator& {
              // ... logic ...
              return *this;
          }

+         auto operator++(int) -> point_generator { auto tmp = *this; ++(*this); return tmp; }

-         const Tripoint &operator*() const {
-             return p;
-         }
+         auto operator*() const -> reference { return p; }

-         bool operator!=( const point_generator &other ) const {
-             const Tripoint &pt = other.p;
-             return traits::z( p ) != traits::z( pt ) || p.xy() != pt.xy();
-         }
-
-         bool operator==( const point_generator &other ) const {
-             return !( *this != other );
-         }
+         // C++20은 ==에서 !=를 합성합니다.
+         // Tripoint가 ==를 지원하면 default를 사용하고, 아니면 수동으로 ==를 구현하세요.
+         auto operator==( const point_generator &rhs ) const -> bool { return p == rhs.p; }
  };
```

## 검증

준수 여부를 확인할 수 있도록 정적 어설션을 추가하세요.

```cpp
static_assert( std::forward_iterator<tripoint_range<Tripoint>::iterator> );
static_assert( std::ranges::forward_range<tripoint_range<Tripoint>> );
```
