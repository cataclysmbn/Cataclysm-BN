# CMake + Visual Studio + Vcpkgを用いたビルド手順

> [!CAUTION]
>
> CMake ビルドは現在、開発途上(WIP)です。

## 前提条件

- `cmake` バージョン3.24.0以上
- [vcpkg.io](https://vcpkg.io/en/getting-started.html)からインストールされた`vcpkg`

注釈: Visual Studio 2022 バージョン 17.6 以降では、vcpkg はディストリビューションに含まれており、Visual Studio Developer Command Prompt (VS 開発者コマンド プロンプト) から利用可能です。この場合、別途インストールする必要はありません。

## 設定

設定は、`CMakePresets.json`内のいずれかのプリセットを使用して行えます。これらのプリセットはすべて、コードをディレクトリ `out/build/<preset>/`内にビルドします。

### ターミナル

`cmake` が `vcpkg`を見つけられることを確認してください。見つけられない場合、パッケージが見つからないというエラーが発生します。以下のいずれかの方法で対応できます。

- VS2022 ユーザーで `vcpkg`がプレインストールされている場合、プレーンなターミナルではなく VS 開発者コマンド プロンプト を実行していれば、`vcpkg` はすで利用可能になっているはずです。
- 任意のcmake configureコマンドに `-DVCPKG_ROOT=C:\dev\vcpkg` (または適切なパス) を追記します。
- 環境変数 `VCPKG_ROOT` に `vcpkg` のチェックアウト先へのパスを設定します。
- `CMakePresets.json` の中に、`VCPKG_ROOT` キャッシュ変数として適切なパスを追加します(ただし、後でコードを扱う予定がある場合、Gitがこのファイルを追跡するため非推奨です)。

以下のコマンドを実行します。

```sh
cmake --list-presets
```

これにより、利用可能なプリセットが表示されます。リストは使用中の環境によって変化します。リストが空の場合、その環境はサポートされていません。

以下のコマンドを実行します。

```sh
cmake --preset <preset>
```

`vcpkg` のインストールが利用可能であれば、これによりすべての依存関係がダウンロードされ、ビルドファイルが生成されます。

VS2022を使用している場合は、名前に `2022` が含まれるプリセットを必ず選択してください。サフィックスがないプリセットは VS2019 をターゲットとします。

このコマンドに `-Doption=value` を追記することで、任意のオプションを上書きできます。詳細については、CMakeガイドの
[ビルドオプション](./cmake.md/#build-options) を参照してください。例えば、テストが不要な場合は、 `-DTESTS=OFF` を使用してテストのビルドを無効にできます。

### Visual Studio

ゲームのソースフォルダを Visual Studio で開きます。

Visual Studio はフォルダを CMake プロジェクトとして認識し、設定を開始しようとする場合がありますが、適切なプリセットを使用していないため、設定は失敗する可能性が高いです。

標準ツールバー の `Configuration` ドロップダウンボックスにプリセットが表示されます。適切なもの
(`windows` と `msvc`を含むはず)を選択し、メインメニューから `プロジェクト` ->
`キャッシュ構成`を選択します。

VS2022 を使用している場合は、名前に `2022` が含まれるプリセットを必ず選択してください。サフィックスがないプリセットは VS2019 をターゲットとします。

## ビルド

### ターミナル

以下のコマンドを実行します。

- `cmake --build --preset <preset> --config Release`

`Release` を `Debug` に置き換えてデバッグビルドを取得したり、`RelWithDebInfo` に置き換えて最適化は少ないがデバッグ情報が多いリリースビルドを取得したりできます。

### Visual Studio

標準ツールバーの `Build Preset` ドロップダウンメニューからビルドプリセットを選択します。メインメニューから `ビルド` -> `すべてビルド`を選択します。

`Release`、`Debug`、`RelWithDebInfo` のビルドタイプを切り替えることも可能ですが、UIレイアウトによっては、このドロップダウンメニューがオーバーフローボタンの背後に隠れている場合があります。

## 翻訳

翻訳はオプションであり、`gettext`パッケージの`msgfmt` バイナリが必要です。 `vcpkg` が自動的にインストールするはずです。

### ターミナル

以下のコマンドを実行します。

- `cmake --build --preset <preset> --target translations_compile`

### Visual Studio

Visual Studio は前のステップで翻訳をビルドしているはずです。ビルドされていない場合は、ソリューションエクスプローラーを開き、CMake ターゲットモードに切り替え（右クリックで実行可能）、
`translations_compile` ターゲットを右クリック -> `Build translations_compile`を実行します。

## インストール

> [!CAUTION]
>
> インストール機能はまだ 作業中(WIP)と見なされており、テストはほとんど行われていません。

### Visual Studio

メインメニューから `ビルド` -> `CataclysmBN のインストール` を選択します。

### ターミナル

以下のコマンドを実行します。

- `cmake --install out/build/<preset>/ --config Release`

`Release` を選択したビルドタイプに置き換えてください。

## 実行

ゲーム実行ファイルとテスト実行ファイルは、両方とも `.\Release\` フォルダ内にあります（フォルダ名はビルドタイプと一致するため、他のビルドタイプでは他のフォルダ名になります）。

データファイルが現在のパスにあることをゲームがデフォルトで期待するため、プロジェクトのトップディレクトリから実行する限り、ターミナルから手動で実行できます。

Visual Studio からの実行とデバッグについては、生成された VS ソリューションファイル `out\build\<preset>\CataclysmBN.sln` を開き（前のステップを IDE またはターミナルで完了したかどうかに関係なくそこに存在します）、以降の作業はこれで行うことを推奨します。

あるいは、「Open Folder」モードのままでいることも可能ですが、その場合、ゲーム実行ファイル（およびテスト）の起動設定をカスタマイズする必要があり、他にもまだ発見されていない副作用があるかもしれません。

### ターミナル

ゲームを起動するには、以下を実行します。

- `.\Release\cataclysm-bn-tiles.exe`

テストを実行するには、以下を実行します。

- `.\Release\cata_test-tiles.exe`

### Visual Studio (省略可能 1、推奨)

Visual Studio を閉じ、`out\build\<preset>\` に移動して `CataclysmBN.sln`を開きます。
`cataclysm-bn-tiles` をスタートアッププロジェクトとして設定し（ソリューションエクスプローラーで右クリックから実行可能）、追加の問題なくゲーム実行ファイルを実行およびデバッグできるようになります。これは、データファイルをトッププロジェクトディレクトリ内で探すように事前設定されています。

テストを実行するには、スタートアッププロジェクトを `cata_test-tiles`に切り替えます。

### Visual Studio (省略可能 2)

Visual Studio が CMake プロジェクトを処理する方法により、VSが「フォルダを開く」モードにある間は、実行可能ファイルのワーキングディレクトリ（作業ディレクトリ） を指定することは不可能です。このStackOverflowの回答がこの点をうまく説明しています: https://stackoverflow.com/a/62309569 幸いなことに、VSは実行可能ファイルの起動オプションを個別にカスタマイズできます。

ソリューションエクスプローラーを開き、まだ行っていない場合は CMake ターゲットモード に切り替えます（右クリックで実行可能）。そこで、 `cataclysm-bn-tiles` ターゲットを右クリック ->
`デバッグ設定の追加`を選択します。Visual Studio はこのプロジェクトの起動設定ファイルを開き、 `cataclysm-bn-tiles` ターゲットの新しい設定が追加されます。以下の行を設定に追加してファイルを保存します。

```
"currentDir": "${workspaceRoot}",
```

最終的な結果は以下のようになります。

```json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "currentDir": "${workspaceRoot}",
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)",
      "name": "cataclysm-bn-tiles.exe (<PATH_TO_SOURCE_FOLDER>\\Debug\\cataclysm-bn-tiles.exe)"
    }
  ]
}
```

これで、Visual Studio 内からゲーム実行ファイルを実行およびデバッグできるようになるはずです。

テストを実行したい場合は、`cata_test-tiles` ターゲットに対してこのプロセスを繰り返します。
