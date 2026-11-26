# 車両タイル表示

## 概要

この文書では、車両の操作画面（`veh_interact`）にタイルベースの描画を追加し、車両の製作と変更中の視認性を向上させる実装計画を説明します。

## 目標

車両製作画面に ASCII 表示の代替としてグラフィカルなタイル描画を追加します。特に ASCII 表現に慣れていないプレイヤーにとって使いやすくなります。

## 現在のアーキテクチャ

### 車両操作画面（`src/vehicle/veh_interact.cpp`）

現在の `display_veh()` 関数（2284行目）は ASCII 描画を使用します:

```cpp
void veh_interact::display_veh()
{
    werase( w_disp );
    const point h_size = point( getmaxx( w_disp ), getmaxy( w_disp ) ) / 2;

    // 構造部品を反復
    std::vector<int> structural_parts = veh->all_standalone_parts();
    for( auto &structural_part : structural_parts ) {
        const int p = structural_part;
        int sym = veh->part_sym( p );      // ASCII シンボル
        nc_color col = veh->part_color( p ); // ncurses の色

        const point q = ( veh->part( p ).mount + dd ).rotate( 3 );
        mvwputch( w_disp, h_size + q, col, special_symbol( sym ) );
    }
}
```

`veh_interact` の主なメンバー:

- `catacurses::window w_disp` - 車両表示ウィンドウ
- `point dd` - 現在のカーソルオフセット（カーソル位置の負の値）
- `int cpart` - 現在選択されている部品のインデックス
- `vehicle *veh` - 変更中の車両

### タイル描画システム

**中心となるクラス**: `cata_tiles`（`src/cata_tiles.cpp`）

**車両部品の描画**（`draw_vpart()`、3612行目）:

```cpp
bool cata_tiles::draw_vpart( const tripoint &p, lit_level ll, int &height_3d,
                             const bool ( &invisible )[5], int z_drop )
{
    const vpart_id &vp_id = veh.part_id_string( veh_part, z_drop > 0, part_mod );
    const int subtile = part_mod == 1 ? open_ : part_mod == 2 ? broken : 0;
    const int rotation = std::round( to_degrees( veh.face.dir() ) );
    const std::string vpname = "vp_" + vp_id.str();

    const tile_search_params tile = {vpname, C_VEHICLE_PART, empty_string, subtile, rotation};
    return draw_from_id_string( tile, p, bgCol, fgCol, ll, true, z_drop, false, height_3d );
}
```

主なパラメーター:

- **タイル ID**: `"vp_" + vpart_id.str()`（例: `"vp_frame"`、`"vp_engine_v8"`）
- **カテゴリー**: `C_VEHICLE_PART`
- **サブタイル**: 0（通常）、`open_`（開いたドア）、`broken`（損傷）
- **回転**: `veh.face.dir()` の角度

### テンプレート: キャラクタープレビュー（`src/character_preview.cpp`）

このファイルは、メインマップ外の UI 画面でタイルを描画する方法を示します:

```cpp
class char_preview_adapter : public cata_tiles
{
public:
    static char_preview_adapter *convert( cata_tiles *ct ) {
        return static_cast<char_preview_adapter *>( ct );
    }

    void display_avatar_preview_with_overlays( const avatar &ch, const point &p, bool with_clothing ) {
        const tile_search_params tile { ent_name, C_NONE, "", corner, rotation };
        draw_from_id_string(
            tile, tripoint( p, 0 ), std::nullopt, std::nullopt,
            lit_level::BRIGHT, false, 0, true, height_3d );
        //                              ^ as_independent_entity = true
    }
};
```

主なパターン:

1. **アダプタークラス**: protected な `draw_from_id_string` にアクセスするための静的キャスト
2. **ズーム制御**: `tilecontext->set_draw_scale( zoom )`
3. **ピクセル位置指定**: `termx_to_pixel_value()` で端末単位をピクセルに変換
4. **独立した描画**: `as_independent_entity = true` によりマップ境界外で描画可能

