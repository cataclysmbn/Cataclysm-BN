---
description: Add canvas-based mouse support to UI elements with callback system
argument-hint: ui-element-name [approach]
---

# Add Mouse Support to UI Elements

This command helps you add mouse interaction (click and hover support) to UI elements using the canvas-based callback system.

Human readable docs are at [docs/en/dev/guides/mouse_support.md](../../docs/en/dev/guides/mouse_support.md).

> [!CAUTION]
> You **MUST** update [docs/en/dev/guides/mouse_support.md](../../docs/en/dev/guides/mouse_support.md) after making changes here.

## Overview

The mouse callback system allows you to attach click and hover handlers directly to terminal cells as they're printed. No need to manually synchronize coordinates between drawing and input handling—callbacks are automatically stored in the canvas and dispatched when mouse events occur.

## Core Concepts

### Mouse Event Types

```cpp
enum class mouse_button {
    none, left, right, middle,
    scroll_up, scroll_down, back, forward
};

enum class mouse_event_type {
    click_down, click_up,
    hover_enter, hover_exit, hover_move
};

struct mouse_event {
    point pos;          // Screen position
    point local_pos;    // Window-relative position
    mouse_button button;
    mouse_event_type type;
    bool shift_held, ctrl_held, alt_held;
    bool left_held, right_held, middle_held;  // For drag detection
};
```

### Callback Types

```cpp
using mouse_callback_t = std::function<void( const mouse_event & )>;

struct mouse_callback_options {
    mouse_callback_t on_click;
    std::optional<mouse_callback_t> on_hover = std::nullopt;
};
```

## Implementation Approaches

Choose based on your UI complexity:

### Approach 1: Canvas Callbacks (Recommended for New Code)

**Best for**: New UI code, simple interactions, when drawing and logic are tightly coupled.

```cpp
// Set callback context, then print normally
set_mouse_callback( window, mouse_callback_options{
    .on_click = []( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            do_action();
        }
    },
    .on_hover = []( const mouse_event &ev ) {
        show_tooltip();
    }
} );

// All subsequent prints attach these callbacks to their cells
mvwprintw( window, point( 10, 5 ), "Click Me" );

// Clear callbacks when done
clear_mouse_callback( window );
```

**Or use RAII**:

```cpp
{
    scoped_mouse_callback guard( window, mouse_callback_options{
        .on_click = []( const mouse_event &ev ) { do_action(); }
    } );

    mvwprintw( window, point( 10, 5 ), "Click Me" );
}  // Callbacks cleared automatically
```

### Approach 2: Manual Hit-Testing (For Existing Complex UI)

**Best for**: Existing UI with complex layout, when callbacks would be messy, legacy code migration.

```cpp
// 1. Track clickable regions while drawing
struct button_region {
    inclusive_rectangle<point> area;
    int action_id;
};
std::vector<button_region> buttons;

// While drawing:
auto button_pos = point( 10, 5 );
auto button_text = "OK";
mvwprintw( window, button_pos, button_text );
buttons.push_back( button_region{
    .area = inclusive_rectangle<point>(
        button_pos,
        button_pos + point( utf8_width( button_text ), 0 )
    ),
    .action_id = ACTION_OK
} );

// 2. Add MOUSE_MOVE and SELECT to input_context
ctxt.register_action( "MOUSE_MOVE" );
ctxt.register_action( "SELECT" );

// 3. In input loop, handle mouse events
auto action = ctxt.handle_input();

if( action == "SELECT" || action == "MOUSE_MOVE" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &btn : buttons ) {
        if( btn.area.contains( mouse_pos ) ) {
            if( action == "SELECT" ) {
                handle_action( btn.action_id );
            } else {
                highlight_button( btn.action_id );
            }
            break;
        }
    }
}
```

### Coordinate Conversion (CRITICAL for TILES mode)

Mouse coordinates from `input_context` are in **pixels** (TILES) or **cells** (curses). You must convert pixels to cells:

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
    return pixel_pos;
#endif
}

