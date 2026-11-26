# Visual Studio 2022 と CMake によるビルド

このガイドでは、Visual Studio 2022 のネイティブ CMake 統合を使って Windows 上で Cataclysm: Bright Nights をビルドする方法を説明します。一度設定すれば、構成、ビルド、デバッグはすべて外部ツールなしで Visual Studio 内から行えます。

> **レガシービルド:** `msvc-full-features/` の `.sln` ベースのビルドは引き続き動作し、このシステムの影響を受けません。同じチェックアウトで両方を使えます。

## 仕組み

プロジェクトには2つの CMake 構成ファイルがあります:

| ファイル             | 用途                 |
| -------------------- | -------------------- |
| `CMakeSettings.json` | Visual Studio IDE    |
| `CMakePresets.json`  | cmake CLI、CI、Linux |

フォルダーを開くと VS は `CMakeSettings.json` を直接読み込みます。VS を開く前に手動で cmake を構成する必要はありません。

> **VS の設定:** **Tools → Options → CMake** で _"When a CMakeSettings.json or CMakePresets.json file is detected"_ を **"Use CMakeSettings.json (Legacy)"** または **"Never use CMake Presets"** に設定してください。これにより VS は `CMakeSettings.json` を使い、`CMakePresets.json` を無視します。

> [!TIP]
>
> Visual Studio から起動するプロンプトベースのワークフローを使いたい場合は、[Visual Studio 外部ツール自動化 (Windows + WSL)](./vs_external_tool_wsl.md) を参照してください。

## 前提条件

