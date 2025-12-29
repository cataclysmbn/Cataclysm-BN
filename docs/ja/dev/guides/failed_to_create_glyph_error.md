# グリフ作成の失敗エラー

類似の問題が、Cataclysm-BN (BN) および Cataclysm-DDA (DDA) の MSYS Windows ビルドと一部の Mac ビルドで報告されています。
https://github.com/CleverRaven/Cataclysm-DDA/issues/50115

これは、SDL2 および Freetype の新しいバージョンが、我々のデフォルトフォントと互換性がないことが原因であると考えられます。vcpkg が両者をそれぞれ20日前と9日前に更新したこと
(https://github.com/microsoft/vcpkg/pull/19509, https://github.com/microsoft/vcpkg/pull/19284)から、これが問題の根本にあると見ています。私は1ヶ月前の vcpkg を使用しており、グリフの問題は発生していません。

残念ながら、Microsoft の公式 C++ ライブラリマネージャーである vcpkg は、特定のバージョンのライブラリをインストールすることを許可していません。したがって、以下の2つの選択肢があります。

1. 更新前の vcpkg の古いバージョンを取得する。 私は以下のバージョンからインストールし、動作することを確認しています。
   https://github.com/microsoft/vcpkg/tree/6bc4362fb49e53f1fff7f51e4e27e1946755ecc6

2. config/fonts.json を開き、Terminus.ttf のエントリを削除する。我々がデフォルトで unifont を使用していない理由は覚えていませんが、国際化/フォントコード（i18n/fonts code）に携わる予定がない限り、視覚的な影響以外は問題ありません。
