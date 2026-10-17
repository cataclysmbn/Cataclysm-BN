{
  description = ''
    Cataclysm: Bright Nights packaged with Nix. SDL3 tiles and ncurses
    builds plus the Catch2 test binary, built from this source with the
    repo's CMake setup. gfx/ ships pre-composed, so the game package
    already bundles every tileset, and nix/registry-mods.nix bundles
    the data/mods/external.json registry content (external mods plus
    the Otopack soundpack), like a release.
  '';

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    inputs@{ flake-parts, self, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      perSystem =
        { pkgs, ... }:
        let
          inherit (pkgs) lib;

          # No .git in flake sources, so the banner version comes from
          # the flake revision instead (package.nix answers CMake's git
          # calls with a stand-in script built from these values).
          version = "0.13.0+" + (self.shortRev or self.dirtyShortRev or "git");
          commitTime = self.lastModified or null;

          # Docs require clang >= 22; the default gcc stdenv is not it.
          clangStdenv = pkgs.llvmPackages_22.stdenv;

          # Registry mods + soundpack bundle, shared by every game
          # package (built once per system, not per variant).
          registryMods = pkgs.callPackage ./nix/registry-mods.nix {
            src = self.outPath;
          };

          mkCbn =
            attrs:
            pkgs.callPackage ./nix/package.nix (
              attrs
              // {
                inherit version registryMods commitTime;
                rev = self.rev or self.dirtyRev or "unknown";
                src = self.outPath;
                stdenv = clangStdenv;
              }
            );

          tiles = mkCbn {
            tiles = true;
            sound = true;
          };
          curses = mkCbn {
            tiles = false;
            sound = false;
          };
          # Test binary with tiles + sound, same flags CI uses.
          # Keeps the tileset and soundpack lists non-empty so game-state
          # init works inside tests.
          cata-test = mkCbn {
            tiles = true;
            sound = true;
            buildTests = true;
          };
        in
        {
          packages = {
            inherit
              tiles
              curses
              cata-test
              ;
            default = tiles;
          };

          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              git
              nixfmt
              cmake
              ninja
            ];

            shellHook = ''
              echo 'Cataclysm-BN nix shell. Try: nix build .#tiles' 1>&2
            '';
          };

          checks =
            let
              # The binaries read data from the store path baked in at
              # compile time (-DPREFIX), so they run from anywhere. The
              # test binary is the exception: it starts with an empty
              # base path, so its resource dirs resolve relative to cwd
              # (./data, ./gfx, ./lang/mo). Logs and saves go to cwd,
              # and non-[nogame] tests boot a full game state, hence
              # the temp dir.
              gameEnv = ''
                cd "$(mktemp -d)"
                export HOME="$PWD"
                export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
              '';
              # share/cataclysm-bn IS the data dir (raw/, gfx/, json/
              # live at its top level), so ./data points straight at it.
              # cata-test ships only the binary, so data comes from tiles.
              testEnv = ''
                root="${tiles}/share/cataclysm-bn"
                ln -s "$root" data
                ln -s "$root/gfx" gfx
                mkdir -p lang
                if [ -d "${tiles}/share/locale" ]; then
                  ln -s "${tiles}/share/locale" lang/mo
                fi
              '';
            in
            {
              # Smoke test: the calendar suite through the Catch2 binary.
              calendar-test = pkgs.runCommand "cbn-calendar-test" { } ''
                ${gameEnv}
                ${testEnv}
                # No GPU in the sandbox, not even software Vulkan, so run
                # compute tests on the CPU instead of failing device setup.
                export CATA_TEST_COMPUTE_ACCELERATION=cpu
                ${cata-test}/bin/cata_test-tiles "[calendar]"
                touch $out
              '';

              # Content gate: validates mod JSON the same way
              # build-scripts/lint-json.sh does for source trees.
              check-mods = pkgs.runCommand "cbn-check-mods" { } ''
                ${gameEnv}
                ${tiles}/bin/cataclysm-bn-tiles --check-mods
                touch $out
              '';

              # Updater tests: the pin-refresh script suite runs mocked
              # (no network).
              updater-tests =
                pkgs.runCommand "cbn-updater-tests"
                  {
                    nativeBuildInputs = with pkgs; [
                      bash
                      coreutils
                      gawk
                      gnugrep
                      gnused
                      jq
                    ];
                  }
                  ''
                    bash ${self + /nix/test-update-registry-mods.sh} ${self + /nix/update-registry-mods.sh}
                    touch $out
                  '';
            };

          apps.update-registry-mods =
            let
              updater = pkgs.writeShellApplication {
                name = "update-registry-mods";
                runtimeInputs = with pkgs; [
                  coreutils
                  gawk
                  git
                  gnugrep
                  gnused
                  jq
                  nix
                ];
                text = builtins.readFile ./nix/update-registry-mods.sh;
              };
            in
            {
              type = "app";
              program = lib.getExe updater;
              meta.description = "Refresh the nix/registry-mods.nix pins";
            };
        };
    };
}
