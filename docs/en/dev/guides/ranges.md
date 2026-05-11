# C++20 Range Compatibility

To enable `std::ranges` algorithms, iterators must satisfy `std::default_initializable`, `std::copyable` (assignable), and provide postfix increment.

## Guidelines

- **Enable Copy Assignment**: Replace `const Container&` members with `const Container*`. References delete the assignment operator; pointers do not.
- **Enable Default Construction**: Provide `iterator() = default` and initialize member pointers to `nullptr`.
- **Define Concepts**: Add `using iterator_concept = std::forward_iterator_tag;`.
- **Enforce Trailing Return Types**: Convert signatures to `auto fn() -> Type`.
- **Implement Postfix Increment**: Required by standard concepts (`it++`).
- **Modernize Comparisons**: Implement `operator==`. Remove `operator!=` (synthesized automatically). Implement `operator<=>` only for Random Access iterators.
- **Utilize View Interface**: Inherit from `std::ranges::view_interface` to automatically generate `empty()`, `front()`, `back()`, and `data()`.

## Refactoring Diff

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
+         // C++20 synthesizes != from ==.
+         // If Tripoint supports ==, use default. Otherwise implement manual ==.
+         auto operator==( const point_generator &rhs ) const -> bool { return p == rhs.p; }
  };
```

## Verification

Add static assertions to ensure compliance.

```cpp
static_assert( std::forward_iterator<tripoint_range<Tripoint>::iterator> );
static_assert( std::ranges::forward_range<tripoint_range<Tripoint>> );
```
