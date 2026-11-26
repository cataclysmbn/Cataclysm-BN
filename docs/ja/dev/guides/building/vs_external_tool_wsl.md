# Visual Studio 外部ツール自動化 (Windows + WSL)

このページでは、`cmd`/PowerShell インターフェースから Cataclysm: BN の CMake ビルド自動化を実行する Visual Studio 外部ツールのワークフローを説明します。

## 概要

- ワークフローは Visual Studio の **External Tools** メニューから起動します。
- 同じ流れで Windows (MSVC) ビルドと WSL ベースの Linux ビルドを実行できます。
- すべての選択はメニューから行うため、手動のシェルコマンドは不要です。
- 完全なデバッガー対応: Windows では VS デバッガーを自動接続し、Linux では VS の SSH 接続を使います。

## Visual Studio に外部ツールを追加する

**Tools -> External Tools...** を開き、項目を追加します。インターフェースモードに関係なく **Arguments** 欄は同じです:

| フィールド        | 値                                   |
| ----------------- | ------------------------------------ |
| Title             | BN Build                             |
| Command           | `cmd.exe`                            |
| Arguments         | `/c "$(SolutionDir)cmake-build.bat"` |
| Initial directory | `$(SolutionDir)`                     |

**Use Output Window** チェックボックスでインターフェースモードを選びます:

| Use Output Window | インターフェース                                               |
| ----------------- | -------------------------------------------------------------- |
| **オフ**          | 別の `cmd` ウィンドウを開き、番号付きのテキストメニューを表示  |
| **オン**          | VS の Output ウィンドウで実行し、WinForms GUI リスト選択を表示 |

GUI 選択（オン）はすべてを VS 内で完結できますが、複数の浮動ウィンドウをクリックする必要があります。テキストメニュー（オフ）は別のコンソールで番号を入力します。どちらも同じ手順を実行し、違いは好みだけです。

