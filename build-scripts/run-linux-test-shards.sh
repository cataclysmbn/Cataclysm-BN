#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat >&2 <<'EOF'
Usage: build-scripts/run-linux-test-shards.sh [OPTIONS] [TEST_BIN -- [TEST_OPTS...]]

Options:
  --mode MODE             auto, file-tags, tiles, or legacy (default: auto)
  --jobs N|auto           concurrent shard jobs (default: auto, detected CPU count)
  --slow-shards N         generated slow file-tag shards (default: 4)
  --non-slow-shards N|auto
                          generated non-slow file-tag shards (default: auto, clamped jobs*2)
  --dry-run               print shard filters without running tests

Defaults:
  TEST_BIN                ./out/build/linux-full/tests/cata_test-tiles
  TEST_OPTS               --min-duration 20 --use-colour no --rng-seed 1
                          --error-format=github-action --gpu-backend=software

Environment:
  CATA_TEST_SHARD_DIR          shard file directory
  CATA_TEST_USER_DIR_PREFIX    test user-dir prefix (default: test_user_dir)
EOF
}

detect_cpu_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif getconf _NPROCESSORS_ONLN >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN
    else
        printf '%s\n' 4
    fi
}

validate_positive_int() {
    local name=$1
    local value=$2
    if ! [[ "$value" =~ ^[1-9][0-9]*$ ]]; then
        printf '%s must be a positive integer: %s\n' "$name" "$value" >&2
        exit 2
    fi
}

mode=auto
parallel_jobs=auto
slow_shards=4
non_slow_shards=auto
dry_run=0
default_test_bin=./out/build/linux-full/tests/cata_test-tiles
test_bin=

while [ $# -gt 0 ]; do
    case "$1" in
        --mode)
            mode="$2"
            shift 2
            ;;
        --jobs)
            parallel_jobs="$2"
            shift 2
            ;;
        --slow-shards)
            slow_shards="$2"
            shift 2
            ;;
        --non-slow-shards)
            non_slow_shards="$2"
            shift 2
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            printf 'Unknown option: %s\n' "$1" >&2
            usage
            exit 2
            ;;
        *)
            test_bin="$1"
            shift
            if [ "${1:-}" = "--" ]; then
                shift
            fi
            break
            ;;
    esac
done

test_opts=( "$@" )

if [ -z "$test_bin" ]; then
    test_bin="$default_test_bin"
fi
if [ "${#test_opts[@]}" -eq 0 ]; then
    test_opts=( --min-duration 20 --use-colour no --rng-seed 1 --error-format=github-action --gpu-backend=software )
fi

if [ ! -x "$test_bin" ]; then
    printf 'Missing executable test binary: %s\n' "$test_bin" >&2
    exit 2
fi

if [ "$parallel_jobs" = "auto" ]; then
    parallel_jobs=$(detect_cpu_count)
fi
validate_positive_int "--jobs" "$parallel_jobs"
validate_positive_int "--slow-shards" "$slow_shards"
if [ "$non_slow_shards" = "auto" ]; then
    non_slow_shards=$(( parallel_jobs * 2 ))
    if [ "$non_slow_shards" -lt 6 ]; then
        non_slow_shards=6
    elif [ "$non_slow_shards" -gt 16 ]; then
        non_slow_shards=16
    fi
fi
validate_positive_int "--non-slow-shards" "$non_slow_shards"

case "$mode" in
    auto|file-tags|tiles)
        mode=file-tags
        ;;
    legacy)
        ;;
    *)
        printf 'Unknown shard mode: %s\n' "$mode" >&2
        exit 2
        ;;
esac

if [ -n "${CATA_TEST_SHARD_DIR:-}" ]; then
    shard_dir="$CATA_TEST_SHARD_DIR"
    rm -rf "$shard_dir"
    mkdir -p "$shard_dir"
else
    shard_dir=$(mktemp -d "${RUNNER_TEMP:-/tmp}/cata-test-shards.XXXXXX")
    trap 'rm -rf "$shard_dir"' EXIT
fi
user_dir_prefix="${CATA_TEST_USER_DIR_PREFIX:-test_user_dir}"

