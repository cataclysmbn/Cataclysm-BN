# 翻訳API

Cataclysm: BNは、翻訳されたテキストを表示するために、[GNU gettext][gettext] と同様に機能するカスタムランタイムライブラリを使用しています。

`gettext` を使用するには、以下の2つのアクションが必要です。

- ソースコード内で翻訳すべき文字列をマークすること。
- 実行時に翻訳関数を呼び出すこと。

翻訳可能な文字列をマークすることで、それらの自動抽出が可能になります。このプロセスにより、ソースコード内にあるオリジナルの文字列（通常は英語）と、それに対応する翻訳された文字列をマッピングするファイルが生成されます。これらのマッピングは、実行時に翻訳関数によって使用されます。

注意点: 翻訳されるのは抽出された文字列のみです。なぜなら、オリジナルの文字列が翻訳を要求するための識別子として機能するからです。翻訳関数が対応する翻訳を見つけられない場合、関数はオリジナルの文字列を返します。

## 翻訳関数

文字列を翻訳対象としてマークし、実行時にその翻訳を取得するためには、以下の関数またはクラスのいずれかを使用する必要があります。

これらの関数のいずれかで使われている文字列リテラルは、自動的に抽出されます。リテラルではない文字列も実行時に翻訳されますが、抽出の対象にはなりません。

### `_()`

この関数は、以下のような単純な文字列での使用に適しています。

```cpp
const char *translated = _( "text marked for translation" )
```

また、以下のように直接使用することもできます。

```cpp
add_msg( _( "You drop the %s." ), the_item_name );
```

JSONファイル内の文字列は、`lang/extract_json_strings.py` スクリプトによって抽出され、実行時に `_()`を使用して翻訳できます。JSON文字列に対して翻訳コンテキストが必要な場合は、代わりに後述の
`class translation` を使用できます。

### `pgettext()`

この関数は、オリジナルの文字列が単独では意味的に曖昧である場合に役立ちます。例えば、色と感情のどちらも意味し得る「blue」という単語などです。

`pgettext` は、翻訳対象の文字列に加えて、コンテキストを受け取ります。このコンテキストは翻訳者には提供されますが、翻訳された文字列の一部にはなりません。この関数の最初のパラメーターがコンテキスト、2番目が翻訳対象の文字列です。

```cpp
const char *translated = pgettext( "The color", "blue" );
```

### `vgettext()`

一部の言語では、複数形に関する複雑なルールがあります。`vgettext`は、これらの複数形を正しく翻訳するために使用できます。最初のパラメーターは単数形の未翻訳文字列、2番目のパラメーターは複数形の未翻訳文字列であり、3番目のパラメーターは実行時にどちらの形式を使用するかを決定するために使用されます。

```cpp
const char *translated = vgettext( "%d zombie", "%d zombies", num_of_zombies );
```

### `vpgettext()`

`vgettext`と同様ですが、翻訳コンテキストを指定できます。

```cpp
const char *translated = vpgettext( "water source, not time of year", "%d spring", "%d springs", num_of_springs );
```

## `translation`

翻訳コンテキストを付けて翻訳のために文字列を格納したい場合や、翻訳が不要な文字列や複数形を持つ文字列を格納したい場合があります。
`translations.h|cpp`ファイル内の`class translation` は これらの機能を単一のラッパーにまとめて提供します。

```cpp
const translation text = to_translation( "Context", "Text" );
```

```cpp
const translation text = to_translation( "Text without context" );
```

```cpp
const translation text = pl_translation( "Singular", "Plural" );
```

```cpp
const translation text = pl_translation( "Context", "Singular", "Plural" );
```

```cpp
const translation text = no_translation( "This string will not be translated" );
```

文字列は、以下のコードで翻訳/取得できます。

```cpp
const std::string translated = text.translated();
```

```cpp
// これは、数値 2 に対応するテキストの複数形を翻訳します
const std::string translated = text.translated( 2 );
```

`class translation` オブジェクトはJSONから読み込むことも可能です。`translation::deserialize()` メソッドは
`JsonIn` オブジェクトからの逆シリアル化を処理します。このため、対応するJSON形式を用いて翻訳データをJSONから読み込むことができます。JSON構文には以下の３つの通形式があります。

