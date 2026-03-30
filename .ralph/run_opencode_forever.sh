#!/usr/bin/env bash
set -euo pipefail

repo_root="/run/media/home/scarf/repo/cata/Cataclysm-BN-worktrees/feat-procgen-food"
prompt_file="$repo_root/.ralph/procgen_forever_prompt.md"

prompt="$({ python - <<'PY'
from pathlib import Path
print(Path("/run/media/home/scarf/repo/cata/Cataclysm-BN-worktrees/feat-procgen-food/.ralph/procgen_forever_prompt.md").read_text())
PY
} )"

exec opencode run \
  --dir "$repo_root" \
  --model openai/gpt-5.4 \
  --variant high \
  "$prompt"
