# Cataclysm のテスト

ソースから Cataclysm を `make` すると、 `tests/` ディレクトリにあるテストケースから実行可能ファイル `tests/cata_test` がビルドされます。これらのテストは、
[Catch2 フレームワーク](https://github.com/catchorg/Catch2)で記述されています。

利用可能なコマンドラインオプションを確認するには `tests/cata_test --help` を実行するか、より詳細な入門については
[Catch2 チュートリアル](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md) を参照してください。

## ガイドライン

テストを作成する際は、使用されるすべてのオブジェクト（直接的または間接的）が、テストの前に完全にリセットされていることを確認してください。ランダムに生成されたオブジェクトのプロパティや、グローバルオブジェクト（多くの場合プレイヤーオブジェクト）を介したテスト間の相互作用により、不安定なテストになる事例がいくつか発生しています。一般的なガイドラインとして、テストケースはスタンドアロンであるべきです（あるテストが別のテストの出力に依存すべきではありません）。

JSON定義を使用してオブジェクトを生成する場合、テストが必要とするオブジェクトのプロパティをアサートするために REQUIRE ステートメントを使用してください。これにより、オブジェクトの何が変更されてテストが壊れたのかを明確にすることで、JSON定義の変更からテストを保護できます。

## テストケースの記述

テストを構成し、表現する方法はいくつか選択できますが、基本的な単位は `TEST_CASE` です。各テスト `.cpp` ファイルは、名前とオプションの（強く推奨される）タグのリストを持つ、少なくとも1つのテストケースを定義する必要があります。

```cpp
TEST_CASE( "sweet junk food", "[food][junk][sweet]" )
{
    // ...
}
```

`TEST_CASE`内では、Catch2 フレームワークは、テストの関連する部分を論理的にグループ化するための多数の異なるマクロを許可しています。高い可読性を促進するアプローチの1つとして、`GIVEN`、`WHEN`、および `THEN` セクションを使用する
[BDD](https://en.wikipedia.org/wiki/Behavior-driven_development)
(振る舞い駆動開発) スタイルです。それらを使用したテストのアウトラインは次のようになります。

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

これらの用語で考えることは、テストのセットアップとテストデータの初期化（通常、`GIVEN` 部分で表現されます）、テストしたい結果を生成する操作の実行（多くの場合、`WHEN` 部分に含まれます）、およびこの結果が期待を満たしていることの検証（当然ながら、`THEN` 部分）という論理的な進行を理解するのに役立つかもしれません。

上記の例を実際のテストコードで埋めると、次のようになります。

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

何が起こっているかを見るために、各部分を順番に見ていきましょう。まず、キャラクターまたはプレイヤーを表す `avatar`を宣言します。このテストはプレイヤーの士気 をチェックするため、クリーンな状態を確実にするためにクリアします。

```cpp
avatar dummy;
dummy.clear_morale();
```

`GIVEN` の内部では、`GIVEN` が述べている内容（キャラクターが甘いものが好きであること）を実装するコードが必要です。ゲームのコードでは、これは `PROJUNK` 特性で表されるため、`toggle_trait` を使用して設定できます。

```cpp
GIVEN( "character has a sweet tooth" ) {
    dummy.toggle_trait( trait_PROJUNK );
```

ここで、`GIVEN` の内部にネストされていることに注意してください。`GIVEN` の残りのスコープでは、`dummy` はこの特性を持ちます。この単純なテストでは、さらに数行にしか影響しませんが、テストがより大きく複雑になるにつれて（そうなります）、ネストされたスコープと、これらをどのように使用してテスト間の相互汚染 (cross-pollution) を回避できるかを認識する必要があります。

さて、`dummy` が甘いものが好きになったので、何か甘いものを食べさせたいので、`neccowafers` アイテムをスポーンさせ、それを食べるように指示できます。

```cpp
WHEN( "they eat some junk food" ) {
    dummy.eat( item( "neccowafers" ) );
```

この時点で呼び出す関数は、テストの焦点となることがよくあります。目標は、コードが到達し、テストによってカバーされるように、関数を通過する何らかのパスを実行することです。ここでは `eat` 関数が例として使用されていますが、`eat`関数自体が非常に高レベルで複雑な関数であり、多くの振る舞いとサブ振る舞いがあります。このテストケースは士気への影響のみに関心があるため、より良いテストでは、`eat` が呼び出す `modify_morale` などのより低レベルな関数を呼び出すでしょう。

`dummy` は `neccowafers` を食べましたが、何か効果があったでしょうか？甘いものが好きなので、`MORALE_SWEETTOOTH` として知られる特定の士気ボーナスを得るはずであり、その大きさは少なくとも `5` でなければなりません。

```cpp
THEN( "they get a morale bonus from its sweetness" ) {
    CHECK( dummy.has_morale( MORALE_SWEETTOOTH ) >= 5 );
}
```

この `CHECK` マクロはブール式を取り、式が偽である場合にテストを失敗させます。同様に、`CHECK_FALSE` を使用することもでき、これは式が真である場合に失敗します。

## 要求と確認

`CHECK` および `CHECK_FALSE` マクロは、式の真偽についてアサーションを行いますが、失敗した場合でもテストの続行を許可します。これにより、複数の `CHECK` を実行でき、そのうちの1つ以上が期待を満たさない場合に通知されます。

もう1つのアサーションは `REQUIRE` (およびその対応物 `REQUIRE_FALSE`)です。`CHECK`
アサーションとは異なり、`REQUIRE` 失敗した場合に続行しません。このアサーションは、テストが続行するために不可欠であると見なされます。

`REQUIRE`は、システムの状態に何らかの変更を加えた後で、仮定を再確認したい場合に役立ちます。例えば、`dummy` が本当に望ましい特性を持っていること、そして `neccowafers` が本当にジャンクフードであることを確認するために、スイートトゥースのテストに追加されたいくつかの `REQUIRE` は次のとおりです。

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

ここで `REQUIRE` を使用するのは、これらが失敗した場合、テストを続行する理由がないからです。我々の仮定が間違っている場合、それに続くものは無効になります。明らかに、`toggle_trait` がキャラクターに `PROJUNK` 特性を与えるのに失敗した場合、または `neccowafers` が結局砂糖でできていないことが判明した場合、士気ボーナスのテストは無意味です。

`REQUIRE` はテストの前提条件であると考え、`CHECK`はテストの結果を見ていると考えることができます。
