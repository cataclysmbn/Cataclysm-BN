# 変更履歴のガイドライン

PRのタイトルは、変更履歴の生成を容易にするために [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) に従います。形式は以下のいずれかです：

```
<タイプ>: <PRの件名>
<タイプ>(<スコープ>, <スコープ>, ...): <PRの件名>
```

例えば、PRのタイトルは以下のようになります：

```
feat: add new mutation
feat(port): port mutation description from DDA
```

PRのタイトルは、プレイヤーが一目で理解しやすいものであるべきです。以下のように、命令形で説明的なタイトル（`<動詞> <名詞>`）を使用することをお勧めします：

```diff
- feat: rebalancing some rifles
+ feat: nerf jam chance of m16 and m4
```

変更前：PRの説明全体を読まないと、強化されたのか弱体化されたのか、どのライフルが変更されたのかを知るのが難しいです。
変更後：タイトル自体から何が変更されたのかを正確に理解するのが簡単です。

## タイプ

タイプはPRタイトルの最初の単語です。これらは行われる変更の種類を指定します。迷った場合は、新機能には `feat`、バグ修正には `fix` を使用してください。以下は頻繁に使用されるカテゴリです：

### `feat`: 機能

新機能、追加、またはバランス調整。

### `fix`: バグ修正

バグを修正したり、ゲームをより安定させたりするものすべて。

### `refactor`: インフラ

動作を変更せずに開発を容易にします。例えば：

- `C++` のリファクタリングとオーバーホール
- `Json` の再編成
- `docs/`、`.github/` およびリポジトリの変更
- その他の開発ツール

### `build`: ビルド

ビルドプロセスの改善：

- より堅牢に
- より使いやすく
- コンパイル時間の短縮

### その他

- `docs`: ドキュメントの変更
- `style`: コードスタイルの変更（空白、フォーマットなど）、通常はJSONフォーマットの修正。
- `perf`: パフォーマンスの改善
- `test`: 不足しているテストの追加または既存のテストの修正
- `ci`: CIプロセスの変更
- `chore`: 上記のカテゴリのいずれにも当てはまらないその他の変更
- `revert`: 以前のコミットの取り消し

## スコープ

1. カテゴリの後ろの括弧内に使用して、PRの範囲をさらに絞り込みます。
2. スコープの数に制限はありません。
3. 必須ではありませんが、推奨されます。
4. これらはガイドラインであり、ルールではありません。あなたのPRに最適なものを自由に選んでください！

### `<なし>`: 一般的な機能

例えば、

プレイヤーに関連する変更：

- プレイヤーが新しいことができるようになる（例：変異、スキル）
- プレイヤーに新しいことが起こる（例：新しい病気）

新しいコンテンツ：

- 新しいモンスター
- 新しいマップエリア
- 新しいアイテム
- 新しい車両
- 新しい装置

PRタイトルの例：

```
feat: strength training activity
feat: mutation system overhaul
feat: semi-plausible smokeless gunpowder recipe
feat(port): game store
```

### `lua`: Lua APIの変更

Lua APIへの変更、例えば：

- [新しいバインディングの追加](./../mod/lua/guides/binding.md)
- luaドキュメント/API生成の改善
- [ハードコードされたC++機能をluaに移行](https://github.com/cataclysmbn/Cataclysm-BN/pull/6901)

PRタイトルの例：

```
feat(lua): add dialogue bindings
```

### `UI`: インターフェース

UI/UXの変更、例えば：

- マウスサポートの追加
- メニューの追加/調整
- ショートカットの変更
- ワークフローの合理化
- 生活の質の向上（QoL）

PRタイトルの例：

```
feat(UI): More info about items dropped with bags
feat(UI): overhaul encumbrance UI
```

### `i18n`: 国際化

翻訳およびその他の言語サポートの改善。

```
fix(UI,i18n): recipe names not translated unless learned
```

### `mods` または `mods/<MOD_ID>`: Mod

- Mod内に含まれる変更
- Mod内で可能なことの拡張

PRタイトルの例：

```
feat(mods/Magical_Nights): add missing owlbear pelts recipe
fix(mods): No Hope doesn't make the world freezing
```

### `balance`: バランス調整

ゲームバランスへの変更。

PRタイトルの例：

```
feat(balance): Give moose pelts instead of hides
```

### `port`: DDAまたは他のフォークからの移植

PRタイトルの例：

```
feat(port): game shop
```
