# 新しい型のバインディング

<a id="adding-new-type-to-the-doc-generator-without-binding-internals"></a>

### 内部をバインドせずにドキュメント生成器へ新しい型を追加する

C++ の型がドキュメント生成器に登録されていない場合、`<cppval: **gibberish** >` として表示されます。この問題を緩和するには、`catalua_luna_doc.h` に `LUNA_VAL( your_type, "YourType" )` を追加します。生成器は引数の型に `YourType` という文字列を使用します。

<a id="binding-new-type-to-lua"></a>

### Lua に新しい型をバインドする

まず、バインディングシステムに新しい型を登録する必要があります。ドキュメント生成器が型を理解できるようにし、ランタイムがその型を含む Lua テーブルを JSON からデシリアライズできるようにするなど、多くの理由で必要です。登録しないと `Type must implement luna_traits<T>` というコンパイルエラーが発生します。

1. `catala_luna_doc.h` に型の宣言を追加します。例えば、架空の `horde` 型（`struct`）をバインドする場合、ファイルの上部付近に1行追加します:
   ```cpp
   struct horde;
   ```
   複雑なテンプレート型では関連ヘッダーを実際に読み込む必要がある場合がありますが、コンパイル時間に大きく影響するため、できるだけ避けてください。

2. 同じファイルで型をドキュメント生成器に登録します。`horde` の例を続けると、次のようにします:
   ```cpp
   LUNA_VAL( horde, "Horde" );
   ```
   C++ の型名にはさまざまなスタイルがありますが、Lua 側ではすべて `CamelCase` にします。

ここから実際の詳細に入ります。バインディングは `catalua_bindings*.cpp` ファイルに実装されています。コンパイルを高速化し、見つけやすくするため複数の `.cpp` ファイルに分かれているので、既存の `catalua_bindings*.cpp` に追加するか、同様のファイルを作成できます。同じ理由で関数にも分かれています。`horde` 型を登録し、新しいファイルと関数に入れてみましょう:

1. `catalua_bindings.h` に新しい関数宣言を追加します:
   ```cpp
   void reg_horde( sol::state &lua );
   ```
2. `catalua_bindings.cpp` の `reg_all_bindings` で関数を呼び出します:
   ```cpp
   reg_horde( lua );
   ```
3. 次の内容で `catalua_bindings_horde.cpp` という新しいファイルを作成します:
   ```cpp
   #include "catalua_bindings.h"

   #include "horde.h" // Replace with the header where your type is defined

   void cata::detail::reg_horde( sol::state &lua )
   {
       sol::usertype<horde> ut =
           luna::new_usertype<horde>(
               lua,
               luna::no_bases,
               luna::constructors <
                   // Define your actual constructors here
                   horde(),
                   horde( const point & ),
                   horde( int, int )
                   > ()
               );

       // Register all needed members
       luna::set( ut, "pos", &horde::pos );
       luna::set( ut, "size", &horde::size );

       // Register all needed methods
       luna::set_fx( ut, "move_to", &horde::move_to );
       luna::set_fx( ut, "update", &horde::update );
       luna::set_fx( ut, "get_printable_name", &horde::get_printable_name );

       // Add (de-)serialization functions so we can carry
       // our horde over the save/load boundary
       reg_serde_functions( ut );

       // Add more stuff like arithmetic operators, to_string operator, etc.
   }
   ```
4. これで完了です。型は Lua から `Horde` という名前で見えるようになり、バインドしたメソッドとメンバーを使用できます。

### Lua に新しい型をバインドする（Neovim の正規表現を使用）

