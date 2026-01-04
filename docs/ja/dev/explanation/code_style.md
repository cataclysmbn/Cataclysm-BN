# C++ コードスタイルガイド

プロジェクト内の全ての C++ コードは、特定のスタイルが適用されています。プルリクエストをプッシュする前に、ご自身の行った変更を astyle に通す必要があります。

現在、astyle バージョン 3.1 を使用しています。バージョン 3.0.1 はわずかな箇所でしか違いを生じませんが、バージョン 3.6.6 では、ほぼ全てのファイルで差異が生じます。

コードブロックは、以下のオプションで astyle に渡すことで、フォーマットが正しいことを保証できます。

```sh
astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

これらのオプションは、`.astylerc`、 `doc/CODE_STYLE.txt` 、および
`msvc-full-features/AStyleExtension-Cataclysm-BN.cfg`にも反映されています。

例として、`vi`エディタでブロックの周囲にマークa と b を設定した場合、以下のように実行します。

```sh
:'a,'b ! astyle --style=1tbs --attach-inlines --indent=spaces=4 --align-pointer=name --max-code-length=100 --break-after-logical --indent-classes --indent-switches --indent-preproc-define --indent-col1-comments --min-conditional-indent=0 --pad-oper --add-braces --convet-tabs --unpad-paren --pad-paren-in --keep-one-line-blocks
```

その他の環境については、[DEVELOPER_TOOLING.md](../reference/tooling) を参照してください。

## コード例

以下は、最も一般的なスタイル上のポイントを示す例です:

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

## コードガイドライン

これらは、より汎用性の低いガイドラインであり、長年にわたり遭遇してきた問題点に基づいています。

- 不変の値を優先し、変数を `const`で宣言してください。可動部分が少ないほど、コードの流れはより予測可能になります。
- `int`を優先してください。
  - 特に`long`は、サポートしている一部のプラットフォームでは int よりも大きい型ではないため、問題を引き起こ
    します。
  - 32 ビットを超える整数値の使用は避けるべきです。本当に必要な場合は `int64_t` を使用してください。
  - `uint` (符号なし整数) も問題があります。オーバーフロー時に好ましくない振る舞いをするため、一般的なプログラミングでの使用は避けるべきです。
    - バイナリデータが必要な場合は、`unsigned int` や `unsigned char` は問題ありませんが、代わりに `std::bitset` の使用を推奨します。
  - `float` の使用は避けるべきですが、有効な用途もあります。
- [`auto` キーワード](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp?view=msvc-170)は、以下のように使用することが理にかなっている場所で使用してください:
  - [後置戻り地型](https://en.wikipedia.org/wiki/Trailing_return_type) を関数宣言で優先してください。長い戻り値型は関数名を不明瞭にし、クラスメソッドの可読性を損ないます。
  ```cpp
  class Bar;
  auto foo( int a ) -> int
  {
      const Bar &bar = some_function();

      return is_bar_ok( bar ) ? 42 : 404;
  }
  ```
  - `decltype` スタイルのジェネリック関数に使用する。
  ```diff
  template<typename A, typename B>
  - decltype(std::declval<A&>() * std::declval<B&>()) multiply(A a, B b)
  + auto multiply( A a, B b ) -> decltype( a * b )
  {
      return a*b;
  }
  ```
  - 長いイテレータ宣言の省略に使用する。
  ```diff
    std::map<int, std::map<std::string, some_long_typename>> some_map;

  - std::map<int, std::map<std::string, some_long_typename>>::iterator iter = some_map.begin();
  + auto iter = some_map.begin();
  ```
  - ラムダ宣言には必須である。
  ```cpp
  auto two_times = []( int a ) { return a * 2; };
  ```
  - それ以外の場合でも、利便性のために可読性を犠牲にしない。[vscode](https://github.com/clangd/vscode-clangd)などの一般的なコードエディタでは、インレイヒントのオプションが利用可能です。

- 標準名前空間に対する `using namespace` の使用を避ける。
- 必要がない限り、クラスに新しいメンバメソッドを追加することを避ける。
  ```diff
  // this function does not access non-public data members or member methods in the class, and thus can be made a free function
  - std::string Character::profession_description() const
  - {
  -     return prof->description( male );
  - }
  + auto profession_description( const Character &c ) -> std::string
  + {
  +     return c.prof->description( c.male );
  + }
  ```
