# ゲームのインストール方法

## ベースゲームをインストールする

- https://github.com/cataclysmbn/Cataclysm-BN にアクセスします。
- `Executables` で、必要なベースゲームのバージョンを選びます。

1. `Stable` はバグが少なめですが、最新コンテンツは含まれません。安定版は数か月ごとに出ます。
2. `Latest release` は Nightly ビルドが公開される場所です（毎晩 1 回リリース）。最新コンテンツを入手できますが、不安定な場合があります。
   <img width="834" height="170" alt="image" src="https://github.com/user-attachments/assets/c23f8629-dc4f-46c7-8f09-28f4d0f2cbfb" />

- どちらの場合でも、**ゲームを更新する前に必ずセーブデータをバックアップしてください**。できればゲームフォルダ全体をバックアップしてください。
- 使用している OS に合ったゲーム版を選びます。
  <img width="1231" height="639" alt="image" src="https://github.com/user-attachments/assets/9f82e154-1034-477b-b8ad-5c83c746dc16" />

- ゲームをインストールしたい場所に新しいフォルダを作成します（ここでは "CBN_TUTO_INSTALL" と呼びます）。
- ゲームの zip ファイルをダブルクリックし、すべてのファイルを選択して、新しく作成したフォルダへドラッグ＆ドロップします。
  <img width="1917" height="1077" alt="image" src="https://github.com/user-attachments/assets/ded02968-52ac-48a5-813c-1883885951cf" />

- おめでとうございます！ゲームはもうインストール済みです。`cataclysm-bn-tiles` というファイルをダブルクリックするとゲームを開始できます。

## 外部 Mod を入手する

- Bright Nights には初回プレイに十分な Mod がすでに含まれていますが、_さらに_ ほしい場合もあるでしょう。
- https://mods.cataclysmbn.org/ にアクセスし、`View all XXX mods` をクリックします。
  <img width="1903" height="930" alt="image" src="https://github.com/user-attachments/assets/08786ffd-6cff-46a5-afb1-83b5db997d7a" />

- 興味を引かれた Mod をクリックし、詳しい情報を確認します。
  <img width="1919" height="932" alt="image" src="https://github.com/user-attachments/assets/3a63722b-11f3-4fdb-b903-c8a9e0affdf3" />

- Mod をダウンロードするには、`Installation` の下にあるリンクをクリックします。
  <img width="1914" height="937" alt="image" src="https://github.com/user-attachments/assets/37052212-929d-4459-b383-a665d10f500e" />

- ベースゲームで行ったのと同じように、Mod ファイルを `/gamefolder/mods/` にドラッグ＆ドロップします。
- _Mod フォルダには modinfo.json ファイルが必要です。そのファイルを含むフォルダをドラッグ＆ドロップしてください。_
  <img width="1917" height="1076" alt="image" src="https://github.com/user-attachments/assets/da63f3ff-40e5-44c2-82cc-15bcb3e3e9b2" />

- Mod がインストールされました！

## ~~本物の~~ サウンドパックを入手する

- まず `/gamefolder/data/sound/` 内に、名前に "otopack" を含むフォルダがあるか確認します。ある場合はこの部分を飛ばして構いません。ない場合は次に進みます。
- https://mods.cataclysmbn.org/mods/otopack_bn_mk_2/ にアクセスします。
- `外部 Mod を入手する` の手順を繰り返しますが、今回はファイルを `/gamefolder/sound/` に入れます。
- 補足: `/gamefolder/` に `sound` フォルダが存在しない場合は作成してください。_`data` にも入れると動作はしますが、そこには何もインストールしないでください。ここはベースゲームファイル用に予約されています。_
  <img width="1914" height="1064" alt="image" src="https://github.com/user-attachments/assets/a71770dc-a032-4ed5-93a6-bf90c60e4a99" />

- サウンドパックがインストールされました！

## （任意）サウンドパック用の追加音楽を入手する

- このサウンドパック用の追加音楽がほしい場合は、こちらのリンク https://github.com/leoCottret/cbn-leocottret-mods/tree/main/MUSICS をクリックして手順を確認してください。

## ゲーム内ですべて有効にする

- ゲーム内でサウンドパックを有効にするには、`Settings` > `Options`（`Enter`）>\
  <img width="1924" height="1079" alt="image" src="https://github.com/user-attachments/assets/f59a43c1-c92a-4a32-b3b1-111d15e7b43e" />

- `General` > `Choose soundpack` "Otopack BN" > `ESC` キー > `Yes` を選択
  <img width="1914" height="1079" alt="image" src="https://github.com/user-attachments/assets/54c1af2a-be56-4bc8-85dd-4013ff73b85e" />

- 新しい Mod でゲームを始めるには、ワールド作成中にその Mod に合わせて `Enter` を押すだけです。
- `World` > `Create World` > 目的の Mod に移動 > `Enter` を押す
  <img width="1916" height="1079" alt="image" src="https://github.com/user-attachments/assets/7da9ea69-d95e-4d87-8285-57242dc2efd0" />

- 黄金律: ゲーム中のどこでも `?` を押すと、その場面で使えるキーを確認（および変更）できます。
