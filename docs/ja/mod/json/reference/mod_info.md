# MOD_INFO

[MODDING.md](../../../../en/mod/json/tutorial/modding.md) も参照してください。

`MOD_INFO` 型のオブジェクトは、モッド自体を説明します。各モッドは必ず 1 つだけ `MOD_INFO` を持ち、
他のモッド用オブジェクトとは異なり、ゲーム起動時にタイトル画面が表示される前に読み込まれます。
そのため、これに関するエラーはすべてタイトル画面が表示される前に出ます。

現在の慣例では、モッドのルートディレクトリにある `mod_info.json` に `MOD_INFO` を書きます。

例:

```json
[
  {
    "type": "MOD_INFO",

    // モッドの一意な識別子です。分かりやすさのため、ASCII の文字・数字・アンダースコアのみを推奨します。
    "id": "better_zeds",
    // モッドのカテゴリです。対応している値の一覧は MODDING.md を参照してください。
    "category": "content",
    // モッドの表示名です。英語で書きます。
    "name": "Better Zombies",
    // モッドの説明です。英語で書きます。
    "description": "Reworks all base game zombies and adds 100+ new variants.",
    // モッドのライセンスです。非 FOSS の場合は通常 "All Rights Reserved" または "Source Available" を使います。
    "license": "CC-BY-SA 4.0 Unported",
    // モッドの元の作者です。
    "authors": ["That Guy", "His Friend"],
    // 作者が何らかの理由でモッドを放棄した場合、現在の保守担当者をここに列挙します。
    "maintainers": ["Mr. BugFixer", "BugFixer Jr."],
    // 任意のローディング画面画像パスです。画像ファイルまたは再帰的に検索するディレクトリを指定できます。
    // "path" がある場合はそのディレクトリ基準で解決し、互換性のため modinfo.json のあるディレクトリも検索します。
    // 対応する画像拡張子は .png, .jpg, .jpeg, .bmp, .gif, .webp です。
    // 作者表示は画像ファイル名の最初のアンダースコアより前の部分を使います。例: "foo_rest.png" は黒いアウトライン付きの白い "by foo" と表示されます。
    "loading_images": ["load_1.png", "load_2.png", "some_directory"],
    // モッドのバージョン文字列です。利用者や保守者の便宜のためのものなので、日付など好きな形式を使えます。
    "version": "2021-12-02",
    // モッドの依存関係一覧です。依存先はこのモッドより先に読み込まれることが保証されます。
    "dependencies": ["bn", "zed_templates"],
    // このモッドと互換性のないモッド一覧です。
    "conflicts": ["worse_zeds"],
    // コアゲームデータ用の特別なフラグです。トータルオーバーホールモッドのみ使用できます。同時に読み込めるコアモッドは 1 つだけです。
    "core": false,
    // モッドを廃止扱いにします。廃止モッドはデフォルトではモッド選択一覧に表示されず、警告付きになります。
    "obsolete": false,
    // modinfo.json から見たモッドファイルの相対パスです。ゲームは modinfo.json があるフォルダとそのすべてのサブフォルダを自動で読み込むため、
    // この項目は何らかの理由で modinfo.json をモッドのサブフォルダに置きたい場合にのみ役立ちます。
    "path": "../common-data/"
  }
]
```
