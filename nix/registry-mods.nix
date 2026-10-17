# Bundle the mod-registry content release builds ship (see
# build-scripts/bundle-registry-mods.ts) as a pure derivation.
#
# The script joins two sources: the lockfile data/mods/external.json
# (id/version/url/extract_path) and the registry metadata at
# https://mods.cataclysmbn.org/generated/mods.json (display name,
# package_type, homepage, license). This derivation mirrors that join at
# eval time: urls and versions come from the lockfile in `src`, registry
# fields from a pinned copy of mods.json, and each archive from a pinned
# fetchurl. The build then replicates the script's extract/verify
# semantics (lock extract_path wins, single-root fallback, marker-file
# verification, credits file).
#
# Notes for maintainers:
#   * Refreshing: run `nix run .#update-registry-mods` (or `--check` to
#     preview). It re-prefetches every lockfile url plus the registry
#     snapshot and rewrites the pins below. Branch archives are moving
#     targets, so any upstream commit breaks its hash until refreshed.
#   * Only otopack_bn_mk_2 is a soundpack; everything else lands in
#     mods/. If the registry ever marks another entry as a soundpack,
#     the routing below picks it up with no other changes.
{
  lib,
  stdenv,
  fetchurl,
  unzip,
  # Repo checkout, so data/mods/external.json (the lockfile) always
  # tracks the working tree instead of being pinned separately.
  src,
}:

let
  inherit (lib) escapeShellArg;

  lockEntries = builtins.fromJSON (builtins.readFile (src + "/data/mods/external.json"));

  # Pinned registry snapshot. The live file feeds display names, package
  # types, homepages, and licenses; pinning keeps the build pure and the
  # credits reproducible. Rewritten by nix/update-registry-mods.sh.
  registryHash = "sha256-OqctInshO0cbQeZd86+cvzFbfMiY+sOUweB7qvyC6To=";
  registryEntries = builtins.fromJSON (
    builtins.readFile (fetchurl {
      url = "https://mods.cataclysmbn.org/generated/mods.json";
      hash = registryHash;
    })
  );
  registryById = lib.listToAttrs (map (entry: lib.nameValuePair entry.id entry) registryEntries);

  # Archive hashes, one per lockfile id (shared urls repeat the hash;
  # identical fixed-output fetches dedupe to one store path). Rewritten
  # by nix/update-registry-mods.sh; keep one entry per line, sorted.
  hashes = {
    cata = "sha256-sDw3zAy+lmHz22lRH9dqbNaAslSF1zXuViQhTDEzAao=";
    cbn_sky_island = "sha256-NxkjerpoAAcANboi5YAPKCCPAf+xz+iPdM8pIEejMk0=";
    jake_s_squirrel_mod = "sha256-Kzu32aTdBZjZMc2xU3Y1cTB6vgW2mAoiM3+QLma/jQ0=";
    medieval_mod_reborn = "sha256-MN9vbtfeEqy0ToXzoYR+EzKl6vd+WNpfUqb2VQ8FEMU=";
    mst_extra = "sha256-lqLfoLvE88m3EHk2U8DQYeRiSFkK83YcCOXgOFWYHxg=";
    ny_transformers = "sha256-Kzu32aTdBZjZMc2xU3Y1cTB6vgW2mAoiM3+QLma/jQ0=";
    otopack_bn_mk_2 = "sha256-OOh2v22FLFTsOaE2q5LgIaj4tb80pV9Xbtdu1BelQDY=";
    overhaul = "sha256-n1F5lDdWuqMHq+D2K6evzGxVjXYOrzJitZE1Kc92kns=";
    overhaul_additions = "sha256-n1F5lDdWuqMHq+D2K6evzGxVjXYOrzJitZE1Kc92kns=";
  };

  # Join one lockfile entry with its registry metadata, mirroring
  # selectEntries/targetDir: the lock url and version win, the lock
  # extract_path wins over the registry one, and the target dir is the
  # last extract_path component (or the id when there is no path).
  entries = map (
    lock:
    let
      registry =
        registryById.${lock.id}
          or (throw "nix/registry-mods.nix: '${lock.id}' is not present in the mod registry");
      # The lockfile uses 0.0.0 as a placeholder for branch-tracking
      # mods the registry auto-versions, so those mismatches are
      # permanent by convention. Warn only when a real pinned version
      # disagrees with the registry: that is lockfile drift worth
      # seeing, and the eval-time warning would otherwise be all noise.
      checked =
        if registry.version != lock.version && lock.version != "0.0.0" then
          lib.warn "${lock.id} version mismatch: lockfile has ${lock.version}, registry has ${registry.version}; using locked version" registry
        else
          registry;
      extractPath = lock.extract_path or checked.source.extract_path or null;
      packageType = checked.package_type or "mod";
    in
    {
      inherit (lock) id url;
      archive = fetchurl {
        inherit (lock) url;
        hash =
          hashes.${lock.id}
            or (throw "nix/registry-mods.nix: no pinned hash for locked mod '${lock.id}'; run nix run .#update-registry-mods");
      };
      inherit extractPath packageType;
      target = if extractPath == null then lock.id else lib.last (lib.splitString "/" extractPath);
      destSubdir = if packageType == "soundpack" then "sound" else "mods";
      display_name = checked.display_name;
      homepage = checked.homepage or "unknown";
      license = checked.license or "unknown";
    }
  ) lockEntries;

  bundleCall = entry: ''
    bundle_entry \
      ${escapeShellArg entry.id} \
      ${entry.archive} \
      ${escapeShellArg (entry.extractPath or "")} \
      ${escapeShellArg entry.target} \
      ${escapeShellArg entry.destSubdir} \
      ${escapeShellArg entry.display_name} \
      ${escapeShellArg entry.homepage} \
      ${escapeShellArg entry.license} \
      ${escapeShellArg entry.url}
  '';
