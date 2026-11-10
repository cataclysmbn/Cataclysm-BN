---
title: Engine and clients
---

One executable links the enabled clients and selects one at startup with `--client=tiles`, `--client=curses` or `--client=mcp`. `TILES`, `CURSES` and `MCP` choose which are built. The default prefers tiles, then curses, then MCP.

All clients run the same game loop and `input_context` logic, and draw into the same `catacurses` cell windows. A native client shows these cells directly. An external client gets them over the [protocol](../reference/engine_client_protocol.md) at each input boundary.

To add a client, implement the device interface, register its factory, and add its native sources to `CataClientSources.cmake`.
