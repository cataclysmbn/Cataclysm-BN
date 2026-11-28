# ターミナルキャプチャの録画

PR でバグや修正を見せるには、curses ビルドを asciinema で録画します。録画は `just term-capture` で短いアニメーション WebP にします。[PR #11041](https://github.com/cataclysmbn/Cataclysm-BN/pull/11041) に修正前と修正後のキャプチャがあります。

## 必要なもの

- curses ビルド: `cmake --preset linux-curses` と `cmake --build --preset linux-curses --target cataclysm-bn`（[CMake](./building/cmake.md) を参照）
- 録画用の [asciinema](https://docs.asciinema.org/)
- 録画のレンダリング用の [agg](https://github.com/asciinema/agg)
- Pillow でスクリプトを実行する [uv](https://docs.astral.sh/uv/)

## 録画

1. ターミナルを小さくします。例えば 120 列 40 行です。ターミナルが小さいほどファイルも小さくなります。
2. リポジトリのルートで、時間制限と別のユーザー・設定ディレクトリを指定して asciinema の中でゲームを起動します:

   ```sh
   asciinema rec -c 'timeout -k 10s 600 out/build/linux-curses/src/cataclysm-bn --userdir capture/ --configdir capture/config/' before.cast
   ```

   `--userdir` と `--configdir` を指定すると、ゲームは新しいセーブと設定を使い、普段のものは変更しません。`timeout` は 600 秒後にゲームを終了します。指定しないと、ターミナルのウィンドウを閉じてもゲームが動き続けることがあります。
3. デバッグメニューで場面を用意し、バグが出る操作をします。
4. ゲームのメニューからゲームを終了します。
5. 修正したビルドで同じように `after.cast` を録画します。

## WebP を作る

1. 録画を GIF にレンダリングします:

   ```sh
   just term-capture render before.cast before.gif
   ```

   GIF は 2 倍速で再生されます。1 秒より長い停止は 0.5 秒になります。
2. GIF の各フレームを PNG ファイルとして保存します:

   ```sh
   just term-capture frames before.gif before-frames/
   ```

3. プレイヤーの操作ごとに最初のフレームを探し、`scenes.json` に場面を 1 つずつ書きます。`start` は PNG ファイル名の番号です。字幕にはゲームに表示される言葉を使います。最後の場面は、バグなら `red`、修正なら `green` にします。

   ```json
   [
     { "start": 40, "caption": "enable UPS charging" },
     { "start": 67, "caption": "next turn: invalid ammo error", "color": "red" }
   ]
   ```

4. WebP を作ります。残す最後のフレームの番号を `--last` で指定します:

   ```sh
   just term-capture encode before.gif before.webp --scenes scenes.json --last 80
   ```

   WebP は最初の場面から始まり、`--last` のフレームで終わります。ゲームの開始画面やメニューなど、最初の場面より前のフレームは切り取られます。各場面の最後のフレームは 2 秒、キャプチャの最後のフレームは 6 秒止まります。エンコードには数分かかることがあります。
5. ブラウザで WebP を開き、各場面を確認します。

WebP は可逆圧縮です。非可逆の WebP ではターミナルの文字がぼやけ、ファイルも大きくなることが多いです。

## PR にキャプチャを追加する

WebP ファイルをフォークのブランチにプッシュします。例えば `assets/<issue 番号>` です。PR の Testing の項目に raw URL を書きます:

```md
![before](https://raw.githubusercontent.com/<you>/Cataclysm-BN/assets/9853/before.webp)
```

キャプチャを差し替えるには、同じパスに新しいファイルをプッシュします。PR の本文を編集する必要はありません。新しいファイルが表示されるまで数分かかることがあります。
