# ドキュメントの更新

ドキュメントサイトを更新するには、以下が必要です：

- [Markdownを数分で学ぶ](https://learnxinyminutes.com/docs/markdown/)
- [GitHubアカウントを作成する](https://github.com/join)（ドキュメントサイトのソースコードはGitHubでホストされているため）

## ブラウザ

![edit page](img/edit.webp)

1. ページ下部の `Edit page` ボタンをクリックします。

![alt text](img/github-edit.webp)

2. ドキュメントサイトのGitHubページにリダイレクトされます。ここで変更を編集およびプレビューできます。

> [!NOTE]
>
> - `CONTRIBUTING.md` の `.md` はMarkdownファイルを表します
> - `docs.mdx` の `.mdx` は [MarkDown eXtended](https://mdxjs.com) を表します
>   - これはJavaScriptと[JSXコンポーネント][jsx]のサポートを含むMarkdownのスーパーセットです
>   - 少し複雑ですが、インタラクティブなコンポーネントを使用できます

[jsx]: https://www.typescriptlang.org/docs/handbook/jsx.html

![propose changes window](https://github.com/scarf005/Cataclysm-BN/assets/54838975/d4a06795-1680-4706-a84c-072346bff109)

3. 右上の `Commit changes...` ボタンをクリックして、[変更をコミット](https://github.com/git-guides/git-commit)します。以下の点を確認してください：

- 短く説明的な `Commit message` を書く
- `Create a new branch for this commit and start a pull request` チェックボックスにチェックを入れる

![comparing changes page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/3551797e-847b-45fe-8869-8b0b15bfb948)

4. `Comparing changes` ページにリダイレクトされます。`Create pull request` ボタンをクリックして、[プルリクエストを作成](./contributing.md#プルリクエストに関する注意点)します。

![open a pull request page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/2a987c19-b165-43c2-a5a2-639f22202926)

5. `Create pull request` ボタンをクリックして、[PRをオープン](./contributing.md#プルリクエストに関する注意点)します。小さな変更の場合は、PRの本文を空のままにしても構いません。

## ローカル開発

> [!NOTE]
>
> このセクションでは、[git](https://git-scm.com) と [javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript) の知識があることを前提としています。もちろん、進めながら学ぶこともできます。

ドキュメントサイトをローカルで実行するには、以下が必要です：

- [deno](https://deno.com) をインストールして、フォーマットと自動ドキュメント生成を行う

### 開発サーバーのセットアップ

```sh
(Cataclysm-BN) $ deno task docs serve

# または、すでにdocsディレクトリ内にいる場合
(Cataclysm-BN/docs) $ deno task serve
```

`http://localhost:3000` でドキュメントサイトにアクセスできるようになります。ドキュメントに変更を加えると、開発サーバーは自動的にリロードされます。

### ページの自動生成

LuaとCLIのドキュメントはソースコードから自動的に生成されます。これらを生成するには、プロジェクトのルートに移動して以下を実行します：

```sh
(Cataclysm-BN) $ deno task docs:gen
```

## ライセンス

- Markdownファイル（`.md` および `.mdx` ファイルを含むがこれに限定されない）に貢献することで、ゲームと同じライセンスである [CC-BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) の下で貢献をライセンスすることに同意したことになります。

- ドキュメントページのソースコード（`.ts` ファイルを含むがこれに限定されない）に貢献することで、[AGPL 3.0](https://www.gnu.org/licenses/agpl-3.0.en.html) の下で貢献をライセンスすることに同意したことになります。
