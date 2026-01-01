# User Interface

Cataclysm: Bright Nights uses ncurses, or in the case of the tiles build, an ncurses port, for user
interface. Window management is achieved by `ui_adaptor`, which requires a resizing callback and a
redrawing callback for each UI to handle resizing and redrawing. Details on how to use `ui_adaptor`
can be found within `ui_manager.h`.

Some good examples of the usage of `ui_adaptor` can be found within the following files:

- `query_popup` and `static_popup` in `popup.h/cpp`
- `Messages::dialog` in `messages.cpp`

## Mouse Support

Since [PR #7737](https://github.com/cataclysmbn/Cataclysm-BN/pull/7737), the game supports a canvas-based mouse callback system. Mouse interaction (clicking and hovering) can be added to UI elements by attaching callbacks directly to terminal cells as they're printed.

### Architecture

- **Callback Storage**: Each `cursecell` stores optional `on_click` and `on_hover` callback pointers (`mouse_callback_ptr`)
- **Context API**: `set_mouse_callback()` and `scoped_mouse_callback` in `output.h` manage callback attachment
- **Dispatch**: SDL/ncurses mouse events are converted to `mouse_event` structs and dispatched through the window stack
- **Hit-Testing**: The topmost visible window at cursor position receives the event

### Quick Example

```cpp
scoped_mouse_callback guard( window, mouse_callback_options{
    .on_click = []( const mouse_event &ev ) {
        if( ev.button == mouse_button::left ) {
            handle_click();
        }
    }
} );

mvwprintw( window, point( 10, 5 ), "Click Me" );
// Callback automatically attached to printed cells
```

### Documentation

- **How-to Guide**: [Adding Mouse Support](../guides/mouse_support.md)
- **API Reference**: `src/mouse_input.h`, `src/output.h`
- **Working Example**: `src/main_menu.cpp`

### Design Principle

The system eliminates manual coordinate synchronization between drawing and input handling. Callbacks are stored directly in the canvas, and the window manager handles hit-testing and dispatch automatically.
