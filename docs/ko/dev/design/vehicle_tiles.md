# 차량 타일 디스플레이

## 개요

이 문서는 차량 상호작용 화면(`veh_interact`)에 타일 기반 렌더링을 추가하여 차량 제작 및 수정 중 시각화를 쉽게 만드는 구현 계획을 설명합니다.

## 목표

차량 제작 화면에 ASCII 디스플레이의 대안으로 그래픽 타일 렌더링을 추가하여 특히 ASCII 표현에 익숙하지 않은 플레이어의 사용성을 개선합니다.

## 현재 아키텍처

### 차량 상호작용 화면 (`src/veh_interact.cpp`)

현재 `display_veh()` 함수(2284번째 줄)는 ASCII 렌더링을 사용합니다:

```cpp
void veh_interact::display_veh()
{
    werase( w_disp );
    const point h_size = point( getmaxx( w_disp ), getmaxy( w_disp ) ) / 2;

    // 구조 파트 순회
    std::vector<int> structural_parts = veh->all_standalone_parts();
    for( auto &structural_part : structural_parts ) {
        const int p = structural_part;
        int sym = veh->part_sym( p );      // ASCII 심볼
        nc_color col = veh->part_color( p ); // ncurses 색상

        const point q = ( veh->part( p ).mount + dd ).rotate( 3 );
        mvwputch( w_disp, h_size + q, col, special_symbol( sym ) );
    }
}
```

`veh_interact`의 주요 멤버:

- `catacurses::window w_disp` - 차량 디스플레이 창
- `point dd` - 현재 커서 오프셋 (커서 위치의 음수)
- `int cpart` - 현재 선택된 파트 인덱스
- `vehicle *veh` - 수정 중인 차량

### 타일 렌더링 시스템

**핵심 클래스**: `cata_tiles` (`src/cata_tiles.cpp`)

**차량 파트 렌더링** (`draw_vpart()`, 3612번째 줄):

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

주요 매개변수:

- **타일 ID**: `"vp_" + vpart_id.str()` (예: `"vp_frame"`, `"vp_engine_v8"`)
- **카테고리**: `C_VEHICLE_PART`
- **서브타일**: 0 (정상), `open_` (열린 문), `broken` (손상됨)
- **회전**: `veh.face.dir()`의 각도

### 템플릿: 캐릭터 미리보기 (`src/character_preview.cpp`)

이 파일은 메인 맵 외부의 UI 화면에서 타일을 렌더링하는 방법을 보여줍니다:

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

주요 패턴:

1. **어댑터 클래스**: protected `draw_from_id_string`에 접근하기 위한 정적 캐스트
2. **줌 컨트롤**: `tilecontext->set_draw_scale( zoom )`
3. **픽셀 위치 지정**: `termx_to_pixel_value()`를 통해 터미널 단위를 픽셀로 변환
4. **독립적 렌더링**: `as_independent_entity = true`는 맵 경계 외부에서 렌더링 허용

---

## 구현 계획

### 1.1 차량 미리보기 어댑터 클래스 생성

**파일**: `src/vehicle_preview.h` / `src/vehicle_preview.cpp`

```cpp
#if defined(TILES)

class veh_preview_adapter : public cata_tiles
{
public:
    static veh_preview_adapter *convert( cata_tiles *ct ) {
        return static_cast<veh_preview_adapter *>( ct );
    }

    // 픽셀 위치에 단일 차량 파트 그리기
    void draw_vpart_at_pixel( const vpart_id &id, const point &pixel_pos,
                               int part_mod, units::angle rotation, bool highlight );

    // 창 중앙에 전체 차량 그리기
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
    int zoom = 64;  // 기본 줌 레벨
    static constexpr int MIN_ZOOM = 16;
    static constexpr int MAX_ZOOM = 128;

    point calc_center_pixel() const;
};

#endif // TILES
```

### 1.2 그래픽 옵션 추가

**파일**: `src/options.cpp`

`add_options_graphics()`에 새 옵션 추가:

```cpp
#if defined(TILES)
    add( "VEHICLE_EDIT_TILES", graphics, translate_marker( "Graphical vehicle display" ),
         translate_marker( "If true, the vehicle interaction screen will display vehicle parts using graphical tiles instead of ASCII symbols." ),
         true, COPT_CURSES_HIDE );
#endif
```

이 옵션:

- TILES 지원이 있을 때만 표시됨
- curses 전용 빌드에서는 숨겨짐 (`COPT_CURSES_HIDE`)
- 기본값은 `true` (사용 가능할 때 타일 사용)

### 1.3 `veh_interact` 클래스 수정

**파일**: `src/veh_interact.h`

멤버 추가:

```cpp
#if defined(TILES)
    std::unique_ptr<vehicle_preview_window> tile_preview;
#endif
```

메서드 추가:

```cpp
#if defined(TILES)
    void display_veh_tiles();  // 새로운 타일 기반 렌더링
#endif
```

### 1.4 `display_veh_tiles()` 구현

