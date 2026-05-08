# In-game debugging tools

The game features a number of inbuilt tools that aim to aid in debugging issues and testing fixes or
new additions.

Below are listed some major ones.

## Debug Message

Also known as `debugmsg`, or "Red text over black screen". Shows up when the game encounters an
error in data files or an inconsistency within its internal state.

![image](https://user-images.githubusercontent.com/60584843/192118702-a214bdeb-6376-463d-88cd-2e17b89c4eeb.png)

Severity varies, from harmless, pedantic warnings to unrecoverable crashes. A copy of each message
is saved in the [debug log](#debug-log), usually prepended with backtrace that describes where (in
the code) the error originated from.

You can press `space` to dismiss the message, or `i` to dismiss the message and all its subsequent
occurences until game restart.

You can suppress (at your own risk) all debug messages by running the game executable with
`--dont-debugmsg` command line option.

In code, you can invoke a debug message like this:

```cpp
#include "debug.h" // See this header for details
 . . .
// 1st argument is a format string, see `string_formatter.h` for details
debugmsg( "Failed to open file, code %d", 123 );
```

## Debug log

Debug log contains various status messages produced by the game, including general info, warnings or
hard errors. It's usually the go-to place for initial diagnostic of most bugs.

It's contained in a single file named `debug.log` which is located in user's `config` folder.

You can manually control debug log verbosity in game options (`Options` -> `Debug` tab -> `Logging`
submenu).

![image](https://user-images.githubusercontent.com/60584843/192119510-adeb2df9-6698-452d-b68a-d329c3a71024.png)

In code, you can print to the log like this:

```cpp
#include "debug.h" // See this header for details
 . . .
// 1st argument is severity level, 2nd argument is category
// Don't append a newline at the end, it will be done automatically.
DebugLogFL( DL::Info, DC::Map ) << "My " << "logged" << " info!";
```

## Crash log

Crash log contains backtrace after game crash. It's extremely helpful for solving crashes, as it
essentially holds the info about what code (and where) experienced the crash.

It's contained in a single file named `crash.log` which is located in user's `config` folder.

Sometimes, if crash is "weird" enough, or the executable was compiled without backtrace support,
crash log is not generated.

In code, you don't have to add any special logic for crash traces, but the build must have
backtraces enabled. Some builds also support optional `libbacktace` that may make the backtraces
more readable. If it takes a long time (up to half a minute) to generate backtraces on Linux,
consider compiling with `clang++` instead of `g++`.

## Debug mode

Not to be confused with [debug menu](#debug-menu), debug mode makes the game produce a lot of noisy
messages in the log, and enables additional functionality in some UI windows.

For example, additional info such as item id, tags and charges will be shown when examining an item,
and vehicle details menu will show pivot point, center of mass and cursor position (in vehicle
coords).

Debug messages are mostly pertaining to vehicle, effect, sound and NPC processing.

![image](https://user-images.githubusercontent.com/60584843/192119060-c8257774-dcc5-4826-af1b-bc59898cdc7f.png)
![image](https://user-images.githubusercontent.com/60584843/192119544-84bb03c7-8a01-4c7d-9024-b8a540f337dc.png)

To access debug mode switch, bind `Toggle Debug Mode` action to some key while in "default" mode
(terrain view with character in the center who you control). Reminder: you can access keybinds list
by pressing `?`.

In code, you can check for debug mode via `debug_mode` variable found in `cached_options.h`.

## Debug menu

Also known as "cheat menu", not to be confused with [debug mode](#debug-mode). The best friend when
it comes to testing.

![image](https://user-images.githubusercontent.com/60584843/192119123-5aa81c1d-95c5-43ae-8b62-c2e494ead1a1.png)

It's hidden by default. To access it, bind `Debug Menu` action to any key (backtick key \` is the
usual choice), and then either press that key, or select `Debug Menu` entry from `Esc` menu.

Most of the functionality there is clearly labeled and split into categories, feel free to explore.

There's a somewhat hidden feature called "Debug Mutations", which resembles cheat codes in older
games and grants your character immunity to some game mechanics. To quickly access them, press
`p`(Player)->`u`(Mutate)->`/`(Filter) and enter `Debug` in the search box.

## Additional JSON checks

It's possible to enable additional JSON data checks, mostly useful for content makers. The list
includes, but not limited to, the following:

1. Unused fields, such as caused by a typo in JSON or a deprecated field being removed from the
   game.
2. Deprecation warnings and migration advice.
3. Checks for potential weird behavior.

Old or infrequently maintained mods may trigger hundreds of these, so the option is disabled by
default. You can enable it in game options (`Options` -> `Debug` tab ->
`Report unused JSON fields`).

If you'd like to propose your changes to BN, keep in mind that both the testing executable
`tests/cata_test[.exe]` and automated PR checks in the repository are run with this option
**enabled**.

TODO: rename the option to imply all listed checks, maybe also enable it by default.

![image](https://user-images.githubusercontent.com/60584843/192119882-5c45ed88-2dee-495a-8457-c767dbe2d53d.png)

## Tileset reload and report

Useful for tileset makers, this feature reloads the active tileset from disk, redraws the screen and
writes completeness report to the [debug log](#debug-log).

To use the feature, you'll have to bind `Reload Tileset` action to any key and press it.

![image](https://user-images.githubusercontent.com/60584843/192119479-cba0d733-af66-4ae0-94d9-5af33b13cfe3.png)