単純な形式 (文字列として):

```json
"name": "bar"
```

完全なオブジェクト形式:

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

簡略化されたオブジェクト形式 (単数形と複数形が同じ場合):

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

上記のコードにおいて、`"ctxt"` と `"str_pl"` はいずれもオプションですが、 `"str_sp"` は `"str"` と `"str_pl"` に同じ文字列を指定するのと同等です。さらに、`"str_pl"` と `"str_sp"` は、翻訳オブジェクトが `plural_tag` や`pl_translation()` を使用して構築された場合、あるいは
`make_plural()` を使用して変換された場合にのみ読み込まれます。例は以下の通りです。

```cpp
translation name{ translation::plural_tag() };
jsobj.read( "name", name );
```

`"str_pl"` と `"str_sp"` のどちらも指定されていない場合、複数形は単数形に "`s`" を追加したものがデフォルトとなります。

以下のように記述することで、翻訳者向けのコメントをJSONエントリに追加することも可能です（エントリの順序は問いません）。

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

現在、このJSON構文がサポートされているのは、以下にリストされている一部のJSON値のみです。

- 他のJSON文字列でこの形式を使用したい場合は、`translations.h` と `translations.cpp` を参照し、対応するコー
  ドを移行してください。
- 移行後、文字列が翻訳のために正しく抽出されていることを確認するため、update_pot.sh をテストしてください。
- また、`translation` クラスによって報告されるテキストスタイリングの問題を修正するために、ユニットテストを
  実行することも推奨されます。

### サポートされているJSON値

- エフェクト名
- アイテムアクション名
- アイテムカテゴリ名
- アクティビティの動詞
- ゲートアクションメッセージ
- 呪文名および説明
- 地形/備品の説明（オーバーマップ地形、設置物）
- モンスターの近接攻撃メッセージ
- 士気効果の説明
- 変異名と説明
- NPCクラス名/説明
- 道具の性能名
- スコアの説明
- スキル名と説明
- CBM名と説明
- 地形の破壊音の説明
- トラップと車両の衝突音の説明
- 車両部品名/説明
- スキル表示タイプ名
- NPCダイアログのu_buy_monsterユニーク名
- スペルメッセージおよびモンスターのスペルメッセージ
- 格闘術名および説明
- ミッション名および説明
- 欠陥名および説明
- 種子データ内の植物名
- 形態変化使用時の行動メッセージとメニューテキスト
- テンプレートNPC名と名前の接尾辞
- NPCの会話応答テキスト
- レリック名のオーバーライド
- レリックのリチャージメッセージ
- スピーチテキスト
- チュートリアルメッセージ
- ビタミン名
- レシピ設計図名
- レシピグループのレシピ説明
- アイテム名 (複数形サポート) および説明
- レシピの説明
- インスクライブ使用アクションの動詞/動名詞
- モンスター名 (複数形サポート) および説明
- スニペット
- 身体部位名
- アクション名のキー割当
- フィールドレベル名

### Lua

[4つの翻訳関数はLuaコードに公開されています](../../mod/lua/tutorial/modding.md#translation-functions)。

### 推奨事項

Cataclysm: BNでは、`itype`や`mtype`といった一部のクラスが、`nname`と呼ばれる翻訳関数のラッパーを提供しています。

空文字列が翻訳対象としてマークされた場合、それは空文字列ではなく、常にデバッグ情報に翻訳されます。
ほとんどの場合、文字列が空になることはないと見なせるため、文字列を常に安全に翻訳対象としてマークできます。
しかし、文字列が空になる可能性があり、かつ翻訳後も空のままである必要がある場合は、文字列が空でないことを確認してから、翻訳関数に渡す必要があります。

エラーメッセージやデバッグメッセージは、翻訳対象としてマークしてはなりません。
これらのメッセージが表示された場合、プレイヤーはゲームが出力した正確なテキストをそのまま報告することが期待されているためです。

詳細については、 [gettext マニュアル][manual] を参照してください。

[gettext]: https://www.gnu.org/software/gettext/
[manual]: https://www.gnu.org/software/gettext/manual/index.html
