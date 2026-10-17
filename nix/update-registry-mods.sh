#!/usr/bin/env bash
# Refresh the registry-mod pins in nix/registry-mods.nix.
#
# Usage: update-registry-mods.sh [--check]
#   (no args)  prefetch every lockfile url plus the registry snapshot,
#              rewrite the hashes table and registryHash pin
#   --check    print current vs prefetched hashes without changing anything
#
# What is pinned and why: data/mods/external.json (the lockfile) lives in
# the repo and is read live, so new entries are detected, not skipped.
# What the updater owns is the hashes table plus registryHash in
# nix/registry-mods.nix. Branch archives are moving targets: any upstream
# commit changes the zip bytes, so the pin breaks until refreshed here.
#
# Cost model: prefetching only downloads archives into the store. Nothing
# here compiles any project code; the game rebuild happens on the next
# `nix build` as usual.
#
# Safety: phase 1 resolves every hash and computes the whole plan before
# writing anything. Phase 2 backs the file up, regenerates the hashes
# block deterministically (sorted, one entry per line) plus the single
# registryHash line, then re-reads everything to confirm. Any failure
# restores the backup. Single-value writes only (no rev+hash staging),
# so there is no placeholder state to repair, just backup and verify.
set -euo pipefail

REGISTRY_URL="https://mods.cataclysmbn.org/generated/mods.json"
NIX_FILE="nix/registry-mods.nix"
LOCK_FILE="data/mods/external.json"

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

cd "$(git rev-parse --show-toplevel)"
CHECK_ONLY=0
if [ "${1-}" = "--check" ]; then
  CHECK_ONLY=1
fi

is_sri_hash() {
  [[ "$1" =~ ^sha256-[A-Za-z0-9+/]{43}=$ ]]
}

current_registry_hash() {
  grep -m1 -o '^  registryHash = "sha256-[A-Za-z0-9+/=]*"' "$NIX_FILE" | cut -d'"' -f2
}

current_hash() {
  grep -m1 -o "^    $1 = \"sha256-[A-Za-z0-9+/=]*\"" "$NIX_FILE" | cut -d'"' -f2 || true
}

# Print the verified hash for a URL, or die. Never guesses: prefetch
# failures and malformed hashes are hard errors with the URL attached.
prefetch() {
  local url="$1" out hash
  out="$(nix store prefetch-file --json "$url")" \
    || die "prefetch failed for $url"
  hash="$(printf '%s\n' "$out" | jq -r '.hash // empty')" \
    || die "cannot parse prefetch output for $url"
  if ! is_sri_hash "$hash"; then
    die "malformed hash for $url: $hash"
  fi
  printf '%s\n' "$hash"
}

# Phase 1: resolve every hash and compute the plan. No writes, no edits.
mapfile -t ids < <(jq -r '.[].id' "$LOCK_FILE") \
  || die "cannot read mod ids from $LOCK_FILE"
[ "${#ids[@]}" -gt 0 ] || die "no entries in $LOCK_FILE"

url_of() {
  jq -r --arg id "$1" '.[] | select(.id == $id) | .url' "$LOCK_FILE"
}

declare -A newhash_of
declare -A action_of
for id in "${ids[@]}"; do
  url="$(url_of "$id")"
  [ -n "$url" ] || die "no url for locked mod $id"
  if [ -z "${newhash_of[$url]+x}" ]; then
    echo "fetch: $url"
    newhash_of[$url]="$(prefetch "$url")"
  fi
  cur="$(current_hash "$id")"
  new="${newhash_of[$url]}"
  if [ -z "$cur" ]; then
    action="missing"
  elif [ "$cur" = "$new" ]; then
    action="skip"
  else
    action="update"
  fi
  action_of[$id]="$action"
  echo "== $id"
  echo "current: ${cur:-<no pin>}"
  echo "fetched: $new"
  echo "plan: $action"
done

echo "fetch: $REGISTRY_URL"
new_registry="$(prefetch "$REGISTRY_URL")"
cur_registry="$(current_registry_hash)" \
  || die "cannot read registryHash from $NIX_FILE"
registry_action="update"
[ "$new_registry" = "$cur_registry" ] && registry_action="skip"
echo "== registry snapshot"
echo "current: $cur_registry"
echo "fetched: $new_registry"
echo "plan: $registry_action"

if [ "$CHECK_ONLY" -eq 1 ]; then
  echo "check only; $NIX_FILE untouched."
  exit 0
fi

all_skip=1
for id in "${ids[@]}"; do
  if [ "${action_of[$id]}" != "skip" ]; then
    all_skip=0
    break
  fi
done
[ "$registry_action" != "skip" ] && all_skip=0
if [ "$all_skip" -eq 1 ]; then
  echo "all sources current; $NIX_FILE untouched."
  exit 0
fi

# Phase 2: apply. Regenerate the hashes block whole (sorted, one entry
# per line) so updates and new-id inserts share one code path, then swap
# the single registryHash line. Backup guards everything; verification
# re-reads every planned pin afterwards.
[ "$(grep -c '^  registryHash = "sha256-[A-Za-z0-9+/=]*"' "$NIX_FILE")" -eq 1 ] \
  || die "expected exactly one registryHash pin in $NIX_FILE"
[ "$(grep -c '^  hashes = {$' "$NIX_FILE")" -eq 1 ] \
  || die "expected exactly one hashes block in $NIX_FILE"

backup="$(mktemp)"
cp "$NIX_FILE" "$backup"

block_tmp="$(mktemp)"
{
  echo "  hashes = {"
  for id in $(printf '%s\n' "${ids[@]}" | sort -u); do
    url="$(url_of "$id")"
    printf '    %s = "%s";\n' "$id" "${newhash_of[$url]}"
  done
  echo "  };"
} > "$block_tmp"

block_file="$block_tmp" awk '
  NR == FNR { block = block $0 "\n"; next }
  /^  hashes = \{$/ {
    if (replaced) { print "ERROR: duplicate hashes block" > "/dev/stderr"; exit 1 }
    replaced = 1
    printf "%s", block
    inblock = 1
    next
  }
  /^  \};$/ && inblock { inblock = 0; next }
  !inblock { print }
' "$block_tmp" "$NIX_FILE" > "$NIX_FILE.new"
mv "$NIX_FILE.new" "$NIX_FILE"

sed -i "s|^  registryHash = \"sha256-[A-Za-z0-9+/=]*\"|  registryHash = \"$new_registry\"|" "$NIX_FILE"

rm -f "$block_tmp"

restore() {
  cp "$backup" "$NIX_FILE"
  rm -f "$backup"
}

# Post-write verification: every planned pin reads back as decided.
for id in "${ids[@]}"; do
  url="$(url_of "$id")"
  now="$(current_hash "$id")" || {
    restore
    die "post-write readback failed for $id; restored from backup"
  }
  if [ "$now" != "${newhash_of[$url]}" ]; then
    restore
    die "post-write verification failed for $id; restored from backup"
  fi
done
now_registry="$(current_registry_hash)" || {
  restore
  die "post-write readback failed for registryHash; restored from backup"
}
if [ "$now_registry" != "$new_registry" ]; then
  restore
  die "post-write verification failed for registryHash; restored from backup"
fi
rm -f "$backup"

echo "done; run nix flake show, review the diff, and commit."
# Note: `nix flake check --no-build` cannot follow this; the registry
# metadata pin realizes at eval time, which --no-build forbids. `show`
# evaluates everything (including the derivation) without building.