**파일**: `src/veh_interact.cpp`

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
    // ... 기존 ASCII 구현 ...
}
```

### 1.5 렌더링 로직

`vehicle_preview_window::display()`:

```cpp
void vehicle_preview_window::display( const vehicle &veh, point cursor_offset,
                                       int highlight_part ) const
{
    werase( w_preview );

    // 창 크기를 픽셀 단위로 가져오기
    const int win_w_px = getmaxx( w_preview ) * termx_to_pixel_value();
    const int win_h_px = getmaxy( w_preview ) * termy_to_pixel_value();
    const point center_px = { win_w_px / 2, win_h_px / 2 };

    // 현재 줌에서의 타일 크기 가져오기
    const int tile_w = tilecontext->get_tile_width();
    const int tile_h = tilecontext->get_tile_height();

    auto *adapter = veh_preview_adapter::convert( &*tilecontext );

    // 모든 차량 파트 그리기
    for( int p : veh.all_standalone_parts() ) {
        const vehicle_part &part = veh.part( p );
        const point mount = part.mount;

        // 커서 기준 상대 위치 계산
        // 참고: ASCII 모드에서 디스플레이 방향을 위해 rotate(3)이 사용됨
        const point rel = ( mount + cursor_offset ).rotate( 3 );

        // 픽셀 위치로 변환 (창 중앙)
        const point pixel_pos = center_px + point( rel.x * tile_w, rel.y * tile_h );

        // 파트 렌더링 정보 가져오기
        char part_mod = 0;
        const vpart_id &vp_id = veh.part_id_string( p, false, part_mod );
        const units::angle rotation = veh.face.dir();
        const bool is_highlighted = ( p == highlight_part );

        adapter->draw_vpart_at_pixel( vp_id, pixel_pos, part_mod, rotation, is_highlighted );
    }

    // 중앙에 십자선 그리기 (현재 커서 위치)
    draw_cursor_crosshair( center_px );

    wnoutrefresh( w_preview );
}
```

---

## 파일 변경 요약

### 새 파일

- `src/vehicle_preview.h` - 타일 미리보기 어댑터 클래스
- `src/vehicle_preview.cpp` - 타일 미리보기 구현

### 수정된 파일

- `src/veh_interact.h` - 타일 미리보기 멤버 추가
- `src/veh_interact.cpp` - 타일 디스플레이 통합
- `src/options.cpp` - `VEHICLE_EDIT_TILES` 옵션 추가
- `src/Makefile` / `CMakeLists.txt` - 새 소스 파일 추가

### 옵션

- `VEHICLE_EDIT_TILES` (그래픽) - 그래픽 차량 디스플레이 전환 (기본값: true)

---

## 기술적 고려사항

### 조건부 컴파일

모든 타일 관련 코드는 `#if defined(TILES)`로 감싸야 합니다:

```cpp
#if defined(TILES)
#include "cata_tiles.h"
#include "sdltiles.h"
// ... 타일 코드 ...
#endif
```

### 좌표 시스템

차량 화면은 여러 좌표 시스템을 사용합니다:

1. **마운트 좌표**: 차량 로컬 (원점은 차량 중심)
2. **화면 좌표**: 터미널 단위 (열/행)
3. **픽셀 좌표**: SDL 픽셀 (타일 렌더링용)

변환:

```cpp
// 마운트 -> 화면 (커서 오프셋 및 회전 포함)
point screen = ( mount + cursor_offset ).rotate( 3 ) + window_center;

// 화면 -> 픽셀
point pixel = screen * point( termx_to_pixel_value(), termy_to_pixel_value() );
```

### 성능

- 가능한 경우 타일 조회 캐싱
- 변경 시에만 다시 그리기 (더티 플래그 사용)
- 과도한 텍스처 스케일링을 방지하기 위해 줌 레벨 제한

### 폴백 동작

- 타일 로드에 실패하면 자동으로 ASCII로 폴백
- Curses 전용 빌드는 여전히 컴파일되고 작동해야 함

---

## 테스트 체크리스트

- [ ] 모든 차량 파트에 대해 타일이 올바르게 렌더링됨
- [ ] 줌 인/아웃이 원활하게 작동함
- [ ] 커서 위치가 명확하게 표시됨
- [ ] 파트 하이라이트 작동 (선택된 파트가 눈에 띔)
- [ ] `VEHICLE_EDIT_TILES` 옵션이 그래픽 설정에 표시됨 (타일 빌드만)
- [ ] 옵션 비활성화 시 ASCII 디스플레이로 폴백
- [ ] Curses 전용 빌드가 여전히 컴파일됨 (TILES 정의되지 않음)
- [ ] 대형 차량(50+ 파트)에 대한 성능이 허용 가능함
- [ ] 열린 문이 올바른 "열림" 타일 변형을 표시함
- [ ] 손상된 파트가 올바른 "손상됨" 타일 변형을 표시함
- [ ] 차량 회전이 올바르게 표시됨