![Visual Studio External Tools メニュー](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![外部ツール設定例](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

専用のデバッグショートカット（例: "BN Debug"）を作るには、同じ設定で2つ目の項目を追加し、Arguments に `-Action debug` を付けます:

```
/c "$(SolutionDir)cmake-build.bat" -Platform win -Preset 1 -BuildType 2 -Action debug
```

## インターフェースモード

**テキストメニュー**（"Use Output Window" オフ）: 番号付きのプロンプトが `cmd` ウィンドウ内に表示されます。選択する番号を入力してください。

![プロンプト駆動の cmd インターフェース](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

**WinForms GUI 選択**（"Use Output Window" オン）: 選択ごとに浮動リストボックスが表示されます。項目をクリックして OK を押すか、ダブルクリックして確定します。

> **Linux/WSL の注:** GUI 選択は Linux ビルドの最初のプラットフォーム選択にだけ適用されます。その後、WSL 操作に必要な管理者コンソールでスクリプトが再起動し、そのウィンドウの以降のメニューはテキストプロンプトを使います。

## ワークフロー

実行するたび、スクリプトは次を尋ねます:

1. **プラットフォーム** — Windows (MSVC) または Linux (WSL)
2. **構成プリセット** — `CMakePresets.json`（存在すれば `CMakeUserPresets.json` も）から読み込む
3. **ビルドタイプ** — Debug / RelWithDebInfo / Release（Windows のみ。Linux はプリセットで設定）
4. **ターゲット** — プリセットの `TILES`/`TESTS` キャッシュ変数から推測、または任意の名前を入力
5. **アクション** — Build、Run、Rebuild、Delete、Debug

ビルドが成功するとメインメニューに戻らず、そのまま Run または Debug を選べます。

各セッションの最後には、毎回回答し直さず同じ構成を実行する "Repeat last" オプションが表示されます。

![ビルド完了出力](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

![ビルド後に実行中の Cataclysm: BN](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)

## 権限昇格

**Windows ビルド**は標準（昇格していない）整合性で実行します。デバッガーの自動接続に必要なので、`cmake-build.bat` や Visual Studio を管理者として実行しないでください。

**Linux/WSL ビルド**は WSL のファイルシステムとネットワーク操作に管理者権限が必要です。昇格されていないことを検出すると、スクリプトは選択したオプションをすべて渡して、昇格した PowerShell ウィンドウを自動的に起動します。

## デバッグ

### Windows

**Debug** アクションを選ぶと:

1. `Start-Process` で標準整合性のゲーム実行ファイルを起動します。
2. COM 自動化（Running Object Table の DTE オブジェクト）を通して実行中の Visual Studio に接続します。
3. ゲームプロセスで `Debugger.Attach()` を呼び、VS のネイティブデバッガーを自動接続します。

スクリプトは VS がプロセスを登録するまで最大5秒間ポーリングし、成功を報告するか代替手順を表示します。

**要件:**

- Visual Studio は標準（非昇格）整合性で実行する必要があります。VS を管理者として起動すると COM ROT が整合性レベルごとに分離され、自動接続が失敗して "no running VS instance found" と表示されます。昇格せずに VS を再起動してください。
- `cmake-build.bat` も昇格せずに実行する必要があります（ランチャーは Windows ビルドでは昇格しません）。

**自動接続に失敗した場合**、スクリプトはプロセス PID を表示します。**Debug → Attach to Process**（`Ctrl+Alt+P`）から名前または PID で手動接続してください。

### Linux（SSH 接続）

Linux WSL ビルドのデバッグでは Visual Studio の SSH リモート接続機能を使います。スクリプトはデバッグ実行のたびに次を自動設定します:

1. WSL にまだなければ `openssh-server` をインストール
2. `/etc/ssh/sshd_config` で `PasswordAuthentication yes` を有効化
3. 不足しているホストキーを作るため `ssh-keygen -A` を実行
4. SSH サービスを開始または再起動
5. VS の GDB がデバッガーの直接の子でないプロセスにも接続できるよう `/proc/sys/kernel/yama/ptrace_scope` を `0` に設定
6. 現在の WSL IP アドレスを解決（WSL2 は起動ごとに新しい IP を割り当てます）
7. 古い `netsh` ポートプロキシを削除し、新しいものを作成: `Windows localhost:2222 → WSL <ip>:22`
8. `localhost:2222` に接続できることを確認
9. WSL 内の `/tmp/` に小さな起動スクリプトを書き、新しい WSL ウィンドウで開く（ゲームが実 TTY と WSLg の `DISPLAY`/`WAYLAND_DISPLAY` 環境を使えるようにする）
10. 手順付きの接続説明を表示

**ゲームウィンドウが開いた後に Visual Studio で接続するには:**

1. **Debug → Attach to Process**（`Ctrl+Alt+P`）を開きます。
2. **Connection type** を `SSH` に設定します。
3. **Connection target** を `localhost:2222` に設定します。
4. Enter または接続ボタンを押し、WSL のユーザー名とパスワードを入力します。
5. プロセス一覧からゲームプロセス（例: `cataclysm-bn-tiles`）を探します。
6. **Attach** をクリックします。

Visual Studio は初回使用後に SSH 接続を保存します。次回からは "Attach to Process" を開き、保存された `localhost:2222` 接続を選んでアタッチするだけです。

> ポートプロキシは `127.0.0.1`（ループバックのみ）を使うため、Windows ファイアウォールの受信規則は不要です。

> `ptrace_scope=0` は一時的な設定で、WSL の再起動時にリセットされます。TSan の `vm.mmap_rnd_bits` 修正とは異なり、永続的な sysctl ファイルには書き込まれません。

## 非対話型ショートカット

`cmake-build.bat` にパラメーターを直接渡すと、すべてのプロンプトを省略できます。特定のアクションへ直接進む外部ツール項目を追加する際に便利です。

```bat
rem Windows MSVC ビルド（プリセット1、RelWithDebInfo）
cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build

rem Windows デバッグ（ゲーム起動と VS デバッガーの自動接続）
cmake-build.bat -Platform win -Preset 1 -BuildType 1 -Action debug

rem Linux WSL ビルド（名前でプリセットを指定）
cmake-build.bat -Platform linux -Preset linux-slim -Target cataclysm-bn-tiles -Action build

rem テストフィルター付き Linux 実行
cmake-build.bat -Platform linux -Preset 2 -Action run -RunArgs "[map]"
```

**パラメーター参照:**

| パラメーター  | 値                                       | 注記                                          |
| ------------- | ---------------------------------------- | --------------------------------------------- |
| `-Platform`   | `win` / `linux`                          |                                               |
| `-Preset`     | プリセット名または1始まりのインデックス  | インデックスは CMakePresets.json の順序に対応 |
| `-BuildType`  | `1`=Debug `2`=RelWithDebInfo `3`=Release | Windows のみ                                  |
| `-Target`     | cmake ターゲット名                       | 省略時はプリセットから推測                    |
| `-Action`     | `build` `run` `rebuild` `delete` `debug` |                                               |
| `-RunArgs`    | バイナリに転送する文字列                 | 例: テストフィルターの `[map]`                |
| `-ExtraFlags` | 追加の cmake 構成フラグ                  | 例: `-DFOO=ON`                                |
