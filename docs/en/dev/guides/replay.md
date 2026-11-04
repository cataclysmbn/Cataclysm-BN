---
title: Recording and replaying input
---

Record a game session with `--replay-record PATH`. Play it with `--replay-play PATH`.

```sh
./cataclysm-bn-tiles --userdir out/record/ --configdir out/record/config/ \
  --seed example --replay-record out/example.jsonl
./cataclysm-bn-tiles --userdir out/play/ --configdir out/play/config/ \
  --replay-play out/example.jsonl
```

Start both runs from identical copies of the user directory. A replay is an input recording. It does not restore the world, configuration, or keybindings.

Use the same build, data, mods, options, and window size. The header supplies the RNG seed. An explicit `--seed` on playback must match it.

A boundary mismatch, an unknown format version, a malformed record, or a missing end record stops playback with the file and line.

## Compare two runs

Add `--state-hash-log PATH` to both runs. The log has one line after each turn, with hashes of the RNG, avatar, monsters, NPCs, and the map around the avatar. Compare the two files to find the first turn that differs.
