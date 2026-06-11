# Slang Probe

This opt-in target validates the first step of the Slang compute migration:

- one Slang source compiles to CPU C++;
- the same source compiles to SPIR-V and Metal;
- the generated CPU C++ executes inside the normal CMake project.

It does not replace the current runtime HLSL/shadercross path yet.

## Linux/macOS

```sh
cmake -S . -B build/slang-probe \
  -DCATA_SLANG_PROBE=ON \
  -DTILES=OFF -DCURSES=OFF -DTESTS=OFF
cmake --build build/slang-probe --target cata_slang_probe
```

## Windows

```powershell
cmake -S . -B build/slang-probe `
  -DCATA_SLANG_PROBE=ON `
  -DTILES=OFF -DCURSES=OFF -DTESTS=OFF
cmake --build build/slang-probe --target cata_slang_probe --config Debug
```

CMake searches `CATA_SLANGC`, `CATA_SLANG_ROOT`, then `PATH`. If Slang is not found and
`BUILD_SLANG=ON`, it downloads the pinned prebuilt Slang release into the build directory under
`_deps/slang/current`.

Expected output:

```text
Slang max probe OK: 99
```

`CATA_SLANG_PROBE_DXIL=ON` can be used later to check DXIL generation on systems with the required
compiler support. DXBC is intentionally not part of this migration path.
