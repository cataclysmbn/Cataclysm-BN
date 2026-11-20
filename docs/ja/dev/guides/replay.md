---
title: 入力の記録と再生
---

`--replay-record PATH` でゲームの入力を記録し、`--replay-play PATH` で再生します。

```sh
./cataclysm-bn-tiles --userdir out/record/ --configdir out/record/config/ \
  --seed example --replay-record out/example.jsonl
./cataclysm-bn-tiles --userdir out/play/ --configdir out/play/config/ \
  --replay-play out/example.jsonl
```

両方の実行は、同じユーザーディレクトリのコピーから始めてください。リプレイは入力だけを記録するため、ワールド、設定、キー割り当ては復元しません。

ビルド、データ、MOD、オプション、ウィンドウサイズを同じにしてください。RNG シードはヘッダーに入っています。再生時に `--seed` を指定する場合は、ヘッダーの値と同じにしてください。

入力地点の不一致、未知の形式バージョン、不正なレコード、終了レコードの欠落があると、ファイルと行番号を示して再生を止めます。

## 2 つの実行の比較

両方の実行に `--state-hash-log PATH` を追加します。ログにはターンごとに 1 行、RNG、アバター、モンスター、NPC、アバター周辺のマップのハッシュが記録されます。2 つのファイルを比較すると、最初に異なるターンがわかります。
