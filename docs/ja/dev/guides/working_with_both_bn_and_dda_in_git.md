# BN と DDA の両方での作業

DDA から何らかの変更点をポートしたい場合があります。これは、リモートを追加し、チェリーピックを実行することで可能になります。

# tl;dr バージョン

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
git fetch dda
git checkout -b ddamaster dda/master
git checkout ddamaster && git pull
```

# 解説バージョン

## セットアップ

BN と名付けられたディレクトリ `Cataclysm-BN`があると仮定して、そこでターミナルを開きます。

DDA のリモートトラッキングブランチを追加します。ブランチ名は `dda` としましょう(`dda`である必要はありません)。

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
```

新しいブランチの内容をダウンロードするには、フェッチする必要があります。

```
git fetch dda
```

Tこれにより、リモートトラッキングブランチからすべてのコンテンツがダウンロードされます。両方のリポジトリに共通するものはダウンロードされないため、それほど時間はかかりません。フェッチが完了すると、リモート上のブランチを`merge`、`pull`、
`checkout` などできますが、それらの前に`dda/`を付ける必要があります
(例:`dda/master`)。 リモートトラッキングブランチを指すローカルブランチを持つと便利です。

```
git checkout -b ddamaster dda/master
```

これにより、基本的に DDA の `master` ブランチである `ddamaster` ブランチが作成されます。`ddamaster` の代わりに別の名前を使用しても構いません。

## 更新

最も簡単な方法は次のとおりです。

```
git checkout ddamaster 
git pull
```

これでコンフリクトが発生することはないはずです。もし発生した場合、メインブランチに変更をコミットした可能性があります。この場合は、変更をバックアップしたい場合があります。

```
git checkout -b temp-branch-name
```

そして、メインブランチをリモートにリセットします。

```
git branch -f ddamaster dda/master
```

## 貢献

```
# BN ブランチに切り替え
git checkout main
# ローカルコンテンツを更新
git pull
# 変更用の新しいブランチを作成
git checkout -b chainsaw-toothbrush-rebalance
# [ファイルを操作]
...
# 変更をコミット
git commit -a
# Cataclysm のフォークに自分の変更をアップロード（ブランチ名が origin だと仮定）
git push -u origin chainsaw-toothbrush-rebalance
# BN の GitHub に移動してプルリクエストを作成
```

# ポート

ポートは `git cherry-pick`で行われます。まず、マージコミットのハッシュ、またはポートしたいコミット範囲の最初と最後のコミットのハッシュを見つける必要があります。GitHub では、それらは PR のコミット説明の右側にあります。完全なハッシュまたは短縮バージョンを使用できます。以下の例では
`fafafaf` がマージコミットであり、 `a0a0a0a` と `b1b1b1b` が範囲の最初と最後のコミットです（順序が重要です）。次に、ポート先のブランチから、マージコミットまたはコミットの範囲をチェリーピックします。

```
# マージコミット
git cherry-pick fafafaf
# コミット範囲
git cherry-pick a0a0a0a..b1b1b1b
```

コンフリクトを解決します（非自明な PR の場合は多数発生し、ほとんどを手動で解決する必要があります）。

```
git mergetool
```

上記を実行するには、マージコンフリクト解決ツールのセットアップが必要な場合があります（TODO: その方法を記述する）。その後、通常どおりコミットし、プッシュします。
