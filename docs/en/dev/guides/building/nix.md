---
title: Nix
---

This repo ships a flake (`flake.nix`) that builds the game through the same CMake setup
described in [CMake](./cmake.md). Nix provides the dependencies and the install prefix;
nothing about the build itself is reimplemented.

Requires Nix with flakes enabled. Everything else, toolchain (clang 22), SDL3 stack, and
shadercross, comes from nixpkgs, so there is no system setup beyond Nix itself.

```sh
nix run .#tiles      # SDL3 + sound game (default)
nix build .#curses   # ncurses text build
nix build .#cata-test
nix flake check      # calendar smoke test + --check-mods
nix develop          # shell with cmake, ninja, git, nixfmt
```

The game revision is the checkout itself (`src = self`), so the flake tracks whatever the
working tree has. The binary banner combines the current stable version with the flake
revision. Flake sources carry no `.git`, so CMake's version calls get answered by a
stand-in script built from the flake's revision and commit time (`nix/package.nix`,
`flake.nix` holds the exact values).

`gfx/` ships pre-composed, so the game package already bundles every tileset.

## Registry mods and soundpack

Release builds bundle the mod-registry content from `data/mods/external.json` via
`build-scripts/bundle-registry-mods.ts` (external mods into `data/mods`, soundpacks
into `data/sound`). The flake mirrors that with `nix/registry-mods.nix`: a pure
derivation that pins each archive plus a snapshot of the registry metadata, extracts
them with the same semantics (lock `extract_path` wins, marker files verified), and
links the result into the game packages. Mods ship in every game build; the Otopack
soundpack only where sound is on. `--check-mods` therefore validates the bundled
registry content too.

Branch archives are moving targets, so any upstream commit breaks its pin. Refresh
with the updater app (it plans first, `--check` previews, and applying keeps a backup
with post-write verification):

```sh
nix run .#update-registry-mods -- --check   # preview
nix run .#update-registry-mods              # rewrite pins
nix flake show                              # eval still clean (`check --no-build`
                                            # forbids the eval-time registry fetch)
```

then review the diff and commit. The updater's mocked suite (`nix/test-update-registry-mods.sh`)
runs as the `updater-tests` flake check.

## Packages

- `tiles`: SDL3 + sound game, the default (`nix run .#tiles` plays it).
- `curses`: text build, no sound.
- `cata-test`: Catch2 test binary.

`nix/package.nix` passes `tiles`, `sound`, and `useXdgDir` straight through to the
matching CMake options; `buildTests` selects the test binary. The toolchain is LLVM 22,
since the docs require clang >= 22.

## Checks

`nix flake check` runs the `[calendar]` smoke test through the Catch2 binary and
`--check-mods` through the game binary, the same content gate as
`./build-scripts/lint-json.sh`. The test binary resolves its data relative to the working
directory, so the check links `./data`, `./gfx`, and `./lang/mo` out of the installed
package. Headless runs set `CATA_TEST_COMPUTE_ACCELERATION=cpu` because the sandbox has
no GPU, not even software Vulkan.

## Requirement differences from other guides

- System SDL3 from nixpkgs. The vendored `BUILD_SDL3` fetch needs network and cannot
  work in the sandbox, so it stays off. The curses build still needs system SDL3 for
  platform services.
- `shadercross` from nixpkgs (`BUILD_SHADERCROSS=OFF`) for the same reason.
- No ccache: nothing persists between sandbox builds.
- Default linker instead of mold.
- `libbacktrace` from nixpkgs with `LIBBACKTRACE=ON`, same as CI, so crash traces keep
  line info.