| ツール                                            | 最小バージョン | 入手先                                                            |
| ------------------------------------------------- | -------------- | ----------------------------------------------------------------- |
| Visual Studio 2022                                | 17.6           | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| VS ワークロード: **Desktop development with C++** | —              | VS Installer                                                      |
| cmake                                             | 3.24           | 上記の VS ワークロードに含まれる                                  |
| ninja                                             | 任意           | 上記の VS ワークロードに含まれる                                  |
| vcpkg                                             | 任意           | VS 2022 17.6+ に含まれる（以下を参照）                            |
| git                                               | 任意           | [git-scm.com](https://git-scm.com/)                               |

### vcpkg

Visual Studio 2022 17.6 以降には vcpkg が含まれています。推奨のインストールオプションを選択していれば、すでに存在します。VS 開発者環境が設定する `VCPKG_INSTALLATION_ROOT` 環境変数を通して CMake が自動的に見つけます。

スタンドアロンの vcpkg を別途インストールした場合は、`VCPKG_ROOT` をそのパスに設定してください。

---

## Visual Studio での日常的なワークフロー

### 1. フォルダーを開く

Visual Studio 2022 を開き、**File → Open → Folder…** を選んで `CMakeLists.txt` を含むプロジェクトのルートディレクトリを指定します。

`msvc-full-features/` の `.sln` ファイルは開かないでください。これはレガシービルドシステムであり、2つは別のシステムです。

### 2. 構成を選択する

標準ツールバーの **Configuration** ドロップダウンから選択します:

| 構成             | 用途                                         |
| ---------------- | -------------------------------------------- |
| `Debug`          | デバッグ、すべてのシンボル、最適化なし       |
| `RelWithDebInfo` | 通常の開発 — 最適化済みでデバッグ可能        |
| `Release`        | 性能テスト、配布                             |
| `Tests`          | テストスイートのビルドと実行                 |
| `Tracy`          | Tracy プロファイラによる性能プロファイリング |

> **RelWithDebInfo** は日常的な開発のデフォルトとして最適です。ゲームを通常速度で実行しながら、ブレークポイントとスタックトレースに十分なデバッグ情報を保持します。

`Tests` 構成はテストスイートを有効にした RelWithDebInfo ビルドです。それ以外の構成ではビルド時間を短くするためテストを無効にしています。

`Tracy` 構成は Tracy の計測を含む Release ビルドです。[Tracy プロファイリング](#tracy-プロファイリング)を参照してください。

### 3. ビルド

**Build → Build All**（または `Ctrl+Shift+B`）を選択します。

初回のビルドでは vcpkg の依存関係をダウンロードしてコンパイルするため時間がかかります。以後はインクリメンタルビルドになります。

### 4. 実行とデバッグ

ツールバーで起動項目を選択します:

| 構成                                     | 起動項目                   |
| ---------------------------------------- | -------------------------- |
| Debug / RelWithDebInfo / Release / Tracy | **cataclysm-bn-tiles.exe** |
| Tests                                    | **cata_test-tiles.exe**    |

次に **F5** を押します。

作業ディレクトリは `launch.vs.json` によりプロジェクトルートに設定されるため、追加設定なしでゲームがデータファイルを見つけます。

---

## ビルドのカスタマイズ

ローカルビルドの cmake 変数を上書きするには、`CMakeSettings.json` を開き、使用する構成の `variables` 配列に項目を追加します。このファイルは git で追跡されるため、個人用の変更はローカルブランチで行うか、構成を別名でコピーしてください。

### 便利な変数

| 変数          | デフォルト                 | 効果                          |
| ------------- | -------------------------- | ----------------------------- |
| `TESTS`       | `OFF`（Tests 構成では ON） | テストスイートをビルド        |
| `JSON_FORMAT` | `ON`                       | JSON formatter ツールをビルド |
| `LOCALIZE`    | `ON`                       | 翻訳サポートをビルド          |
| `SOUND`       | `ON`                       | オーディオサポートをビルド    |

---

## Tracy プロファイリング

[Tracy](https://github.com/wolfpld/tracy) はリアルタイムのフレームプロファイラです。VS ツールバーで **Tracy** 構成を選び、通常どおりビルドします。Tracy は `TRACY_ON_DEMAND` モードを使用するため、Tracy ビューアーが接続して録画を開始したときだけプロファイリングし、ビューアーなしでもゲームを使用できます。

ターミナルワークフローでも `windows-tiles-sounds-x64-msvc-tracy` CMake プリセットから Tracy を使えます。[ターミナルワークフロー](#ターミナルワークフロー)を参照してください。

---

## ターミナルワークフロー

`setup.ps1` は前提条件を検証し、ターミナルビルド用の CMake プリセットを構成します。通常の PowerShell ウィンドウから一度実行します:

```powershell
.\setup.ps1
```

このスクリプトは前提条件を確認し、翻訳のビルドに必要な gettext バイナリをダウンロードしてから `cmake --preset windows-tiles-sounds-x64-msvc` を実行します。

その後、**VS 2022 Developer Command Prompt** または **Developer PowerShell** から標準の cmake コマンドを使えます:

```powershell
# 一度だけ構成（または CMakeLists.txt の変更後）
cmake --preset windows-tiles-sounds-x64-msvc

# ビルド
cmake --build --preset windows-msvc-relwithdebinfo

# プロジェクトルートからゲームを実行
.\out\build\windows-tiles-sounds-x64-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe

# テストを実行
.\out\build\windows-tiles-sounds-x64-msvc\tests\RelWithDebInfo\cata_test-tiles.exe

# 翻訳だけをビルド
cmake --build --preset windows-msvc-relwithdebinfo --target translations_compile

# インストール（ゲームとデータを自己完結型ディレクトリにコピー）
cmake --install out\build\windows-tiles-sounds-x64-msvc --config RelWithDebInfo
```

> **注:** 通常のターミナル（VS 開発者ターミナルではない）から `cmake --build` を実行すると、`CMakeUserPresets.json` に保存された VS 環境に依存します。このファイルがない場合は `setup.ps1` で再生成するか、VS 開発者コマンドプロンプトを使ってください。

---

## トラブルシューティング

### CMake の構成が直ちに失敗する

**最も一般的な原因:** vcpkg が見つからない。

`VCPKG_ROOT` が設定されている（または VS 付属の vcpkg が利用可能な）ことを確認します。VS 開発者コマンドプロンプトを開いて次を実行してください:

```
echo %VCPKG_ROOT%
echo %VCPKG_INSTALLATION_ROOT%
```

少なくとも一方が `vcpkg.exe` を含むディレクトリを指すはずです。どちらも設定されていなければ `setup.ps1` を実行してください。VS 付属の vcpkg を自動的に探します。

### VS に `x64-Debug` 構成が表示される、または ncurses エラーが出る

VS が `CMakeSettings.json` を使っていません。**Tools → Options → CMake → General** でプリセット統合を **"Use CMakeSettings.json (Legacy)"** または **"Never use CMake Presets"** に設定し、以下の完全リセットを行います。

### 構成は成功するがヘッダー/ライブラリが見つからずビルドに失敗する

VS 環境（`INCLUDE`、`LIB`、`PATH`）が正しく取得されていない可能性があります。次を試してください:

1. プロジェクトルートの `CMakeUserPresets.json` を削除します。
2. 該当する `out\build\` サブディレクトリを完全に削除します。
3. `setup.ps1` を再実行して両方を再生成します。

### ゲームがすぐクラッシュする、またはデータが見つからない

プロジェクトルートの `launch.vs.json` は F5 起動の作業ディレクトリをプロジェクトルートに設定します。ファイルがないか VS が読み込んでいない場合、`./data/` が見つからないことがあります。

エクスプローラーやターミナルから `.exe` を直接起動する場合は、プロジェクトルートから実行してください:

```powershell
# 正しい — プロジェクトルートから実行
.\out\build\win-rel-deb\src\cataclysm-bn-tiles.exe

# 間違い — ./data/ が見つからない
cd out\build\win-rel-deb\src
.\cataclysm-bn-tiles.exe
```

### 構成中に `VsDevCmd.bat not found` エラーが出る

`VsDevCmd.bat` は VS のインストール先にあります。このエラーが出る場合、VS が標準でない場所にインストールされている可能性があります。cmake を実行する前に `DevEnvDir` 環境変数を VS の `Common7\IDE` ディレクトリに設定します:

```powershell
$env:DevEnvDir = "D:\VisualStudio\Common7\IDE\"
cmake --preset windows-tiles-sounds-x64-msvc
```

### ビルドが非常に遅い

ccache がインストールされ `PATH` にある場合、自動的に検出されて使用されます。[ccache.dev](https://ccache.dev/) からインストールすると、`git clean` やブランチ切り替え後のインクリメンタルビルドが大幅に速くなります。

### ビルド環境を完全にリセットする方法

問題が発生してクリーンな状態にしたい場合:

```powershell
# VS のキャッシュされたプロジェクト状態（古い構成、IntelliSense DB）を削除
Remove-Item -Recurse -Force .vs

# すべてのビルド出力ディレクトリを削除
Remove-Item -Recurse -Force out\build

# 生成されたユーザープリセットを削除（次の構成で再生成される）
Remove-Item -Force CMakeUserPresets.json

# ターミナルビルド用に setup を再実行（次回 VS を開いたときにも再構成される）
.\setup.ps1
```

### 再構成時に CMakeUserPresets.json が "already exists" と表示される

これは意図された動作です。カスタマイズを上書きしないよう、ファイルは最初の構成時だけ生成されます。デフォルトの生成内容に戻すには削除して `setup.ps1` を実行してください。
