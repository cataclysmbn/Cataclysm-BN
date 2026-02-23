# Visual Studio 外部ツール自動化 (Windows + WSL)

このページでは、Visual Studio の外部ツールワークフローを使って、Cataclysm: BN の CMake
ビルド自動化をプロンプト駆動の `cmd` インターフェースから実行する方法を説明します。

## 概要

- このワークフローは Visual Studio の **External Tools** メニューから起動します。
- ツールは `cmd` ウィンドウを開き、プロンプトで操作を案内するため、手動でシェルコマンドを入力する必要はありません。
- 同じフローで Windows ビルドと WSL ベースの Linux ビルドの両方を実行できます。

## Visual Studio に外部ツールを追加する

**Tools -> External Tools...** を開き、ビルドツールの項目を設定します。

![Visual Studio External Tools menu](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![External tool configuration example](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

## 実行インターフェース

起動すると、自動化ツールはメニュー形式のプロンプトフローを持つ `cmd` セッションを開きます。

![Prompt-driven cmd interface](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

`cmd` 上で動作しますが、ユーザーは選択肢を選んで質問に答えるだけでよく、手動でコマンドを作成する必要はありません。

短いデモ動画:

<video controls src="https://github.com/user-attachments/assets/54968297-3b4c-462f-9bae-bd5c1e190b75"></video>

## ビルド完了

ワークフローは同じコマンドウィンドウ内に完了状態を表示します。

![Build completion output](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

完了後は、ビルド済みのゲームを通常どおり起動できます。

![Cataclysm: BN running after build](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)