---

## 実装計画

### 1.1 車両プレビューアダプタークラスの作成

**ファイル**: `src/vehicle/vehicle_preview.h` / `src/vehicle/vehicle_preview.cpp`

```cpp
#if defined(TILES)

class veh_preview_adapter : public cata_tiles
{
public:
    static veh_preview_adapter *convert( cata_tiles *ct ) {
        return static_cast<veh_preview_adapter *>( ct );
    }

    // ピクセル位置に単一の車両部品を描画
    void draw_vpart_at_pixel( const vpart_id &id, const point &pixel_pos,
                               int part_mod, units::angle rotation, bool highlight );

    // ウィンドウの中央に車両全体を描画
    void draw_vehicle_preview( const vehicle &veh, const catacurses::window &win,
                                point cursor_offset, int highlight_part );
};

struct vehicle_preview_window {
    catacurses::window w_preview;

    void prepare( int nlines, int ncols );
    void display( const vehicle &veh, point cursor_offset, int highlight_part ) const;
    void zoom_in();
    void zoom_out();
    void clear() const;

private:
    int zoom = 64;  // デフォルトのズームレベル
    static constexpr int MIN_ZOOM = 16;
    static constexpr int MAX_ZOOM = 128;

    point calc_center_pixel() const;
};

#endif // TILES
```

### 1.2 グラフィックスオプションの追加

**ファイル**: `src/options.cpp`

`add_options_graphics()` に新しいオプションを追加します:

```cpp
#if defined(TILES)
    add( "VEHICLE_EDIT_TILES", graphics, translate_marker( "Graphical vehicle display" ),
         translate_marker( "If true, the vehicle interaction screen will display vehicle parts using graphical tiles instead of ASCII symbols." ),
         true, COPT_CURSES_HIDE );
#endif
```

このオプション:

- TILES 対応時だけ表示される
- curses 専用ビルドでは非表示（`COPT_CURSES_HIDE`）
- デフォルト値は `true`（利用可能ならタイルを使用）

### 1.3 `veh_interact` クラスの変更

**ファイル**: `src/vehicle/veh_interact.h`

メンバーを追加します:

```cpp
#if defined(TILES)
    std::unique_ptr<vehicle_preview_window> tile_preview;
#endif
```

メソッドを追加します:

```cpp
#if defined(TILES)
    void display_veh_tiles();  // 新しいタイルベースの描画
#endif
```

### 1.4 `display_veh_tiles()` の実装

**ファイル**: `src/vehicle/veh_interact.cpp`

```cpp
#if defined(TILES)
void veh_interact::display_veh_tiles()
{
    if( !tile_preview ) {
        tile_preview = std::make_unique<vehicle_preview_window>();
        tile_preview->prepare( getmaxy( w_disp ), getmaxx( w_disp ) );
    }

    tile_preview->display( *veh, dd, cpart );
}
#endif

void veh_interact::display_veh()
{
#if defined(TILES)
    if( is_draw_tiles_mode() && get_option<bool>( "VEHICLE_EDIT_TILES" ) ) {
        display_veh_tiles();
        return;
    }
#endif
    // ... 既存の ASCII 実装 ...
}
```

### 1.5 描画ロジック

`vehicle_preview_window::display()`:

