#!/usr/bin/env bash
# Tests for update-registry-mods.sh with mocked git/nix. No network.
#
# Usage: test-update-registry-mods.sh <updater-under-test>
#
# Each case builds a temp dir with mock git/nix on PATH plus a fixture
# lockfile (data/mods/external.json) and fixture pins
# (nix/registry-mods.nix), runs the updater, and asserts exit codes,
# file contents, and which fetches ran. Cases: unchanged-skip,
# hash-update, check-mode-clean, fetch-failure-restore,
# missing-id-insert, malformed-output, registry-update.
set -u

UPDATER="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
PASS=0
FAIL=0
TMPDIRS=""

ok() {
  PASS=$((PASS + 1))
  echo "ok: $1"
}

bad() {
  FAIL=$((FAIL + 1))
  echo "NOT OK: $1"
  [ -n "${2-}" ] && printf '%s\n' "$2" | head -20
}

cleanup() {
  for d in $TMPDIRS; do rm -rf "$d"; done
}
trap cleanup EXIT

# Valid SRI hashes built programmatically (43 base64 chars + "=").
HASH_A="sha256-$(printf 'A%.0s' $(seq 42))B="
HASH_B="sha256-$(printf 'C%.0s' $(seq 42))D="
HASH_C="sha256-$(printf 'E%.0s' $(seq 42))F="
HASH_D="sha256-$(printf 'G%.0s' $(seq 42))H="
HASH_REG_OLD="sha256-$(printf '0%.0s' $(seq 42))1="
HASH_REG_NEW="sha256-$(printf '2%.0s' $(seq 42))3="

URL_A="https://example.com/mod-a.zip"
URL_B="https://example.com/mod-b.zip"
URL_C="https://example.com/mod-c.zip"
REGISTRY_URL="https://mods.cataclysmbn.org/generated/mods.json"

# setup <id:url ...>: fresh fixture dir in $FIX, mocks on PATH.
# Always writes a two-entry lockfile (mod-a, mod-b) plus pins; cases
# extend it afterwards.
setup() {
  FIX="$(mktemp -d)"
  TMPDIRS="$TMPDIRS $FIX"
  mkdir -p "$FIX/bin" "$FIX/data/mods" "$FIX/nix"
  cat > "$FIX/bin/git" <<EOF
#!/bin/sh
if [ "\$1 \$2" = "rev-parse --show-toplevel" ]; then
  echo "$FIX"
else
  exit 1
fi
EOF
  # Mock nix: last arg is the url; behavior from $MOCK_MAP lines
  # "<url> <hash|FAIL|GARBAGE|BADHASH>". Every call appends its url to
  # $MOCK_CALL_LOG for fetch-count assertions. POSIX sh: the sandbox
  # has no /usr/bin/env, so /bin/sh plus no bashisms.
  cat > "$FIX/bin/nix" <<'EOF'
#!/bin/sh
eval "url=\${$#}"
echo "$url" >> "$MOCK_CALL_LOG"
resp="$(awk -v u="$url" '$1 == u { print $2; exit }' "$MOCK_MAP")"
case "$resp" in
  "" | FAIL) exit 1 ;;
  GARBAGE) echo "this is not json" ;;
  BADHASH) echo '{"hash": "definitely-not-a-hash"}' ;;
  *) printf '{"hash": "%s"}\n' "$resp" ;;
esac
EOF
  chmod +x "$FIX/bin/git" "$FIX/bin/nix"
  export PATH="$FIX/bin:$PATH"
  export MOCK_MAP="$FIX/map.txt"
  export MOCK_CALL_LOG="$FIX/calls.txt"
  : > "$MOCK_CALL_LOG"
  cat > "$FIX/data/mods/external.json" <<EOF
[
  { "id": "mod-a", "version": "1.0", "url": "$URL_A" },
  { "id": "mod-b", "version": "2.0", "url": "$URL_B" }
]
EOF
  cat > "$FIX/nix/registry-mods.nix" <<EOF
{
  registryHash = "$HASH_REG_OLD";
  hashes = {
    mod-a = "$HASH_A";
    mod-b = "$HASH_B";
  };
}
EOF
  cp "$FIX/nix/registry-mods.nix" "$FIX/pins.before"
  printf '%s %s\n' "$URL_A" "$HASH_A" > "$MOCK_MAP"
  printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
  printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
}

run_updater() {
  ( cd "$FIX" && bash "$UPDATER" ${1-} >"$FIX/out.txt" 2>&1 )
}

# 1: everything current -> untouched, exit 0.
setup
if run_updater; then
  if cmp -s "$FIX/pins.before" "$FIX/nix/registry-mods.nix" && grep -q "untouched" "$FIX/out.txt"; then
    ok "unchanged-skip leaves pins untouched"
  else
    bad "unchanged-skip changed pins or missed message" "$(cat "$FIX/out.txt")"
  fi
else
  bad "unchanged-skip exited nonzero" "$(cat "$FIX/out.txt")"
fi