// Usage in input handling:
auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );
```

**Why this matters**: Forgetting this conversion will cause clicks to be off by a factor of font size (typically 12-16x).

## Step-by-Step Implementation

### For Simple Buttons/Items

1. **Choose approach** (Approach 1 recommended for new code)

2. **Register mouse actions** (if using manual hit-testing):
   ```cpp
   input_context ctxt( "MY_UI" );
   ctxt.register_action( "MOUSE_MOVE" );  // For hover
   ctxt.register_action( "SELECT" );      // For clicks
   ctxt.register_action( "SCROLL_UP" );   // For mouse wheel up (CRITICAL for scrollable content)
   ctxt.register_action( "SCROLL_DOWN" ); // For mouse wheel down (CRITICAL for scrollable content)
   ```
   
   **IMPORTANT**: If your UI has any scrollable content (lists, text, etc.), you **MUST** register `SCROLL_UP` and `SCROLL_DOWN` or mouse wheel scrolling will not work!

3. **Set up callbacks or tracking**:
   - **Approach 1**: Call `set_mouse_callback()` before printing
   - **Approach 2**: Create tracking structure for hit regions

4. **Draw your UI normally**:
   ```cpp
   mvwprintw( window, point( x, y ), "Button Text" );
   ```

5. **Handle events** (Approach 2 only):
   ```cpp
   if( action == "SELECT" ) {
       auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );
       // Hit-test against your tracked regions
   }
   ```

### For Complex Regions (Lists, Tables, etc.)

```cpp
// Track each list item's screen position
struct list_item_region {
    inclusive_rectangle<point> area;
    size_t item_index;
};
std::vector<list_item_region> item_regions;

// While rendering list:
for( size_t i = 0; i < items.size(); ++i ) {
    auto item_pos = point( list_x, list_y + i );
    mvwprintw( window, item_pos, items[i].name );

    item_regions.push_back( list_item_region{
        .area = inclusive_rectangle<point>(
            item_pos,
            item_pos + point( list_width, 0 )
        ),
        .item_index = i
    } );
}

// In input handler:
if( action == "SELECT" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &region : item_regions ) {
        if( region.area.contains( mouse_pos ) ) {
            select_item( region.item_index );
            break;
        }
    }
}
```

### For Scrollable Content (CRITICAL)

When adding mouse support to scrollable areas (text displays, long lists, etc.), you need:

1. **Register scroll actions** in input context:
   ```cpp
   ctxt.register_action( "SCROLL_UP" );   // Mouse wheel up
   ctxt.register_action( "SCROLL_DOWN" ); // Mouse wheel down
   ```

2. **Track scrollbar region** during drawing:
   ```cpp
   // In your drawing function
   draw_scrollbar( w_border, scroll_pos, viewport_height, content_lines, point_south, BORDER_COLOR, true );
   
   // Store scrollbar position for click handling
   if( content_lines > viewport_height ) {
       auto scrollbar_x = border_x + point_south.x;
       auto scrollbar_y = border_y + point_south.y;
       scrollbar_region = inclusive_rectangle<point>(
           point( scrollbar_x, scrollbar_y ),
           point( scrollbar_x, scrollbar_y + viewport_height - 1 )
       );
       scroll_max = content_lines - viewport_height;
   }
   ```

3. **Handle scroll events**:
   ```cpp
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

## Complete Working Example

See `src/main_menu.cpp` (commit 4ce6f29d14) for a real-world implementation:

```cpp
// 1. Coordinate conversion helper
static auto pixel_to_cell( point pixel_pos ) -> point
{
#if defined(TILES)
    return point { pixel_pos.x / fontwidth, pixel_pos.y / fontheight };
#else
    return pixel_pos;
#endif
}

// 2. Track button regions while drawing
std::vector<std::pair<inclusive_rectangle<point>, int>> button_map;

// While printing menu items:
for( size_t i = 0; i < menu_items.size(); ++i ) {
    auto button_area = print_button( window, pos, menu_items[i] );
    button_map.emplace_back( button_area, i );
}

// 3. Register mouse actions
input_context ctxt( "MAIN_MENU" );
ctxt.register_action( "MOUSE_MOVE" );
ctxt.register_action( "SELECT" );
ctxt.register_action( "SCROLL_UP" );   // Required for mouse wheel
ctxt.register_action( "SCROLL_DOWN" ); // Required for mouse wheel

// 4. Handle in input loop
auto action = ctxt.handle_input();

if( action == "MOUSE_MOVE" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &[area, item_id] : button_map ) {
        if( area.contains( mouse_pos ) ) {
            highlight_menu_item( item_id );
            break;
        }
    }
}

if( action == "SELECT" ) {
    auto mouse_pos = pixel_to_cell( ctxt.get_raw_input().mouse_pos );

    for( const auto &[area, item_id] : button_map ) {
        if( area.contains( mouse_pos ) ) {
            activate_menu_item( item_id );
            return;
        }
    }
}
```