in
stdenv.mkDerivation {
  pname = "cbn-registry-mods";
  version = "external-json";

  dontUnpack = true;
  nativeBuildInputs = [ unzip ];
  strictDeps = true;

  buildPhase = ''
    runHook preBuild

    bundle_entry() {
      local id="$1" archive="$2" extract="$3" target="$4" subdir="$5"
      local display="$6" homepage="$7" license="$8" url="$9"
      echo "$id bundled as $subdir/$target"

      local tmp
      tmp=$(mktemp -d)
      unzip -q "$archive" -d "$tmp"

      # Mirror findExtractDir: trailing-component match on the extract
      # path, single-root fallback when the entry has no path.
      local src_dir=""
      if [ -n "$extract" ]; then
        src_dir=$(find "$tmp" -type d -path "*/$extract" -print -quit)
        if [ -z "$src_dir" ]; then
          echo "$id: archive did not contain $extract" >&2
          exit 1
        fi
      else
        local roots
        roots=$(find "$tmp" -mindepth 1 -maxdepth 1 -type d)
        if [ "$(printf '%s\n' "$roots" | wc -l)" -ne 1 ]; then
          echo "$id: archive has multiple roots" >&2
          exit 1
        fi
        src_dir=$roots
      fi

      local dest="bundle-root/$subdir/$target"
      mkdir -p "$(dirname "$dest")"
      rm -rf "$dest"
      cp -r "$src_dir" "$dest"
      rm -rf "$tmp"

      # Mirror verify: mods carry modinfo.json, soundpacks carry
      # soundpack.txt or soundpack.json.
      if [ "$subdir" = "sound" ]; then
        if [ ! -f "$dest/soundpack.txt" ] && [ ! -f "$dest/soundpack.json" ]; then
          echo "$id: did not extract soundpack.txt or soundpack.json" >&2
          exit 1
        fi
      else
        if [ ! -f "$dest/modinfo.json" ]; then
          echo "$id: did not extract modinfo.json" >&2
          exit 1
        fi
      fi

      # Mirror writeCredits, one block per lockfile entry.
      {
        echo "$display ($id)"
        echo "Source: $url"
        echo "Homepage: $homepage"
        echo "License: $license"
        echo ""
      } >> bundle-root/mods/external-mods-CREDITS.txt
    }

    rm -rf bundle-root
    mkdir -p bundle-root/mods
    {
      echo "Bundled registry mods"
      echo "====================="
      echo ""
    } > bundle-root/mods/external-mods-CREDITS.txt

    ${lib.concatMapStringsSep "\n" bundleCall entries}

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp -r bundle-root/. $out/
    runHook postInstall
  '';

  meta = {
    description = "Cataclysm-BN registry mods and soundpack (data/mods/external.json bundle)";
    homepage = "https://mods.cataclysmbn.org";
    # Mixed upstream licensing across entries (mostly
    # ALL-RIGHTS-RESERVED, plus CC-BY-SA-4.0/3.0); see the
    # external-mods-CREDITS.txt in the output for per-entry terms.
    platforms = lib.platforms.all;
  };
}
