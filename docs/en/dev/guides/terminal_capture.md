# Recording terminal captures

To show a bug or a fix in a PR, record the curses build with asciinema. Then turn the recording
into a short animated WebP with `just term-capture`.
[PR #11041](https://github.com/cataclysmbn/Cataclysm-BN/pull/11041) has a before and an after
capture.

## Requirements

- the curses build: `cmake --preset linux-curses` and
  `cmake --build --preset linux-curses --target cataclysm-bn` (see [CMake](./building/cmake.md))
- [asciinema](https://docs.asciinema.org/) to record
- [agg](https://github.com/asciinema/agg) to render the recording
- [uv](https://docs.astral.sh/uv/) to run the script with Pillow

## Record

1. Make the terminal small, for example 120 columns and 40 rows. The file is smaller with a small
   terminal.
2. From the repository root, start the game in asciinema with a time limit and separate user
   and config directories:

   ```sh
   asciinema rec -c 'timeout -k 10s 600 out/build/linux-curses/src/cataclysm-bn --userdir capture/ --configdir capture/config/' before.cast
   ```

   With `--userdir` and `--configdir`, the game uses new saves and settings and does not change
   your own. `timeout` stops the game after 600 s. Without it, the game can keep running after you
   close the terminal window.
3. Use the debug menu to set up the scene, then do the steps that show the bug.
4. Quit the game from its menu.
5. Record `after.cast` the same way with the fixed build.

## Make the WebP

1. Render the recording as a GIF:

   ```sh
   just term-capture render before.cast before.gif
   ```

   The GIF plays at 2x speed. Pauses longer than 1 s become 0.5 s.
2. Save each frame of the GIF as a PNG file:

   ```sh
   just term-capture frames before.gif before-frames/
   ```

3. Find the first frame of each player action. Write them to `scenes.json`, one scene per action.
   `start` is the number in the PNG file name. Use the words the game shows in the caption. Make the
   last scene `red` for the bug or `green` for the fix.

   ```json
   [
     { "start": 40, "caption": "enable UPS charging" },
     { "start": 67, "caption": "next turn: invalid ammo error", "color": "red" }
   ]
   ```

4. Make the WebP. Give the number of the last frame to keep with `--last`:

   ```sh
   just term-capture encode before.gif before.webp --scenes scenes.json --last 80
   ```

   The WebP starts at the first scene and ends at the `--last` frame. Frames before the first
   scene, such as the game start and the menus, are cut. The last frame of each scene stays on
   screen for 2 s, and the last frame of the capture stays for 6 s. The encoding can take a few
   minutes.
5. Open the WebP in a browser and check each scene.

The WebP is lossless. With lossy WebP, terminal text is blurry and the file is often larger.

## Add the capture to the PR

Push the WebP files to a branch on your fork, for example `assets/<issue number>`. Put the raw URL
in the Testing section of the PR:

```md
![before](https://raw.githubusercontent.com/<you>/Cataclysm-BN/assets/9853/before.webp)
```

To replace a capture, push a new file to the same path. You do not need to edit the PR body. The
new file can take a few minutes to show.
