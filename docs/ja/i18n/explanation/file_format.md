# 翻訳ファイル形式 (.po)

翻訳データは、各言語および国固有の言語コードで命名された [`".po"` ファイル (Portable Object)][po]に格納されます。例えば、スペインで話されるスペイン語の翻訳は `es_ES.po` に、メキシコで話されるスペイン語の翻訳は `es_MX.po` に格納されます。

[po]: https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html

このファイルはプレーンテキスト形式のため、任意の方法で編集することが可能です。しかし、翻訳者の方々は、[Poedit](https://poedit.net)などの専用の翻訳エディタやや、 ウェブベースの翻訳ツール (例: <https://translations.launchpad.net>)を好んで使用することがよくあります。

`".po"` ファイルの形式はエントリーのリストで構成されており、翻訳対象の英語のフレーズと、それに続くローカライズされた翻訳がセットになっています。英語のフレーズは `msgid`で始まる行、または複数行に記述されます。翻訳されたフレーズは `msgstr` で始まる行、または複数行に記述されます。

`msgid` 行の前には、その単語やフレーズがソースコード内のどこから来たかを示すコメント行が存在します。
これは、英語の意味が不明瞭な場合に非常に役立つことが多いです。
また、翻訳を容易にするために開発者によって残されたコメントが含まれることもあります。

ほとんどのエントリーは次のような形式になります。

```
#: action.cpp:421
msgid "Construct Terrain"
msgstr "niarreT tcurtsnoC"
```

ここでの英語のフレーズは「Construct Terrain」であり、これはaction.cppファイルの421行目から来ています。この例の翻訳は、単に英語の文字を逆にしたものです。この設定により、ゲームは「Construct Terrain」の代わりに「niarreT tcurtsnoC」と表示することになります。

もう一つの例を以下に示します。

Another exmple is:

```
#: action.cpp:425 defense.cpp:635 defense.cpp:701 npcmove.cpp:2049
msgid "Sleep"
msgstr "pleeS"
```

これは前の例と似ていますが、より一般的に使われるフレーズである点が異なります。
このフレーズは、`action.cpp`、`defense.cpp`（2回）、および`npcmove.cpp`のファイルで使用されています。
翻訳は、これらすべての使用箇所を置き換えることになります。

## ファイルヘッダー

`".po"` ファイルの冒頭にあるヘッダーは、comment/msgid/msgstr の形式と異なる唯一の部分です。

既に確立されている翻訳に取り組んでいる場合は、これを変更する必要はありません。

新規の翻訳を始める場合、ヘッダーの大部分は、ご使用のエディタ、または翻訳の初期化に推奨されているプログラムである`msginit`（詳細はTRANSLATING.mdをご覧ください）によって自動的に設定されるはずです。

ただし、他の翻訳ファイルから開始する場合は、いくつかの項目を変更する必要があるかもしれません。可能な限り適切に記入するようにしてください。

ヘッダーは次のようになります。

```
#
# Translators:
# くりやまたいせい, 2023
# Olanti <olanti-p@yandex.ru>, 2023
# O M, 2024
# pigmentblue15, 2025
# やまねこ, 2025
# Coolthulhu <Coolthulhu@gmail.com>, 2025
# jacli mouber, 2025
#
msgid ""
msgstr ""
"Project-Id-Version: cataclysm-bn\n"
"POT-Creation-Date: 2025-10-27 01:44+0000\n"
"PO-Revision-Date: \n"
"Last-Translator: YOUR NAME <your@email.address>\n"
"Language-Team: Japanese (https://app.transifex.com/bn-team/teams/113585/"
"ja/)\n"
"Language: ja\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=1; plural=0;\n"
"X-Generator: Poedit 3.7\n"
```

新規の翻訳を開始する場合、または既存の翻訳の担当者である場合は、翻訳に関する質問や問題が発生した際に連絡が取れるよう、氏名とメールアドレスを含めることが推奨されます。

手動で簡単に記入できない唯一の重要な部分は、`Plural-Forms` セクションです。これは、あなたの言語において異なる数の物事 (例:単数形、複数形) がどのように扱われるかを決定するものです。これについては、後ほど詳しく説明いたします。

## フォーマット文字列と改行

一部の文字列には、`%s`、`%2$d`、`\n` などの特殊な用語が含まれています。

`\n` は改行を表します。\
コードは可能な限り自動的に行を折り返すため、ほとんどの場合は不要です。
しかし、要素を意図的に異なる行に配置したい場合などに使用されることがあります。
改行を入れるべき場所では、翻訳にも必ず`\n`を使用してください。

`%s` およびその他類似の用語（例：`%d`, `%2$d`）は、ゲーム実行時に他の値に置き換えられます。翻訳に応じて、これらの位置を移動する必要があるかもしれません。`%` で始まるすべての用語を翻訳内に保持することが重要です。これらを削除したり変更したりすると、ゲーム内で正しく値が表示されなくなります。

以下は、`%d`が数値に置き換えられる例です。

```
#: addiction.cpp:224
#, c-format
msgid ""
"Strength - %d;   Perception - 1;   Dexterity - 1;\n"
"Depression and physical pain to some degree.  Frequent cravings.  Vomiting."
msgstr ""
";1 - ytiretxeD   ;1 - noitpecreP   ;%d - htgnertS\n"
".gnitimoV  .sgnivarc tneuqerF  .eerged emos ot niap lacisyhp dna noisserpeD"
```

ここで重要なのは、`%d` が逆順にされず、`\n` が行の末尾に残っていることです。このケースでは、メッセージが表示される際に、`%d`はキャラクターの筋力修正値に置き換えられます。

場合によっては、用語の順序を変更する必要があるかもしれません。これを行うと、ゲームが値を正しく認識できず混乱する可能性があります。
`%`用語の順序が変わる場合、ゲームがどの値がどれに対応するかを認識できるように、すべての用語に番号を追加する必要があります。一部の文字列には既にこれらの番号が付いていますが、付いていないものもあります。

例として、`%s shoots %s!` という文字列があるとします。翻訳によって、`%s is shot by %s!` のように語順が変更されるかもしれません。しかし、これでは射手と被射体（撃たれる側）の順序が逆転してしまいます。

この場合、各 `%s` は、`%`と`s`の間に数字 (1〜9) とドル記号 ($) を付けて番号付けする必要があります。
例えば、`%s shoots %s!` は `%1$s shoots %2$s!` と同等になります。
したがって、上記の翻訳の例は、`%2$s is shot by %1$s!` とすることで、正しく機能します。

ゲームはこれらの `%1$s`、`%2$s` といったパラメーターを自動的に解釈できますが、翻訳を行う際には次のことを確認する必要があります。
(A): 翻訳内のすべての %用語に番号が振られていること。
(B): その番号が、元の英語テキストでの出現順序に対して正しいこと。

例えば:

```
#: map.cpp:680
#, c-format
msgid "%s loses control of the %s."
msgstr "%2$s eht fo lortnoc sesol %1$s"
```

この例は、もし`Abigail`が`truck`を運転していると仮定した場合、ゲーム内では `kcurt eht fo lortnoc sesol liagibA`として表示されます。

## 文字列内の特殊タグ

翻訳内のいくつかの文字列には、前または内部に特殊なタグが付いている場合があります。これらのタグはそのまま残し、文字列の残りの部分のみを翻訳する必要があります。
例えば、`data/raw/names.json`にあるNPC名や都市名は、単語（例：素材としての`Wood`と、姓としての`Wood`）との衝突を避けるために`<name>`という接頭辞が付いています。
これらについては、`<name>`の部分を残しておく必要があります。

例:

```
#. ~ proper name; gender=female; usage=given
#: lang/json/json_names.py:6
msgid "<name>Abigail"
msgstr "<name>liagibA"
```

名前には、その名前がゲーム内で何に使用されるかを示すコメントも上部に付いています。
この場合、`Abigail`は女性NPCの「名前（`given name`）」として使用される可能性があります。

## 複数形

多くの言語では、数（単数、複数など）によって異なる用語を使用します。
これらは、`.po`ファイルのヘッダーにある`Plural-Forms`行で定義された複数形のルールを使用してサポートされます。
このような場合、数に応じて異なる形式に対応するために、複数の `msgstr` 行が存在します。
ゲームは、数の値に応じて正しい形式を自動的に選択します。

例:

```
#: melee.cpp:913
#, c-format
msgid "%d enemy hit!"
msgid_plural "%d enemies hit!"
msgstr[0] "!tih ymene %d"
msgstr[1] "!tih seimene %d"
```

ここで、最初のエントリーは `enemy`が1つの場合に、2番目のエントリーは `enemies`が複数の場合に対応します。
この規則は言語によって大きく異なる点にご注意ください。
