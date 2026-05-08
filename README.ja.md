# Cataclysm: Bright Nights

<header align="center">
  <a><img src="docs/en/contribute/img/readme-title.png" title="screenshots of (clockwise from upper-right: Chaosvolt (x2), ExecutorBill, scarf005"></a>

[![en][icon-en]][en] [![ko][icon-ko]][ko] [![ja][icon-ja]][ja]

</header>

[en]: ./README.md
[icon-en]: https://img.shields.io/badge/lang-en-red?style=flat-square
[ko]: ./README.ko.md
[icon-ko]: https://img.shields.io/badge/lang-ko-orange?style=flat-square
[ja]: ./README.ja.md
[icon-ja]: https://img.shields.io/badge/lang-ja-green?style=flat-square

Cataclysm: Bright Nightsは、ポストアポカリプスの世界を舞台にしたSF要素を含むローグライクゲームです。

「ゾンビゲーム」と評されることもありますが、Cataclysmにはそれ以上のものがあります。過酷で永続的な、手続き的に生成される世界で生き延びるために奮闘してください。食料や装備を求めて死んだ文明の残骸を漁り、運が良ければ満タンのガソリンタンクを持つ車両を見つけて、そこから脱出しましょう。

ゾンビから巨大昆虫、殺人ロボット、そしてさらに奇妙で危険なものまで、多種多様な強力なモンスターと戦って倒すか逃げるかし、そしてあなたの持ち物を狙う、あなたと同じような他の生存者たちとも対峙してください。

大災厄を止める方法を見つけるか...それとも最強のモンスターとなるか。

