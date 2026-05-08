# ユーザーインターフェース

Cataclysm: Bright Nights は、ユーザーインターフェースに ncurses、あるいはタイルビルドの場合は ncursesポートを使用しています。 ウィンドウ管理は `ui_adaptor`によって実現されます。この`ui_adaptor`は、各UIに対して、サイズ変更と再描画を処理するためのサイズ変更コールバックと再描画コールバックを必要とします。
`ui_adaptor` の使用方法に関する詳細は、 `ui_manager.h`内で確認できます。

`ui_adaptor` の使用例として適切なものは、以下のファイル内で見つけることができます。

- `popup.h/cpp` 内の`query_popup` および `static_popup`
- `messages.cpp` 内の `Messages::dialog`
