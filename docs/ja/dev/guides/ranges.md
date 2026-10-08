# C++20 Range の互換性

`std::ranges` アルゴリズムを有効にするには、イテレータが `std::default_initializable`、`std::copyable`（代入可能）を満たし、後置インクリメントを提供する必要があります。

## ガイドライン

- **コピー代入を有効にする**: `const Container&` メンバーを `const Container*` に置き換えます。参照は代入演算子を削除しますが、ポインターは削除しません。
- **デフォルト構築を有効にする**: `iterator() = default` を提供し、メンバーポインターを `nullptr` で初期化します。
- **コンセプトを定義する**: `using iterator_concept = std::forward_iterator_tag;` を追加します。
- **後置戻り値型を適用する**: シグネチャを `auto fn() -> Type` に変換します。
- **後置インクリメントを実装する**: 標準コンセプト (`it++`) に必要です。
- **比較を現代化する**: `operator==` を実装します。`operator!=` は自動合成されるため削除します。`operator<=>` は Random Access イテレーターにだけ実装します。
- **ビューインターフェースを利用する**: `std::ranges::view_interface` を継承し、`empty()`、`front()`、`back()`、`data()` を自動生成します。

## リファクタリング diff

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
+         // C++20 は == から != を合成します。
+         // Tripoint が == をサポートする場合は default を使い、そうでなければ手動で == を実装します。
+         auto operator==( const point_generator &rhs ) const -> bool { return p == rhs.p; }
  };
```

## 検証

準拠していることを確認するため、静的アサーションを追加します。

```cpp
static_assert( std::forward_iterator<tripoint_range<Tripoint>::iterator> );
static_assert( std::ranges::forward_range<tripoint_range<Tripoint>> );
```