クラスや構造体を手作業で Lua にバインドするのはかなり面倒なので、ヘッダーファイルを変換してクラスをバインドする別の方法があります。[前述](#binding-new-type-to-lua)の第2部の第3ステップでは、Neovim の組み込み正規表現と C++ マクロを使ってクラスをバインドできます。

1. クラス定義のコピーを作成します。
2. 次の両方を適用します: `%s@class \([^{]\)\+\n*{@private:@` `%s@struct \([^{]\)\+\n*{@public:@`
3. 冒頭にあるコンストラクターや不要なメソッドを手動で削除します。
4. すべての `private`/`protected` メソッドを削除します: `%s@\(private:\|protected:\)\_.\{-}\(public:\|};\)@\2`
5. クラス定義末尾の `};` を削除します。
6. `public` ラベルを削除します: `%s@ *public:\n@`
7. コメントを削除します: `%s@\( *\/\*\_.\{-}\*\/\n\{,1\}\)\|\( *\/\/\_.\{-}\(\n\)\)@\3@g`
8. 基本インデントが0になるまでインデントを解除します。
9. メソッド定義を宣言に変換します: `%s@ *{\(}\|\_.\{-}\n^}\)@;`
10. ほとんどのメソッド宣言を1行にまとめます: `%s@\((\|,\)\n *@\1@g`
11. デフォルト値を削除します: `%s@ *= *\_.\{-}\( )\|;\|,\)@\1@g`
12. `overriden`/`static` メソッド・メンバーと `using` を削除します:
    `%s@.*\(override\|static\|using\).*\n@@g`
13. `template` を削除します: `%s@^template<.*>\n.*\n@@g`
14. `virtual` タグを削除します: `%s@^virtual *@`
15. すべての行がセミコロンで終わっているか確認します: `%s@\([^;]\)\n@\0@gn`
16. 関数の数を数えます: `%s@\(.*(.*).*\)@@nc`
17. 最初に見つかった関数を末尾へ移動します: `%s@\(.*(.*).*\)\n\(\n*\)\(\_.*\)@\3\1\2`
18. 次に、15のステップで見つかった数から1を引いた回数だけ、16のステップを繰り返します。Neovim では、マッチ数から1を引いた値、`@`、それから `:` を入力します。例えば `'217@:'` なら直前のコマンドを217回繰り返します。
19. 改行を整理します: `%s@\n\{3,}@\r\r`
20. メソッドをマクロでラップします: `%s@\(.*\) \+\([^ ]\+\)\((.*\);@SET_FX_T( \2, \1\3 );`
21. メンバーをマクロでラップします。先に対象範囲を選択してください:
    `s@.\{-}\([^ ]\+\);@SET_MEMB( \1 );`
22. 以前複数行だったメソッド宣言を再び複数行にします:
    `%s@\(,\)\([^ ]\)@\1\r        \2@g`

残るのは、テキストのまとまりを Lua バインディングに使用することです。`horde` の例を続けると、これらのマクロを使ったコードは次のようになります:

```cpp
#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"

#include "horde.h" // Replace with the header where your type is defined

void cata::detail::reg_horde( sol::state &lua )
{
    #define UT_TYPE horde
    sol::usertype<UT_TYPE> ut =
    luna::new_usertype<UT_TYPE>(
        lua,
        luna::no_bases,
        luna::constructors <
            // Define your actual constructors here
            UT_TYPE(),
            UT_TYPE( const point & ),
            UT_TYPE( int, int )
            > ()
       );

    // Register all needed members
    SET_MEMB( pos );
    SET_MEMB( size );

    // Register all needed methods
    SET_FX_T( move_to, ... ); // Instead of ..., there'd be the type declaration of the method.
    SET_FX_T( update, ... );
    SET_FX_T( get_printable_name, ... );

    // Add (de-)serialization functions so we can carry
    // our horde over the save/load boundary
    reg_serde_functions( ut );

    // Add more stuff like arithmetic operators, to_string operator, etc.
    // ...
    #undef UT_TYPE // #define UT_TYPE horde
}
```

この方法にはテンプレートメソッドのバインディングがなく、壊れる可能性があります。コンパイラーエラーやリンカーのフリーズが起きるため、基本的に壊れているものと考え、わずかな修正や手動追加が必要になると想定するのがよいでしょう。

### Lua に新しい enum をバインドする

enum のバインディングは型のバインディングと似ています。架空の `horde_type` enum をバインドしてみましょう:

1. enum に明示的な基底型（ヘッダーで `enum name` の後にある `: type` 部分）がない場合は、まず基底型を指定します。例:
   ```diff
     // hordes.h
   - enum class horde_type {
   + enum class horde_type : int {
       animals,
       robots,
       zombies
     }
   ```
2. `catalua_luna_doc.h` に宣言を追加します:
   ```cpp
   enum horde_type : int;
   ```
3. `catalua_luna_doc.h` で次のように登録します:
   ```cpp
   LUNA_ENUM( horde_type, "HordeType" )
   ```
4. enum が `std::string` との自動変換を実装していることを確認します。詳細は `enum_conversions.h` を参照してください。すでに実装されている enum もありますが、多くは実装されていません。通常はヘッダーで enum `T` の `enum_traits<T>` を特殊化し、`.cpp` ファイルで enum から文字列への変換を使う `io::enum_to_string<T>` を定義します。`enum_traits<T>` に必要な「最後」の値を持たない enum もあります。その場合は追加する必要があります:
   ```diff
     enum class horde_type : int {
       animals,
       robots,
   -   zombies
   +   zombies,
   +   num_horde_types
     }
   ```
   これは「単調な」enum、つまり0から始まり値を飛ばさないものにだけ機能します。上の例では `animals` は暗黙的に `0`、`robots` は `1`、`zombies` は `2` なので、正しく期待どおりの暗黙値 `3` を持つ `num_horde_types` を簡単に追加できます。
5. `catalua_bindings.cpp` の `reg_enums` 関数で enum のフィールドをバインドします:
   ```cpp
   reg_enum<horde_type>( lua );
   ```
   これはステップ4の自動変換を使うため、JSON と Lua で同じ名前になります。

### 新しい `string_id<T>` または `int_id<T>` を Lua にバインドする

これらは `T` 自体のバインディングとは別に行えます。

1. まだ登録していなければ、型 `T` をドキュメント生成器に登録します（[関連ドキュメント](#adding-new-type-to-the-doc-generator-without-binding-internals)を参照）。
2. ステップ1の `LUNA_VAL` を `LUNA_ID` に置き換えます。
3. 型 `T` が演算子 `<` と `==` を実装していることを確認します。通常は手動で実装しやすく、`catalua_type_operators.h` の `LUA_TYPE_OPS` マクロで半自動的に実装できます。
4. 型 `T` に null `string_id` があることを確認します。なければ `string_id_null_ids.cpp` に追加できます。`T` が class として定義されている場合は `MAKE_CLASS_NULL_ID` マクロ、それ以外の場合は `MAKE_STRUCT_NULL_ID` マクロを使います。
5. 型 `T` の `string_id` に `obj()` と `is_valid()` メソッドが実装されていることを確認します。これらは型ごとに実装されています。他の `string_id` を例として確認することを推奨します。
6. 該当する `catalua_bindings_ids_*.cpp` のまとまりに、型 T が定義されているヘッダーを追加します:
   ```cpp
   #include "your_type_definition.h"
   ```
7. そのまとまりの `reg_game_ids_*` 関数で次のように登録します:
   ```cpp
   reg_id<T, true>( lua );
   ```

`string_id<T>` だけをバインドし、`int_id<T>` を必要としない（または実装できない）場合は、この `true` を `false` に置き換えられます。

この段階で、例えば `is_valid()` や `NULL_ID()` メソッドについてリンカーエラーが出ることがあります。さまざまな理由で、すべての string または int id にこれらが実装されているわけではないためです。その場合は手動で定義してください。詳しくは `string_id` と `int_id` の関連ドキュメントを参照してください。

これで完了です。型 `T` は Lua では `Raw` 接尾辞付きで表示され、`string_id<T>` には `Id`、`int_id<T>` には `IntId` 接尾辞が付きます。例えば `LUNA_ID( horde, "Horde" )` では次のようになります:

- `horde` -> `HordeRaw`
- `string_id<horde>` -> `HordeId`
- `int_id<horde>` -> `HordeIntId`

3つの間の型変換はすべてシステムによって自動実装されます。`T` の実際のフィールドとメソッドは、通常どおり Lua にバインドできます。
