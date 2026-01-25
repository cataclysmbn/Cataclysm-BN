# 貢献について

## 協力したいですか？

支援はいつでも歓迎します。特に以下の分野で助けが必要です：

- バグ報告。DDAから引き継がれたバグも含みます。
- バグではないが問題である箇所の特定。紛らわしい説明、類似のケースと比較して明らかに異常な数値、文法ミス、解決策が明白なUIの不具合など。
- 無駄なものを有用にする、または削除する。分解レシピがあるべきなのにないアイテムへの追加、スポーンリスト内の完全に重複するアイテムを一般的なバージョンに置き換える（例：「小さなラベル付きの瓶」を単なる「小さな瓶」に）。
- タイルセット作業。新しい電力網要素のような新しいオブジェクトを追加することがありますが、それらには新しいタイルが必要になる場合があります。
- バランス分析。これは深く掘り下げたものか、「明らかに正しい」ものである必要があります。「明らかに正しい」とは、例えば「武器xはyよりもステータスが完全に優れているが、yの方が希少な素材を必要とし、製作条件も同じである」といった場合です。
- プロファイラを使用したパフォーマンスのボトルネック特定。
- コード品質の改善。

## 始め方

Cataclysm: Bright Nightsへの貢献は簡単です：

1. GitHubでこのリポジトリをフォーク（Fork）します。
2. 変更を加えます。
3. プルリクエスト（Pull Request）を送ります。

> [!NOTE]
>
> #### ライセンス
>
> Cataclysm: Bright NightsはCreative Commons Attribution ShareAlike 3.0ライセンスの下で公開されています。
> ゲームのコードとコンテンツは、いかなる目的であれ自由に使用、変更、再配布することができます。詳細は http://creativecommons.org/licenses/by-sa/3.0/ を参照してください。つまり、プロジェクトへの貢献もすべて同じライセンスの対象となり、このライセンスは取り消すことができません。

## ガイドライン

守っていただきたいガイドラインがいくつかあります：

- このリポジトリを `upstream` リモートとして追加してください。
- `main` ブランチは常にクリーンに保ってください。そうすることで、このリポジトリの変更を自分のリポジトリに簡単に取り込むことができます。
- 新しい機能やバグ修正のセットごとに、新しいブランチを作成してください。
- ローカルブランチから `main` ブランチには絶対にマージ（merge）しないでください。`main` ブランチは `upstream/main` からプル（pull）することによってのみ更新してください。

## コードスタイル

### C++

コードスタイルは `astyle` によって強制されます。詳細は [CODE_STYLE](./../dev/explanation/code_style.md) を参照してください。

### JSON

JSONファイルは `tools/format` にあるカスタムフォーマッタを使用してフォーマットされます。詳細は [JSONスタイルガイド](./../mod/json/explanation/json_style.md) を参照してください。

### Markdown

