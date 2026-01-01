# Adding Mouse Support to UI Elements

This guide shows you how to add mouse interaction (clicking and hovering) to UI elements using the [canvas-based callback system](https://github.com/cataclysmbn/Cataclysm-BN/issues/7736).

## Quick Start

The mouse callback system lets you attach click and hover handlers directly to terminal cells as they're printed. No manual coordinate synchronization needed—callbacks are stored in the canvas and automatically dispatched when mouse events occur.

**Minimum example:**

```cpp
#include "output.h"
#include "mouse_input.h"

// Set callback, print text, callbacks automatically attached
set_mouse_callback( window, mouse_callback_options{
    .on_click = []( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            do_action();
        }
    }
} );

mvwprintw( window, point( 10, 5 ), "Click Me" );
clear_mouse_callback( window );
```

## Architecture Overview

### Mouse Event Types

```cpp
enum class mouse_button {
    none, left, right, middle,
    scroll_up, scroll_down, back, forward
};

enum class mouse_event_type {
    click_down,     // Button pressed
    click_up,       // Button released
    hover_enter,    // Cursor entered cell
    hover_exit,     // Cursor left cell
    hover_move      // Cursor moving within region
};

struct mouse_event {
    point pos;              // Screen-space position
    point local_pos;        // Window-relative position
    mouse_button button;
    mouse_event_type type;
    bool shift_held, ctrl_held, alt_held;
    bool left_held, right_held, middle_held;  // For drag detection
};
```

### How It Works

1. **Callback Storage**: Each `cursecell` has `on_click` and `on_hover` callback pointers
2. **Attachment**: `set_mouse_callback()` sets a "current callback" for the window
3. **Printing**: All print functions check for current callback and attach to written cells
4. **Dispatch**: SDL/ncurses mouse events → coordinate lookup → callback invocation
5. **Window Stack**: Hit-testing resolves topmost visible window at cursor position

## Two Approaches

Choose based on your use case:

### Approach 1: Canvas Callbacks (Recommended)

**Best for:** New UI code, simple interactions, when drawing and logic are coupled.

```cpp
// RAII-style (recommended)
{
    scoped_mouse_callback guard( window, mouse_callback_options{
        .on_click = []( const mouse_event &ev ) { 
            if( ev.button == mouse_button::left ) {
                select_item();
            }
        },
        .on_hover = []( const mouse_event &ev ) {
            if( ev.type == mouse_event_type::hover_enter ) {
                highlight_item();
            }
        }
    } );

    mvwprintw( window, point( 10, 5 ), "OK" );
}  // Callbacks cleared automatically
```

**Pros:** Simple, automatic cleanup, no coordinate math
**Cons:** All printed text in scope gets the same callback

### Approach 2: Manual Hit-Testing (For Complex UI)

**Best for:** Existing UI with complex layout, different actions per item, legacy code.

```cpp
// 1. Track clickable regions while drawing
struct button_region {
    inclusive_rectangle<point> area;
    int action_id;
};
std::vector<button_region> buttons;

auto btn_pos = point( 10, 5 );
mvwprintw( window, btn_pos, "OK" );
buttons.push_back( button_region{
    .area = inclusive_rectangle<point>( btn_pos, btn_pos + point( 2, 0 ) ),
    .action_id = ACTION_OK
} );

// 2. Register mouse actions in input_context
input_context ctxt( "MY_UI" );
ctxt.register_action( "MOUSE_MOVE" );
ctxt.register_action( "SELECT" );
ctxt.register_action( "SCROLL_UP" );   // CRITICAL: Required for mouse wheel scrolling
ctxt.register_action( "SCROLL_DOWN" ); // CRITICAL: Required for mouse wheel scrolling

// 3. Handle in input loop
auto action = ctxt.handle_input();
if( action == "SELECT" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &btn : buttons ) {
        if( btn.area.contains( mouse_pos ) ) {
            handle_action( btn.action_id );
            break;
        }
    }
}
```

**Pros:** Precise per-item control, works with existing patterns
**Cons:** Manual coordinate tracking, more boilerplate

## Critical: Coordinate Conversion

**In TILES mode, mouse coordinates are PIXELS, not cells. You MUST convert:**

```cpp
#if defined(TILES)
extern int fontwidth;
extern int fontheight;
#endif

static auto pixel_to_cell( point pixel_pos ) -> point
{
#if defined(TILES)
    return point { pixel_pos.x / fontwidth, pixel_pos.y / fontheight };
#else
    return pixel_pos;  // Already in cells for curses
#endif
}

// Usage:
auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );
```

**Forgetting this causes clicks to be off by font size (12-16x).**

## Common Patterns

### Button with Hover Effect

```cpp
auto highlighted = false;

scoped_mouse_callback guard( window, mouse_callback_options{
    .on_click = [&]( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            execute_action();
        }
    },
    .on_hover = [&]( const mouse_event &ev ) {
        highlighted = ( ev.type == mouse_event_type::hover_enter );
        redraw();
    }
} );

mvwprintw( window, pos, highlighted ? "[ OK ]" : "  OK  " );
```

### Right-Click Context Menu

```cpp
scoped_mouse_callback guard( window, mouse_callback_options{
    .on_click = [&]( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            select_item();
        } else if( ev.button == mouse_button::right ) {
            show_context_menu( ev.pos );
        }
    }
} );
```

### Drag Detection

```cpp
auto drag_start = std::optional<point>{};

scoped_mouse_callback guard( window, mouse_callback_options{
    .on_click = [&]( const mouse_event &ev ) {
        if( ev.type == mouse_event_type::click_down && ev.button == mouse_button::left ) {
            drag_start = ev.pos;
        } else if( ev.type == mouse_event_type::click_up && drag_start.has_value() ) {
            handle_drag( *drag_start, ev.pos );
            drag_start = std::nullopt;
        }
    }
} );
```

### List Items (Different Action Per Row)

Use manual hit-testing for this:

```cpp
struct list_item_region {
    inclusive_rectangle<point> area;
    size_t item_index;
};
std::vector<list_item_region> item_regions;

// While rendering:
for( size_t i = 0; i < items.size(); ++i ) {
    auto item_pos = point( list_x, list_y + i );
    mvwprintw( window, item_pos, items[i].name );

    item_regions.push_back( list_item_region{
        .area = inclusive_rectangle<point>( item_pos, item_pos + point( list_width, 0 ) ),
        .item_index = i
    } );
}

// In input handler:
if( action == "SELECT" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &region : item_regions ) {
        if( region.area.contains( mouse_pos ) ) {
            select_item( region.item_index );
            return;
        }
    }
}
```

### Scrollable Content (CRITICAL)

**If your UI has ANY scrollable content (text, lists, etc.), you MUST register scroll actions:**

```cpp
input_context ctxt( "MY_UI" );
ctxt.register_action( "SCROLL_UP" );   // Mouse wheel up
ctxt.register_action( "SCROLL_DOWN" ); // Mouse wheel down
// ... other actions
```

**Complete scrollbar interaction example:**

```cpp
// 1. Draw scrollbar and track its region
draw_scrollbar( w_border, scroll_pos, viewport_height, content_lines, point_south, BORDER_COLOR, true );

// Store scrollbar position for click handling
std::optional<inclusive_rectangle<point>> scrollbar_region;
int scroll_max = 0;

if( content_lines > viewport_height ) {
    auto scrollbar_x = border_x + point_south.x;
    auto scrollbar_y = border_y + point_south.y;
    scrollbar_region = inclusive_rectangle<point>(
        point( scrollbar_x, scrollbar_y ),
        point( scrollbar_x, scrollbar_y + viewport_height - 1 )
    );
    scroll_max = content_lines - viewport_height;
}

// 2. Handle scroll events
auto action = ctxt.handle_input();

// Handle keyboard/wheel scrolling
if( action == "UP" || action == "PAGE_UP" || action == "SCROLL_UP" ) {
    if( scroll_pos > 0 ) {
        scroll_pos--;
    }
} else if( action == "DOWN" || action == "PAGE_DOWN" || action == "SCROLL_DOWN" ) {
    if( scroll_pos < scroll_max ) {
        scroll_pos++;
    }
}

// Handle scrollbar clicks
if( action == "SELECT" && scrollbar_region.has_value() &&
    scrollbar_region->contains( mouse_pos ) ) {
    auto relative_y = mouse_pos.y - scrollbar_region->p_min.y;
    auto scrollbar_height = scrollbar_region->p_max.y - scrollbar_region->p_min.y + 1;

    if( relative_y == 0 ) {
        // Up arrow clicked
        if( scroll_pos > 0 ) {
            scroll_pos--;
        }
    } else if( relative_y == scrollbar_height - 1 ) {
        // Down arrow clicked
        if( scroll_pos < scroll_max ) {
            scroll_pos++;
        }
    } else {
        // Track/bar clicked - jump to position
        auto slot_height = scrollbar_height - 2;
        auto slot_pos = relative_y - 1;
        auto new_scroll_pos = ( slot_pos * scroll_max ) / slot_height;
        scroll_pos = std::clamp( new_scroll_pos, 0, scroll_max );
    }
}
```

**See**: `src/main_menu.cpp` `display_text()` and `opening_screen()` for a complete working example.

## Step-by-Step Migration Example

**Before (keyboard-only menu):**

```cpp
void show_menu() {
    input_context ctxt( "MENU" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "QUIT" );

    catacurses::window w = catacurses::newwin( 10, 30, point( 5, 5 ) );

    while( true ) {
        werase( w );
        mvwprintw( w, point( 2, 2 ), "[O]K" );
        mvwprintw( w, point( 2, 4 ), "[C]ancel" );
        wrefresh( w );

        auto action = ctxt.handle_input();
        if( action == "SELECT" ) {
            return;
        }
    }
}
```

**After (with mouse support using manual hit-testing):**

```cpp
void show_menu() {
    input_context ctxt( "MENU" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "MOUSE_MOVE" );  // Added for hover
    ctxt.register_action( "SCROLL_UP" );   // Added for mouse wheel (if menu scrolls)
    ctxt.register_action( "SCROLL_DOWN" ); // Added for mouse wheel (if menu scrolls)

    catacurses::window w = catacurses::newwin( 10, 30, point( 5, 5 ) );

    struct button_area {
        inclusive_rectangle<point> area;
        std::string action;
    };

    while( true ) {
        werase( w );

        std::vector<button_area> buttons;

        auto ok_pos = point( 2, 2 );
        mvwprintw( w, ok_pos, "[O]K" );
        buttons.push_back( button_area{
            .area = inclusive_rectangle<point>( ok_pos, ok_pos + point( 3, 0 ) ),
            .action = "SELECT"
        } );

        auto cancel_pos = point( 2, 4 );
        mvwprintw( w, cancel_pos, "[C]ancel" );
        buttons.push_back( button_area{
            .area = inclusive_rectangle<point>( cancel_pos, cancel_pos + point( 7, 0 ) ),
            .action = "QUIT"
        } );

        wrefresh( w );

        auto action = ctxt.handle_input();

        // Handle mouse clicks
        if( action == "MOUSE_MOVE" || action == "SELECT" ) {
            auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );
            
            for( const auto &btn : buttons ) {
                if( btn.area.contains( mouse_pos ) ) {
                    if( action == "SELECT" ) {
                        action = btn.action;  // Treat click as that action
                    }
                    // Could add hover highlighting here
                    break;
                }
            }
        }

        if( action == "SELECT" ) {
            return;
        } else if( action == "QUIT" ) {
            return;
        }
    }
}
```

## Testing Checklist

- [ ] Build succeeds in both TILES and curses modes
- [ ] Left-click activates correct action
- [ ] Right-click works (if implemented)
- [ ] Hover effects appear (if implemented)
- [ ] Clicks outside active regions do nothing
- [ ] Works with window scrolling (if applicable)
- [ ] No coordinate offset issues (test at different screen resolutions)

## Troubleshooting

| Problem                      | Solution                                                                        |
| ---------------------------- | ------------------------------------------------------------------------------- |
| Clicks offset by 12-16 cells | Add `pixel_to_cell()` conversion                                                |
| Callbacks not firing         | Check `input_context` has `"MOUSE_MOVE"` or `"SELECT"` registered               |
| **Mouse wheel not working**  | **Register `"SCROLL_UP"` and `"SCROLL_DOWN"` actions in input_context**         |
| Scrollbar clicks not working | Track scrollbar region and handle in `"SELECT"` action (see Scrollable Content) |
| Wrong button clicked         | Check both `ev.button` and `ev.type` (down vs up)                               |
| Hover not working            | Ensure `on_hover` is set in `mouse_callback_options`                            |
| Overlapping regions          | Hit-test in reverse drawing order (last drawn = topmost)                        |

## Code References

- **Main example**: `src/main_menu.cpp` (commit 4ce6f29d14)
- **Mouse types**: `src/mouse_input.h`
- **Callback API**: `src/output.h` (`mouse_callback_options`, `scoped_mouse_callback`)
- **Input handling**: `src/input.h` (`input_context`)
- **Issue/PR**: #7736, #7737

## Design Philosophy

The canvas callback system follows the principle from issue #7736:

> "No need to synchronize coordinates between mouse click callback and UI output functions, no need to handle how windows obscure one another—just sample the canvas under cursor, grab the callback pointer and call the function."

For simple UIs, use canvas callbacks (Approach 1). For complex existing UIs, manual hit-testing (Approach 2) provides more control while still being cleaner than the old coordinate synchronization approach.
