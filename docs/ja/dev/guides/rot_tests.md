# 腐敗の回帰テストとカバレッジ

リポジトリのルートで腐敗テストを実行します。

```sh
cmake --preset linux-full
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278
```

テストは `data/mods/TEST_DATA/rot.json` の固定フィクスチャと、範囲を制限した Catch2 ジェネレーターを使用します。保管温度、時間と腐敗の境界、入れ子の容器、死体の回収、スポーン、コールバックの所有権を対象にします。調理の回帰テストでは、21日後に実際の油調理器と RV キッチンのアクティビティも実行します。手作りのコーンミールとラードも含み、材料と燃料が実際に消費されることを検証します。

## 腐敗の境界

- `src/rot/rot_calculation.h` は、検証済みの正の賞味期限と純粋な算術を定義します。腐らないアイテムには賞味期限のプロファイルがなく、相対腐敗度は常に 0 です。有限でない値と範囲外の相対腐敗度の編集は拒否されます。
- `src/rot/item_rot.cpp` は、この算術をアイテムの状態、天候、既存の処理周期に結び付けます。燻製と製粉の間は、固有の賞味期限を 0 に置き換えず、腐敗の進行だけを止めます。通常のライブアイテムの getter は、既存の遅延実体化の動作を維持します。
- 完成した構成要素の所有権により、不活性な材料レコードを識別します。`COMPONENT` フラグがない古いセーブも同様です。作成途中の材料はライブのままです。レコードを取り除くかコピーすると、記録された期間を腐敗させずにライブのクロックを再開します。中に入ったアイテムは一緒に再開し、別に入れ子になったレコードは不活性のままです。
- 個数スタックのコールバックのコピーは、コールバックが終わるまで元のレコード/ライブの役割を借ります。実際の配置が優先され、一時的な役割はコピーも保存もされません。
- 既存のセーブフィールド `rot`、`last_rot_check`、`components` は維持します。単独の古い `COMPONENT` フラグは getter だけで効く従来の動作を維持し、明示的な処理を無効にしません。

元の issue の回帰タグは `[issue_10181]` です。`[lifecycle]` は、レコードの読み取り、回収、コピー、処理の停止、調理のワークフローを対象にします。`[calculation]` は、純粋な算術と直接のエントリポイントの境界を対象にします。コールバックのテストは、前のテストが余った容量を残していても、ベクターの再確保を強制します。

## カバレッジ 100% の要求

[カバレッジチェッカー](../../../../tools/check_rot_coverage.ts)は、`src/rot.cpp`、`src/rot/item_rot.cpp`、`src/rot/rot_calculation.cpp` のすべての関数と入れ子のコールバックを自動的に含めます。明示的なマニフェストは、より大きなソースファイルに残っている位置/実体化、作成時の鮮度、表示、死体/スポーン、走査のアダプターも対象にします。新しい腐敗の実装は、`item.h` に測定されないインライン本体として追加せず、ファイル全体が測定範囲に入る場所に置いてください。この値は腐敗サブシステムの分母であり、ゲーム全体のカバレッジではありません。

測定のたびに新しいプロファイルディレクトリを使用してください。古いプロファイルや、失敗したテストのプロファイルはマージしないでください。

```sh
cmake --preset linux-full \
  -DCMAKE_PROJECT_INCLUDE="$PWD/build-scripts/rot-coverage.cmake"
mkdir -p out/rot-coverage/build
LLVM_PROFILE_FILE="$PWD/out/rot-coverage/build/%p.profraw" \
  cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
profiles=$(mktemp -d "$PWD/out/rot-coverage/run-XXXXXX")
LLVM_PROFILE_FILE="$profiles/%p.profraw" \
  SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278 && \
  llvm-profdata merge -sparse "$profiles/"*.profraw -o "$profiles/rot.profdata" && \
  tools/check_rot_coverage.ts out/build/linux-full/tests/cata_test-tiles "$profiles/rot.profdata"
```

チェッカーは LLVM のネイティブな関数サマリーを使用し、丸めた `100.00%` ではなく、未到達のリージョン、行、分岐の結果が 0 であることを要求します。関数やサマリーがない場合も失敗します。`llvm-cov`、`llvm-profdata`、`llvm-cxxfilt` のバージョンは Clang に合わせてください。チェッカーには Deno が必要で、リポジトリのルートで実行します。

`-DROT_SANITIZE=ON` を指定すると、[rot-coverage.cmake](../../../../build-scripts/rot-coverage.cmake) に列挙されたソースファイルに AddressSanitizer も適用されます。プログラム全体のサニタイザービルドではありません。カバレッジのプロファイルはサニタイザーの診断とは独立しているため、カバレッジが通ってもサニタイザーの失敗は許容されません。

次のコマンドで通常のビルドに戻します。

```sh
cmake --preset linux-full -DCMAKE_PROJECT_INCLUDE= -DROT_SANITIZE=OFF
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
```

カバレッジのマニフェストやチェッカーを拡張するときは、チェッカーのテストも実行してください。

```sh
deno test tools/check_rot_coverage_test.ts
deno check tools/check_rot_coverage.ts
deno lint tools/check_rot_coverage.ts tools/check_rot_coverage_test.ts
```

テスト実行ファイルは通常、ゲームメッセージを破棄します。画面に表示されるテキストを検証する必要があるテストは、`tests/message_helpers.h` の `capture_messages_during` を使用できます。キャプチャはスレッドローカルで、スコープを持ち、入れ子にできます。