> Cataclysm: Bright NightsはCataclysm: Dark Days Aheadのフォークです。
> [CDDAとの違いをご覧ください](https://docs.cataclysmbn.org/game/changelog/).

## ダウンロード

### 実行ファイル

[![Stable][stable-releases-badge]][stable-releases] [![Recent][all-releases-badge]][all-releases] [![Experiemental][experimental-badge]][experimental-releases] [![Flatpak][flathub-badge]][flathub-releases]

#### Linuxでの手順

ゲームが依存する多くの依存関係はデフォルトでインストールされている可能性が高いですが、一部はディストリビューションにデフォルトでインストールされていない可能性があります。

人気のあるディストリビューションファミリー向けのコマンドは以下の通りです:

- Ubuntu / Debian: `sudo apt install libsdl2-image-2.0-0 libsdl2-ttf-2.0-0 libsdl2-mixer-2.0-0 libfreetype6 zip libsqlite3-0`
- Fedora: `sudo dnf install SDL2 SDL2_image SDL2_ttf SDL2_mixer freetype zip sqlite`
- Arch: `sudo pacman -S sdl2 sdl2_image sdl2_ttf sdl2_mixer zip sqlite`

### ランチャー

主にサポートされているランチャーは[Catapult](https://github.com/qrrk/Catapult)で、BNとDDAの両方を扱うことができます(デフォルトはDDAなので、トップメニューで必ず変更してください!)

### サードパーティMod

Bright Nightsは、メインゲームに同梱されていないModを簡単に見つけられるよう[Mod レジストリ](https://mods.cataclysmbn.org/)を使用しています。

### ソースコード

[![Source Code][source-badge]][source] [![Zip Archive][clone-badge]][clone]

[stable-releases]: https://github.com/cataclysmbn/Cataclysm-BN/releases/latest "安定版の実行ファイルをダウンロード"
[stable-releases-badge]: https://img.shields.io/github/v/release/cataclysmbn/Cataclysm-BN?style=for-the-badge&color=success&label=stable
[all-releases]: https://github.com/cataclysmbn/Cataclysm-BN/releases?q=prerelease%3Atrue&expanded=true
[all-releases-badge]: https://img.shields.io/github/v/release/cataclysmbn/Cataclysm-BN?style=for-the-badge&color=important&label=Latest%20Release&include_prereleases&sort=date
[experimental-releases]: https://github.com/cataclysmbn/Cataclysm-BN/releases/tag/experimental
[experimental-badge]: https://img.shields.io/github/v/release/cataclysmbn/Cataclysm-BN?style=for-the-badge&color=salmon&label=Experimental%20Release&include_prereleases&sort=date
[flathub-releases]: https://flathub.org/apps/org.cataclysmbn.CataclysmBN
[flathub-badge]: https://img.shields.io/flathub/v/org.cataclysmbn.CataclysmBN?style=for-the-badge&color=success
[source]: https://github.com/cataclysmbn/Cataclysm-BN/archive/master.zip "ソースコードは.zip形式のアーカイブとしてダウンロードできます"
[source-badge]: https://img.shields.io/badge/Zip%20Archive-black?style=for-the-badge&logo=github
[clone]: https://github.com/cataclysmbn/Cataclysm-BN/ "GitHubリポジトリからクローンする"
[clone-badge]: https://img.shields.io/badge/Clone%20From%20Repo-black?style=for-the-badge&logo=github

## ビルド方法

- [CMake](docs/en/dev/guides/building/cmake.md)
- [makefile](docs/en/dev/guides/building/makefile.md): Linux、macOS、BSDをサポート
- [MSYS2](docs/en/dev/guides/building/msys.md)
- [vcpkg](docs/en/dev/guides/building/vs_vcpkg.md)

詳細は[公式ドキュメント](https://docs.cataclysmbn.org/dev/guides/building/cmake/)をお読みください。

## 貢献

> Cataclysm: Bright NightsはCreative Commons Attribution ShareAlike 3.0ライセンスの下で開発されています。
> ゲームのコードとコンテンツは、いかなる目的においても自由に使用、修正、再配布することができます。
> 詳細はhttp://creativecommons.org/licenses/by-sa/3.0/ をご覧ください。プロジェクトと共に配布されている
> 一部のコードはプロジェクトの一部ではなく、異なるソフトウェアライセンスの下でリリースされており、
> 異なるソフトウェアライセンスの対象となるファイルには独自のライセンス通知があります。

詳細は[公式ドキュメント](https://docs.cataclysmbn.org/contribute/contributing/)をご確認ください。

## ドキュメント

ゲームプレイと開発のドキュメントは、マークダウン形式で [doc](./docs/)ディレクトリにあります。以下も利用できます:

- [公式ドキュメント](https://docs.cataclysmbn.org/)サイトを訪問
- [ドキュメントをローカルでビルドして提供](./docs/en/contribute/docs.md)

## コミュニティ

[![公式ドキュメント](https://img.shields.io/badge/Docs-LightGray?style=for-the-badge)][docs]
[![Discussions](https://img.shields.io/badge/Discussions-black?style=for-the-badge&logo=github)][discussion]
[![Discord](https://img.shields.io/discord/830879262763909202?style=for-the-badge&logo=discord)][discord]
[![Discussions](https://img.shields.io/badge/CDDA%20Modding-green?style=for-the-badge&logo=discord)][modding]

[discussion]: https://github.com/cataclysmbn/Cataclysm-BN/discussions
[discord]: https://discord.gg/XW7XhXuZ89
[modding]: https://discord.gg/B5q4XCa "非公式のDDAモッディングコミュニティDiscordにはBNチャンネルがあります"
[docs]: https://docs.cataclysmbn.org "公式BNドキュメント"

## よくある質問

#### チュートリアルはありますか?

はい、メインメニューの**Special**メニューでチュートリアルを見つけることができます(多くのコード変更により、チュートリアルが機能しない可能性があることに注意してください)。ゲーム内でも`?`キーでドキュメントにアクセスできます。

#### キーバインドを変更するにはどうすればよいですか?

`?`キーを押し、次に`1`キーを押すと、キーコマンドの完全なリストが表示されます。`+`キーを押してキーバインドを追加し、対応する文字キー`a-w`でアクションを選択し、そのアクションに割り当てたいキーを押します。

#### 新しいワールドを開始するにはどうすればよいですか?

メインメニューの**世界**で新しいワールドが生成されます。**世界生成**選択してください。

#### ゲームに音楽(または音)がありません。どうすれば追加できますか?

[Catapult](https://github.com/qrrk/Catapult)などのサードパーティランチャーを使用すると、ランチャーメニューからサウンドパック(およびMod)をインストールできます。手動でも可能です。サウンドパックを追加するには、ダウンロードして`<Game Folder>\data\sound\`に解凍し、設定から選択して、ゲームを再起動します。推奨されるのは[Otopack](https://github.com/NarandBD/Otopack-BN-Mk-2)です。

#### サードパーティModはどこに置けばよいですか?

サードパーティModを置く場所は、手動でインストールしたかCatapultでインストールしたかによって異なります。どちらの場合でも、サードパーティModはdata/modsには**入れません**。このフォルダはリポジトリ内のModのみに予約されるべきです。特にCatapultはこのフォルダの内容を自動的に削除するためです!

手動インストールの場合は、player-modsフォルダを使用してください(存在しない場合は、古いバージョンを使用しているため手動でcataclysmベースフォルダにフォルダを作成するか、サポートされているプラットフォームでXDGまたはHomeディレクトリを使用しています)

Catapultの場合は、Catapultインストール内のbn/userdata/modsに配置します(フォルダがまだない場合は作成します)。例えば、Catapult/bn/userdata/modsのようになります。

XDGディレクトリを使用しているLinuxユーザー(flatpakではない)の場合: ユーザーModディレクトリは`~/.local/share/cataclysm-bn/mods`にあります(`~/.local/share/cataclysm-bn`が一般的なユーザーディレクトリです)

flatpakユーザーの場合、ユーザーModフォルダは`~/.var/app/org.cataclysmbn.CataclysmBN/data/cataclysm-bn/mods`です(一般的なユーザーディレクトリは`~/.var/app/org.cataclysmbn.CataclysmBN/data/cataclysm-bn/`です)

#### ゲームを手動で更新するにはどうすればよいですか?

Modを適切に管理していると仮定すると、正しい更新プロセスは、古いdataフォルダを削除し(安全のためにgfxフォルダも削除したい場合)、その後古いBNフォルダの内容を新しいBNダウンロードで上書きすることです。古いdataフォルダの削除は、単に古いフォルダを上書きするだけでは、obsoletionフォルダなどで発生する可能性があるファイルを削除する更新に対応できないため、とても重要です。

savesフォルダ、memorial、graveyardなどは削除しないでください!

あるいは、常にCatapultランチャーを使用して更新を処理させることもできます。バニラゲームを正しく更新する実績があります。

#### バグを見つけました。どうすればよいですか?

[バグレポート](https://github.com/cataclysmbn/Cataclysm-BN/issues/new?template=bug_report.yml)は、デバッグメニューから送信できます。

ゲーム内で`GitHubでバグ報告を提出する` を実行して問題を送信してください。

| 1. メインメニュー (ESC) -> デバッグメニュー (b)を開く |              2. 情報(i)を開く               |
| :---------------------------------------------------: | :-----------------------------------------: |
|      ![](docs/en/contribute/img/readme-bug1.png)      | ![](docs/en/contribute/img/readme-bug2.png) |
|           3. GitHubでバグ報告を提出する (U)           |       4.issueへのリンクが生成されます       |
|      ![](docs/en/contribute/img/readme-bug3.png)      | ![](docs/en/contribute/img/readme-bug4.png) |

`バージョンと設定`が記入されたバグレポートがブラウザで開きます。

#### 提案をしたいのですが、どうすればよいですか?

- シンプルなアイデアの場合
  [Discussionsページ](https://github.com/cataclysmbn/Cataclysm-BN/discussions/categories/ideas)
  をご覧ください。新機能、移植リクエスト、Modアイデア、その他何でも構いません。
- [GitHubページ](https://github.com/cataclysmbn/Cataclysm-BN/issues/)で
  [新機能リクエストフォーム](https://github.com/cataclysmbn/Cataclysm-BN/issues/new?
  template=feature_request.yml)を使用して問題を送信してください。