if [ "$mode" = "file-tags" ]; then
    case " ${test_opts[*]} " in
        *' --filenames-as-tags '*) ;;
        *) test_opts=( --filenames-as-tags "${test_opts[@]}" ) ;;
    esac

    printf '%s\n' "vehicle_rail_movement_basic" > "$shard_dir/00-vehicle-rails-basic"
    printf '%s\n' "vehicle_rail_movement_fork" > "$shard_dir/09-vehicle-rails-fork"
    printf '%s\n' "vehicle_rail_movement_shifting" > "$shard_dir/19-vehicle-rails-shifting"
    printf '%s\n' "vehicle_rail_movement_derailed,vehicle_rail_movement_ramp" > "$shard_dir/29-vehicle-rails-other"
    for shard in $(seq 1 "$non_slow_shards"); do
        printf -v shard_name '%02d-non-slow' "$shard"
        : > "$shard_dir/$shard_name"
    done
    printf '%s\n' "[#vision_test] ~[.]" "[#shadowcasting_test] ~[.]" "[#zlevel_visibility_cache_test] ~[.]" > "$shard_dir/20-visibility"
    printf '%s\n' "[#vehicle_efficiency_test] ~[.]" > "$shard_dir/21-vehicle-efficiency"
    printf '%s\n' "starting_items" > "$shard_dir/22-starting-items"

    for shard in $(seq 1 "$slow_shards"); do
        : > "$shard_dir/3$shard-slow"
    done

    discovery_compute_accel="${CATA_TEST_COMPUTE_ACCELERATION:-cpu}"
    CATA_TEST_COMPUTE_ACCELERATION="$discovery_compute_accel" \
        "$test_bin" "${test_opts[@]}" --user-dir="$shard_dir/discovery" --list-tags \
        > "$shard_dir/test-tags" || true
    CATA_TEST_COMPUTE_ACCELERATION="$discovery_compute_accel" \
        "$test_bin" "${test_opts[@]}" --user-dir="$shard_dir/slow-discovery" --list-tags "[slow] ~starting_items" \
        > "$shard_dir/slow-tags" || true

    slow_index=0
    while IFS= read -r test_file_tag; do
        shard=$(( slow_index % slow_shards + 1 ))
        printf '[slow] ~starting_items %s\n' "$test_file_tag" >> "$shard_dir/3$shard-slow"
        slow_index=$(( slow_index + 1 ))
    done < <(
        awk '{
          for( i = 1; i <= NF; ++i ) {
            if( $i ~ /^\[#.*_test\]$/ ) {
              print $i
            }
          }
        }' "$shard_dir/slow-tags" | sort -u
    )
    if [ "$slow_index" = "0" ]; then
        printf '%s\n' "[slow] ~starting_items" > "$shard_dir/31-slow"
    fi

    test_index=0
    while IFS= read -r test_file_tag; do
        shard=$(( test_index % non_slow_shards + 1 ))
        printf -v shard_name '%02d-non-slow' "$shard"
        printf '~[slow] ~[.] %s\n' "$test_file_tag" >> "$shard_dir/$shard_name"
        test_index=$(( test_index + 1 ))
    done < <(
        awk '{
          for( i = 1; i <= NF; ++i ) {
            if( $i ~ /^\[#.*_test\]$/ && $i !~ /^\[#(vehicle_(efficiency|rails)|vision|shadowcasting|zlevel_visibility_cache)_test\]$/ ) {
              print $i
            }
          }
        }' "$shard_dir/test-tags" | sort -u
    )
else
    printf '%s\n' "[slow] ~starting_items" > "$shard_dir/00-slow"
    printf '%s\n' "~[slow] ~[.],starting_items" > "$shard_dir/01-non-slow"
fi

mapfile -t shard_files < <(find "$shard_dir" -maxdepth 1 -type f -size +0c -name '[0-9]*' | sort)

if [ "$dry_run" = "1" ]; then
    for shard_file in "${shard_files[@]}"; do
        printf '%s: %s\n' "$(basename "$shard_file")" "$(paste -sd, "$shard_file")"
    done
    exit 0
fi

shard_runner="$shard_dir/run-shard.sh"
cat > "$shard_runner" <<'EOF'
#!/usr/bin/env bash
set -u

shard_file=$1
shard=$(basename "$shard_file")
filter=$(paste -sd, "$shard_file")
user_dir="$USER_DIR_PREFIX"_"$shard"
if [ -n "${CATA_TEST_COMPUTE_ACCELERATION:-}" ]; then
    shard_compute_accel="$CATA_TEST_COMPUTE_ACCELERATION"
elif [ "$shard" = "20-visibility" ]; then
    shard_compute_accel="${CATA_TEST_VISIBILITY_COMPUTE_ACCELERATION:-gpu_software}"
else
    shard_compute_accel=cpu
fi
start=$(date +%s)
printf 'Starting shard %s with %s compute\n' "$shard" "$shard_compute_accel"
CATA_TEST_COMPUTE_ACCELERATION="$shard_compute_accel" "$TEST_BIN" $TEST_OPTS --user-dir="$user_dir" "$filter"
status=$?
end=$(date +%s)
printf 'Finished shard %s in %ss with status %s\n' "$shard" "$(( end - start ))" "$status"
exit "$status"
EOF
chmod +x "$shard_runner"

export TEST_BIN="$test_bin"
export TEST_OPTS="${test_opts[*]}"
export USER_DIR_PREFIX="$user_dir_prefix"
if command -v parallel >/dev/null 2>&1; then
    parallel --jobs "$parallel_jobs" --verbose --linebuffer --halt soon,fail=1 \
        "$shard_runner" {} ::: "${shard_files[@]}"
else
    printf '%s\0' "${shard_files[@]}" | xargs -0 -n 1 -P "$parallel_jobs" "$shard_runner"
fi