# 2: one stale hash -> rewritten; shared urls fetched once.
setup
printf '%s %s\n' "$URL_A" "$HASH_C" > "$MOCK_MAP"
printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
if run_updater; then
  pin_a="$(grep -o 'mod-a = "[^"]*"' "$FIX/nix/registry-mods.nix" | cut -d'"' -f2)"
  pin_b="$(grep -o 'mod-b = "[^"]*"' "$FIX/nix/registry-mods.nix" | cut -d'"' -f2)"
  fetch_a="$(grep -c -F -x "$URL_A" "$MOCK_CALL_LOG")"
  if [ "$pin_a" = "$HASH_C" ] && [ "$pin_b" = "$HASH_B" ] && [ "$fetch_a" -eq 1 ]; then
    ok "hash-update rewrites only the stale pin"
  else
    bad "hash-update wrong pins (a=$pin_a b=$pin_b fetches=$fetch_a)" "$(cat "$FIX/nix/registry-mods.nix")"
  fi
else
  bad "hash-update exited nonzero" "$(cat "$FIX/out.txt")"
fi

# 3: --check with a stale pin -> reports, writes nothing, exit 0.
setup
printf '%s %s\n' "$URL_A" "$HASH_C" > "$MOCK_MAP"
printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
if run_updater "--check"; then
  if cmp -s "$FIX/pins.before" "$FIX/nix/registry-mods.nix" && grep -q "plan: update" "$FIX/out.txt"; then
    ok "check-mode reports without writing"
  else
    bad "check-mode wrote or stayed silent" "$(cat "$FIX/out.txt")"
  fi
else
  bad "check-mode exited nonzero" "$(cat "$FIX/out.txt")"
fi

# 4: prefetch failure mid-run -> backup restored, nonzero exit.
setup
printf '%s %s\n' "$URL_A" "$HASH_C" > "$MOCK_MAP"
printf '%s %s\n' "$URL_B" "FAIL" >> "$MOCK_MAP"
printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
if run_updater; then
  bad "fetch-failure exited zero" "$(cat "$FIX/out.txt")"
else
  if cmp -s "$FIX/pins.before" "$FIX/nix/registry-mods.nix"; then
    ok "fetch-failure restores pins from backup"
  else
    bad "fetch-failure left partial writes" "$(diff "$FIX/pins.before" "$FIX/nix/registry-mods.nix")"
  fi
fi

# 5: lockfile id with no pin -> inserted sorted with prefetched hash.
setup
cat > "$FIX/data/mods/external.json" <<EOF
[
  { "id": "mod-0", "version": "0.1", "url": "$URL_C" },
  { "id": "mod-a", "version": "1.0", "url": "$URL_A" },
  { "id": "mod-b", "version": "2.0", "url": "$URL_B" }
]
EOF
printf '%s %s\n' "$URL_A" "$HASH_A" > "$MOCK_MAP"
printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
printf '%s %s\n' "$URL_C" "$HASH_D" >> "$MOCK_MAP"
printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
if run_updater; then
  order="$(grep -o '^    mod-[a-z0-9-]*' "$FIX/nix/registry-mods.nix" | tr '\n' ' ')"
  pin_new="$(grep -o 'mod-0 = "[^"]*"' "$FIX/nix/registry-mods.nix" | cut -d'"' -f2)"
  if [ "$order" = "    mod-0     mod-a     mod-b " ] && [ "$pin_new" = "$HASH_D" ]; then
    ok "missing-id inserted sorted with prefetched hash"
  else
    bad "missing-id wrong order/content ($order $pin_new)" "$(cat "$FIX/nix/registry-mods.nix")"
  fi
else
  bad "missing-id exited nonzero" "$(cat "$FIX/out.txt")"
fi

# 6: malformed prefetch output -> hard error, pins untouched. Both
# failure shapes: unparsable output and valid JSON with a bad hash.
for mode in GARBAGE BADHASH; do
  setup
  printf '%s %s\n' "$URL_A" "$mode" > "$MOCK_MAP"
  printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
  printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_OLD" >> "$MOCK_MAP"
  if run_updater; then
    bad "malformed-output($mode) exited zero" "$(cat "$FIX/out.txt")"
  else
    if cmp -s "$FIX/pins.before" "$FIX/nix/registry-mods.nix"; then
      ok "malformed-output($mode) errors without writing"
    else
      bad "malformed-output($mode) left partial writes" "$(cat "$FIX/out.txt")"
    fi
  fi
done

# 7: registry snapshot moved -> registryHash updated, entry pins kept.
setup
printf '%s %s\n' "$URL_A" "$HASH_A" > "$MOCK_MAP"
printf '%s %s\n' "$URL_B" "$HASH_B" >> "$MOCK_MAP"
printf '%s %s\n' "$REGISTRY_URL" "$HASH_REG_NEW" >> "$MOCK_MAP"
if run_updater; then
  reg="$(grep -m1 -o 'registryHash = "[^"]*"' "$FIX/nix/registry-mods.nix" | cut -d'"' -f2)"
  pin_a="$(grep -o 'mod-a = "[^"]*"' "$FIX/nix/registry-mods.nix" | cut -d'"' -f2)"
  if [ "$reg" = "$HASH_REG_NEW" ] && [ "$pin_a" = "$HASH_A" ]; then
    ok "registry-update swaps only the snapshot pin"
  else
    bad "registry-update wrong pins ($reg $pin_a)" "$(cat "$FIX/nix/registry-mods.nix")"
  fi
else
  bad "registry-update exited nonzero" "$(cat "$FIX/out.txt")"
fi

echo "---"
echo "pass: $PASS fail: $FAIL"
[ "$FAIL" -eq 0 ]
