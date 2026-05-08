# UDP タイルセットのビルド

現在、https://github.com/Theawesomeboophis 氏が UndeadPeople タイルセットに取り組んでいます。

# 彼の Discord サーバー: https://discord.gg/ftgMS5Rcsd

> compose.py の設定方法と、unpacked の適切な使用方法を教えてください。

1. UndeadPeopleUnpacked リポジトリのフォーク アンパックされたタイルセットが含まれているリポジトリ
   https://github.com/Theawesomeboophis/UndeadPeopleUnpacked をフォークしてください。
   tileset.
2. Python のインストール:https://www.python.org/downloads/ から最新バージョンを使用してください。
   インストーラの設定で「Add Python to PATH」（Python を PATH に追加）にチェックが入っていることを確認してください。
3. Libvips のインストール: Libvips をインストールします。ダウンロードはこちらから:
   https://github.com/libvips/build-win64-mxe/releases 。
   vips-dev-w64-all-8.11.3.zip (またはそれ以降のバージョン) を入手してください。 これを任意のディレクトリ
   (例:`C:\vips`)に展開し、`C:\vips\bin` を Windows の PATH に追加します。PATH への追加方法については、こちらを参照してください: https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/ (私は Libvips に別のディレクトリを使用しています）)
   ![screenshot 2021-09-13 001](https://user-images.githubusercontent.com/17512620/133093842-ef200cef-898a-4b5b-8a8e-23588e768483.png)

4. pyvips のインストール: pyvips をインストールします。コンソールで次のように入力するだけです。
   ![screenshot 2021-09-13 003](https://user-images.githubusercontent.com/17512620/133094097-04750819-c729-473c-a1c9-f87b00e5bf9c.png)

これで完了です。compose.py が動作するようになります。

最終処理:

その後、`\UndeadPeopleUnpacked\!COMPOSE_MAIN.bat` を実行して、タイルセットがパッキング（圧縮）されているかどうかを確認してください。すべてが正しく設定されていれば、パッキングされたファイルは `UndeadPeopleUnpacked\!dda\` 内に見つかります。これには時間がかかります。

![screenshot 2021-09-13 004](https://user-images.githubusercontent.com/17512620/133094442-30e28aad-4304-4710-8674-7314f2987473.png)

![screenshot 2021-09-13 005](https://user-images.githubusercontent.com/17512620/133094858-e123f137-a8e9-4d69-bae4-d28c065d9a81.png)

そして、ここにパックされたタイルセットがあります。