`doc/` などのMarkdownファイルは [`deno`](https://deno.com) の組み込みフォーマッタを使用してフォーマットされます。どこでも [`deno fmt`](https://deno.land/manual/tools/formatter) を実行してMarkdownファイルを整形できます。VSCodeでは、以下の設定を行うことで保存時に自動フォーマットできます：

```json
// .vscode/settings.json
{
  "[markdown]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "denoland.vscode-deno"
  }
}
```

### Lua

Luaファイルは [`dprint`](https://dprint.dev) の組み込みフォーマッタを使用してフォーマットされます。どこでも [`deno task dprint fmt`](https://dprint.dev/plugins/lua) を実行してLuaファイルを整形できます。詳細は [Luaスタイルガイド](./../mod/lua/explanation/lua_style.md) を参照してください。

## 翻訳

Cataclysm: BNの翻訳はTransifexを使用して行われます。サポートされている言語の最新リストについては、[翻訳プロジェクト](https://app.transifex.com/bn-team/cataclysm-bright-nights/) を確認してください。

詳細情報：

- [翻訳者向け](./../i18n/tutorial/transifex)
- [開発者向け](./../i18n/reference/translation)
- [メンテナ向け](./../i18n/guides/maintain)

## ドキュメント

<!--
![](./img/contributing-doxy1.png)
![](./img/contributing-doxy2.png)

自動生成されたドキュメントは [GitHub Pages](https://cataclysmbn.github.io/Cataclysm-BN) で公開されています。 -->

### Doxygenコメント

クラスやクラスメンバに関する詳細なドキュメントは、新しい貢献者がコードを読みやすくするために役立ちます。既存のクラスへの新しいDoxygenコメントの追加は歓迎されます。

クラスのコメントには以下のテンプレートを使用してください：

```cpp
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 */
class foo {

}
```

関数のコメントには以下のテンプレートを使用してください：

```cpp
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 * @param param1 Description of param1 (optional)
 * @return Description of return (optional)
 */
int foo(int param1);
```

メンバ変数のコメントには以下のテンプレートを使用してください：

```cpp
/** Brief description **/
int foo;
```

### ドキュメント追加のガイドライン

- Doxygenコメントは実装ではなく、外部に対する振る舞いを記述すべきです。ただし、Cataclysmの多くのクラスは絡み合っているため、実装の説明が必要な場合もよくあります。
- 名前だけでは初心者に分からないことを説明してください。
- 重複した説明は避けてください：`/** Map **/; map* map;` は役に立たないコメントです。
- Xを文書化する場合、X自体が何をするかだけでなく、Xが他のコンポーネントとどのように相互作用するかを説明してください。

### ローカルでのドキュメントビルドと閲覧

- doxygenをインストール
- `doxygen doxygen_doc/doxygen_conf.txt`
- `firefox doxygen_doc/html/index.html` (firefoxはお好みのブラウザに置き換えてください)

## ワークフローの例

### 環境設定

_(これは一度だけ行う必要があります。)_

1. GitHubでこのリポジトリをフォークします。

1. フォークしたリポジトリをローカルにクローンします。

```sh
$ git clone https://github.com/YOUR_USERNAME/Cataclysm-BN.git
# ターミナルの現在のディレクトリにリポジトリのフォークをクローンします
```

3. コミットメッセージのテンプレートを設定します。

```sh
$ git config --local commit.template .gitmessage
```

4. このリポジトリをリモートとして追加します。

```sh
$ cd Cataclysm-BN
# プロンプトのアクティブディレクトリを新しくクローンした "Cataclysm-BN" ディレクトリに変更します
$ git remote add -f upstream https://github.com/cataclysmbn/Cataclysm-BN.git
# 元のリポジトリを "upstream" という名前のリモートに割り当てます
```

コミットメッセージのガイドラインの詳細については、以下を参照してください：

- [codeinthehole.com](https://codeinthehole.com/tips/a-useful-template-for-commit-messages/)
- [chris.beams.io](https://chris.beams.io/posts/git-commit/)
- [help.github.com](https://help.github.com/articles/closing-issues-using-keywords/)

### `main` ブランチの更新

0. **`main` ブランチが origin の `main` ではなく upstream の `main` を追跡していることを確認してください：**

```sh
$ git branch --set-upstream-to upstream/main main
# mainブランチがupstreamのmainを追跡するようにします。
```

`main` が正しく upstream を指しているか確認するには、`git remote -v` と入力します。以下のようになるはずです：

```sh
$ git remote -v

origin  git@github.com:YOUR_USERNAME/Cataclysm-BN.git (fetch)
origin  git@github.com:YOUR_USERNAME/Cataclysm-BN.git (push)
upstream        https://github.com/cataclysmbn/Cataclysm-BN.git (fetch)
upstream        https://github.com/cataclysmbn/Cataclysm-BN.git (push)
```

そして `git branch -vv` と入力すると、`main` ブランチの横に `[upstream/main]` が表示されるはずです：

```sh
$ git branch -vv

# ...
* main   ed11439ee61 [upstream/main] feat: add more vending machines to marina (#7001)
# ...
```

1. `main` ブランチにいることを確認してください。

```sh
$ git switch main
```

2. `upstream/main` ブランチから変更を取り込みます（pull）。

```sh
$ git pull --ff-only upstream main
# "upstream" リモートの "main" ブランチから変更を取得します
```

> **注意** エラーが発生した場合、ローカルの `main` ブランチに直接コミットしたことを意味します。
> [この問題を解決する方法については、ここをクリックしてください](#why-does-git-pull---ff-only-result-in-an-error)。

### 変更を加える

0. まだ行っていない場合は、`main` ブランチを更新してください。

1. 新しい機能やバグ修正ごとに、新しいブランチを作成してください。

```sh
$ git switch --create new_feature
# "new_feature" という新しいブランチを作成し、それをアクティブなブランチにします
```

2. ローカルで変更をコミットしたら、GitHub上のフォークにプッシュする必要があります。

```sh
$ git push origin new_feature
# クローン時に origin は自動的にフォークを指すように設定されています
```

3. ブランチでの作業が完了し、すべての変更をコミットしてプッシュしたら、`new_feature` ブランチからこのリポジトリの `main` ブランチへプルリクエストを送ってください。

> **注意** GitHub上の `new_feature` ブランチへの新しいコミットは自動的にプルリクエストに含まれます。そのため、関連する変更のみを同じブランチにコミットするようにしてください。

## プルリクエストに関する注意点

PRを作成したがまだ作業中の場合は、[ドラフト(draft)](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests#draft-pull-requests)としてマークしてください。
これにより、準備ができている部分のみをレビューできるようになり、レビュープロセスがスピードアップし、完全に準備ができていないものがマージされるのを防ぐことができます。

PRを作成するためにIssueを解決または参照することは必須ではありませんが、そうする場合は、PRが解決する問題を詳細に説明する必要があります。

### キーワードを使用してIssueを閉じる

もう一つ：PRがIssueを閉じる、修正する、または解決することを示す場合、説明のどこかに以下を含めてください：

```md
- {keyword} #{issue}
```

例：`- fixed #12345`

### キーワード

`{keyword}` は以下のいずれかである必要があります：

- `close`, `closes`, `closed`
- `fix`, `fixes`, `fixed`
- `resolve`, `resolves`, `resolved`

### Issue

`{issue}` はPRがマージされた後に閉じるIssueの番号です。

これにより、PRが取り込まれたときに自動的にIssueが閉じられ、マージ作業が少し速くなります。

### 一度に複数のIssueを閉じる

```md
- {keyword} #{issue}, {keyword} #{issue}
```

詳細は https://help.github.com/articles/closing-issues-using-keywords を参照してください。

## ツールサポート

貢献を適切なスタイルに保つのに役立つさまざまなツールが利用可能です。
詳細は [開発者ツール](./../dev/reference/tooling) を参照してください。

## 高度なテクニック

これらのガイドラインは必須ではありませんが、物事を整理しておくのがはるかに簡単になります。

### リモート追跡ブランチの使用

リモート追跡ブランチを使用すると、どのリモートブランチから変更を取得するかを自動的に知ることができるため、このリポジトリの `main` ブランチと簡単に連絡を取り合うことができます。

```sh
$ git branch -vv
* main        xxxx [origin/main] ....
  new_feature xxxx ....
```

ここでは2つのブランチがあります。`origin/main` を追跡している `main` と、どのブランチも追跡していない `new_feature` です。実際には、これはgitがどこから変更を取得すればよいかわからないことを意味します。

```sh
$ git checkout new_feature
Switched to branch 'new_feature'
$ git pull
There is no tracking information for the current branch.
Please specify which branch you want to merge with.
```

`upstream/main` から `new_feature` ブランチに変更を簡単にプルするには、gitにどのブランチを追跡すべきかを伝えることができます。（ローカルのmainブランチに対してもこれを行うことができます。）

```sh
$ git branch -u upstream/main new_feature
Branch new_feature set up to track remote branch main from upstream.
$ git pull
Updating xxxx..xxxx
....
```

ブランチを作成すると同時に追跡情報を設定することもできます。

```sh
$ git branch new_feature_2 --track upstream/main
Branch new_feature_2 set up to track remote branch main from upstream.
```

> **注意**：これにより `upstream/main` からのプルは簡単になりますが、プッシュに関しては何も変わりません。`upstream/main` へのプッシュ権限がないため、`git push` は失敗します。

```sh
$ git push
error: The requested URL returned error: 403 while accessing https://github.com/cataclysmbn/Cataclysm-BN.git
fatal: HTTP request failed
$ git push origin
....
To https://github.com/YOUR_USERNAME/Cataclysm-BN.git
xxxx..xxxx  new_feature -> new_feature
```

## ユニットテスト

ソースツリーの tests/ にテストスイートが組み込まれています。ゲームソースへの**あらゆる**変更の後にテストスイートを実行する必要があります。通常通り `make` を実行すると tests/cata_test にテスト実行ファイルがビルドされ、通常の実行ファイルと同様に、または `make check` を介して呼び出すことができます。引数なしで実行すると、テストスイート全体が実行されます。`--help` を付けると、操作を調整するために使用できるいくつかの呼び出しオプションが表示されます。

```sh
$ make
... compilation details ...
$ tests/cata_test
Starting the actual test at Fri Nov  9 04:37:03 2018
===============================================================================
All tests passed (1324684 assertions in 94 test cases)
Ended test at Fri Nov  9 04:37:45 2018
The test took 41.772 seconds
```

習慣的に `make YOUR BUILD OPTIONS && make check` のように make を呼び出すことをお勧めします。

## ゲーム内テスト、テスト環境、デバッグメニュー

新しい機能を実装する場合でも、バグを修正する場合でも、ゲーム内で変更をテストすることは常に良い習慣です。通常のゲームをプレイしてテストするための正確な条件を作成するのは難しい作業になる可能性があるため、デバッグメニューがあります。メニューを表示するためのデフォルトのキーはないため、最初に割り当てる必要があります。

キーバインドメニュー（`Escape` を押してから `1`）を表示し、ほぼ一番下までスクロールして `+` を押し、新しいキーバインドを追加します。_Debug menu_ 項目に対応する文字を押し、デバッグメニューを表示するために使用したいキーを押します。変更をテストするには、新しいキャラクターで新しい世界を作成します。その世界に入ったら、デバッグメニュー用に割り当てたキーを押すと、次のようなものが表示されるはずです：

```
┌─────────────────────────────────────────────────────┐
│ Debug Functions - Manipulate the fabric of reality! │
├─────────────────────────────────────────────────────┤
│ i Info                                              │
│ Q Quit to main menu                                 │
│ s Spawning...                                       │
│ p Player...                                         │
│ t Teleport...                                       │
│ m Map...                                            │
└─────────────────────────────────────────────────────┘
```

これらのコマンドを使用すると、変更をテストするための適切な条件を再現できるはずです。[BN wiki](https://cataclysmbn-guide.com/) には、デバッグメニューに関する有用な情報があるかもしれません。

## よくある質問

### なぜ `git pull --ff-only` でエラーが発生するのですか？

`git pull --ff-only` でエラーが表示される場合、ローカルの `main` ブランチに直接コミットしたことを意味します。これを修正するには、これらのコミットを含む新しいブランチを作成し、`upstream/main` から分岐したポイントを見つけて、`main` をそのポイントにリセットします。

```sh
$ git pull --ff-only upstream main
From https://github.com/cataclysmbn/Cataclysm-BN
 * branch            main     -> FETCH_HEAD
fatal: Not possible to fast-forward, aborting.
$ git branch new_branch main          # 現在のコミットを一時ブランチでマーク
$ git merge-base main upstream/main
cc31d0... # mainに直接コミットする前の最後のコミット
$ git reset --hard cc31d0....
HEAD is now at cc31d0... ...
```

これで `main` がクリーンアップされたので、`upstream/main` から簡単にプルでき、`new_branch` で作業を続けることができます。

```sh
$ git pull --ff-only upstream main
# "upstream" リモートから一致するブランチ（この場合は "main"）の変更を取得します
$ git checkout new_branch
```

その他のよくある質問については、[開発者FAQ](./../dev/reference/faq.md) を参照してください。