## Common Patterns

### Button with Hover Effect

```cpp
auto button_highlighted = false;

set_mouse_callback( window, mouse_callback_options{
    .on_click = [&]( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            execute_action();
        }
    },
    .on_hover = [&]( const mouse_event &ev ) {
        if( ev.type == mouse_event_type::hover_enter ) {
            button_highlighted = true;
            redraw();
        } else if( ev.type == mouse_event_type::hover_exit ) {
            button_highlighted = false;
            redraw();
        }
    }
} );

mvwprintw( window, pos, button_highlighted ? "[ OK ]" : "  OK  " );
clear_mouse_callback( window );
```

### Context Menu (Right-Click)

```cpp
set_mouse_callback( window, mouse_callback_options{
    .on_click = []( const mouse_event &ev ) {
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

set_mouse_callback( window, mouse_callback_options{
    .on_click = [&]( const mouse_event &ev ) {
        if( ev.type == mouse_event_type::click_down && ev.button == mouse_button::left ) {
            drag_start = ev.pos;
        } else if( ev.type == mouse_event_type::click_up && drag_start.has_value() ) {
            auto drag_end = ev.pos;
            handle_drag( *drag_start, drag_end );
            drag_start = std::nullopt;
        }
    }
} );
```

## Testing Checklist

After implementing mouse support:

- [ ] Build succeeds
  ```sh
  cmake --build --preset linux-full --target cataclysm-bn-tiles
  ```

- [ ] Format code
  ```sh
  cmake --build out/build/linux-full --target astyle
  ```

- [ ] Test in TILES mode:
  - [ ] Left-click activates correct action
  - [ ] Hover shows correct visual feedback (if applicable)
  - [ ] Right-click works (if applicable)
  - [ ] Clicks on overlapping elements resolve correctly

- [ ] Test in curses mode (if applicable):
  - [ ] Mouse events still work
  - [ ] No coordinate conversion issues

- [ ] Edge cases:
  - [ ] Click outside active region does nothing
  - [ ] Click on partially visible elements
  - [ ] Rapid clicking doesn't cause issues
  - [ ] Works with scrolling content (if applicable)

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Clicks are offset | Add `pixel_to_cell()` conversion |
| Callbacks not firing | Check if `input_context` has `"MOUSE_MOVE"` or `"SELECT"` registered |
| **Mouse wheel not working** | **Register `"SCROLL_UP"` and `"SCROLL_DOWN"` actions in your input_context** |
| Scrollbar clicks not working | Track scrollbar region and handle in `"SELECT"` action (see Scrollable Content section) |
| Wrong button clicked | Check both `ev.button` and `ev.type` (down vs up) |
| Hover not working | Ensure `on_hover` is set in `mouse_callback_options` |
| Overlapping regions | Hit-test in reverse drawing order (last drawn = topmost) |
| Memory issues | Use `scoped_mouse_callback` for RAII cleanup |

## Files to Modify

| File | Purpose |
|------|---------|
| `src/mouse_input.h` | Mouse event types (reference only) |
| `src/output.h` | Callback API (reference only) |
| Your UI file | Implementation |

## References

- **Mouse Infrastructure**: Issue #7736, PR #7737
- **Working Example**: `src/main_menu.cpp` (commit 4ce6f29d14)
- **API Reference**: `src/output.h` (mouse_callback_options, scoped_mouse_callback)
- **Input Context**: `src/input.h` (register_action, handle_input)

## Design Guidelines

Following AGENTS.md conventions:

- **Use `auto`** for all type declarations
- **Trailing return types**: `auto foo() -> void`
- **Designated initializers**: `mouse_callback_options{ .on_click = ... }`
- **Options structs**: For helpers with >3 parameters
- **Lambdas**: Prefer captures for local state
- **RAII**: Use `scoped_mouse_callback` over manual cleanup
