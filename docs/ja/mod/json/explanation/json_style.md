---
title: JSON Style Guide
sidebar:
  badge:
    text: Unstable
    variant: caution
---

[C++ コードスタイル](../../../dev/explanation/code_style.md)と同様に、JSONのスタイリングポリシーは、開発への不当な混乱を防ぐため、JSONが追加または編集される際は、比較的小さなまとまり単位で、JSONを更新することです。

## なぜ独自のJSONフォーマッタがあるのか？

DDA は独自のJSONパーサーを記述しました。それは `tools/format/format.cpp` に存在し、
`src/json.cpp`を利用してJSONをパースし、出力します。

これは、既存のJSONフォーマッタ (例: `deno fmt`) の使用を不可能にするため、最適な解決策ではありませんが、[前回の試み](https://github.com/cataclysmbnteam/Cataclysm-BN/pull/3118)では、不利益が利益を上回ることが証明されました。

## JSONの記述例

この例は、スタイリング機能のほとんどを概説しています。

```json
[
  {
    "type": "foo",
    "id": "example",
    "short_array": [1, 2, 3, 4, 5],
    "short_object": {
      "item_a": "a",
      "item_b": "b"
    },
    "long_array": [
      "a really long string to illustrate line wrapping, ",
      "which occurs if the line is longer than 120 characters"
    ],
    "nested_array": [
      [
        ["item1", "value1"],
        ["item2", "value2"],
        ["item3", "value3"],
        ["item4", "value4"],
        ["item5", "value5"],
        ["item6", "value6"]
      ]
    ]
  }
]
```

インデントは2スペースです。カンマとコロンを除くすべてのJSON区切り文字は、空白(スペースまたは改行) で囲まれます。カンマとコロンの後ろには空白が続きます。
オブジェクト内のエントリは、常に改行で区切られます。配列内のエントリは、結果の行が120文字(インデントを含む)を超過する場合に、改行で区切られます。改行は、開き角括弧、閉じ角括弧、またはエントリの後ろで発生します。

## 整形ツール

整形ツールは、Makefileを介して、または (`make style-json`でビルドされた)
`tools/format/json_formatter.cgi`として直接、あるいはCGIとして
http://dev.narc.ro/cataclysm/format.html から呼び出すことができます。

Visual Studioソリューションを使用している場合、プロジェクト内のすべてのJSONを整形するためのコマンドをVisual Studioに設定できます。

1. ソリューション全体またはJsonFormatterプロジェクトのみをビルドして、
   JsonFormatterプロジェクトをビルドします。これにより、
   `tools/format/json_formatter.exe` バイナリが作成されます。
2. 新規の外部ツールエントリ ( `ツール` > `外部ツール..` > `追加` ) を追加し、以下
   のように設定します。
   - タイトル: `Lint All JSON`
   - コマンド: `C:\windows\system32\windowspowershell\v1.0\powershell.exe`
   - 引数: `-file $(SolutionDir)\style-json.ps1`
   - 初期ディレクトリ: `$(SolutionDir)`
   - 出力ウィンドウの使用: チェックを入れる

この時点で、メニュー ( `ツール` > `Lint All JSON` ) を使用してコマンドを呼び出すことができ、実行結果を出力ウィンドウで確認できます。さらに、 `ツール` > `オプション` > `環境` > `キーボード`に移動し、
`Tools.ExternalCommand`を含むコマンドを検索し、リスト内でのコマンドの位置に対応するもの (例:リストの最上位であれば`Tools.ExternalCommand1`) を選択することで、このコマンドにキーバインドを設定できます。

### 単一ファイルの場合

一つのJSONファイルを整形するには、`json_formatter.exe path/to/file.json`を実行します。これにより、そのファイルが整形されます。「needs linting」と表示された場合、そのファイルは整形されていませんでしたが、現在は整形されているはずです。
ドラッグ&ドロップを使用することも可能です。JSONファイルを `json_formatter` のアイコンにドラッグし、数秒間待ちます。

### *nix環境の場合

メインのリポジトリディレクトリで `make style-json` を実行します。
