---
title: Engine and clients
---

A Tiles or curses build shows the game in its own window or terminal. The protocol engine `cataclysm-bn-mcp` (`-DMCP=ON`) is a separate executable without a window, a terminal or an audio device. An external client plays it over the [protocol](../reference/engine_client_protocol.md).

All of them run the same game loop and `input_context` logic, and draw into the same `catacurses` cell windows. A native build shows these cells directly. The protocol engine keeps them in memory and sends them to its client at each input boundary.