```cpp
void vehicle_preview_window::display( const vehicle &veh, point cursor_offset,
                                       int highlight_part ) const
{
    werase( w_preview );

    // ウィンドウサイズをピクセル単位で取得
    const int win_w_px = getmaxx( w_preview ) * termx_to_pixel_value();
    const int win_h_px = getmaxy( w_preview ) * termy_to_pixel_value();
    const point center_px = { win_w_px / 2, win_h_px / 2 };

    // 現在のズームでのタイルサイズを取得
    const int tile_w = tilecontext->get_tile_width();
    const int tile_h = tilecontext->get_tile_height();

    auto *adapter = veh_preview_adapter::convert( &*tilecontext );

    // すべての車両部品を描画
    for( int p : veh.all_standalone_parts() ) {
        const vehicle_part &part = veh.part( p );
        const point mount = part.mount;

        // カーソルを基準にした相対位置を計算
        // ASCII モードでは表示方向のために rotate(3) を使用する
        const point rel = ( mount + cursor_offset ).rotate( 3 );

        // ピクセル位置へ変換（ウィンドウ中央）
        const point pixel_pos = center_px + point( rel.x * tile_w, rel.y * tile_h );

        // 部品の描画情報を取得
        char part_mod = 0;
        const vpart_id &vp_id = veh.part_id_string( p, false, part_mod );
        const units::angle rotation = veh.face.dir();
        const bool is_highlighted = ( p == highlight_part );

        adapter->draw_vpart_at_pixel( vp_id, pixel_pos, part_mod, rotation, is_highlighted );
    }

    // 中央に十字カーソルを描画（現在のカーソル位置）
    draw_cursor_crosshair( center_px );

    wnoutrefresh( w_preview );
}
```

---

## ファイル変更の概要

### 新規ファイル

- `src/vehicle/vehicle_preview.h` - タイルプレビューアダプタークラス
- `src/vehicle/vehicle_preview.cpp` - タイルプレビューの実装

### 変更ファイル

- `src/vehicle/veh_interact.h` - タイルプレビューメンバーを追加
- `src/vehicle/veh_interact.cpp` - タイル表示を統合
- `src/options.cpp` - `VEHICLE_EDIT_TILES` オプションを追加
- `CMakeLists.txt` - 新しいソースファイルを追加

### オプション

- `VEHICLE_EDIT_TILES`（グラフィックス） - グラフィカルな車両表示を切り替え（デフォルト: true）

---

## 技術的な考慮事項

### 条件付きコンパイル

タイル関連コードはすべて `#if defined(TILES)` で囲む必要があります:

```cpp
#if defined(TILES)
#include "cata_tiles.h"
#include "sdltiles.h"
// ... タイルコード ...
#endif
```

### 座標システム

車両画面では複数の座標システムを使用します:

1. **マウント座標**: 車両ローカル（原点は車両中心）
2. **画面座標**: 端末単位（列/行）
3. **ピクセル座標**: SDL ピクセル（タイル描画用）

変換:

```cpp
// マウント -> 画面（カーソルオフセットと回転を含む）
point screen = ( mount + cursor_offset ).rotate( 3 ) + window_center;

// 画面 -> ピクセル
point pixel = screen * point( termx_to_pixel_value(), termy_to_pixel_value() );
```

### パフォーマンス

- 可能な場合はタイル検索をキャッシュする
- 変更時だけ再描画する（dirty フラグを使用）
- 過剰なテクスチャスケーリングを防ぐためズームレベルを制限する

### フォールバック動作

- タイルの読み込みに失敗した場合は自動的に ASCII にフォールバックする
- curses 専用ビルドもコンパイルして動作しなければならない

---

## テストチェックリスト

- [ ] すべての車両部品でタイルが正しく描画される
- [ ] ズームイン/アウトが滑らかに動作する
- [ ] カーソル位置が明確に表示される
- [ ] 部品のハイライトが動作する（選択部品が目立つ）
- [ ] `VEHICLE_EDIT_TILES` オプションがグラフィックス設定に表示される（タイルビルドのみ）
- [ ] オプションを無効にすると ASCII 表示にフォールバックする
- [ ] curses 専用ビルドもコンパイルされる（TILES 未定義）
- [ ] 大型車両（50以上の部品）で許容できる性能を保つ
- [ ] 開いたドアが正しい「開」タイルバリエーションを表示する
- [ ] 損傷した部品が正しい「損傷」タイルバリエーションを表示する
- [ ] 車両の回転が正しく表示される
