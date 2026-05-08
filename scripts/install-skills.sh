#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE_DIR="$REPO_ROOT/.codex/skills"

if [ ! -d "$SOURCE_DIR" ]; then
  echo "Skill source directory not found: $SOURCE_DIR" >&2
  exit 1
fi

if [ "${CODEX_HOME:-}" ]; then
  TARGET_ROOT="$CODEX_HOME"
else
  TARGET_ROOT="$HOME/.codex"
fi

TARGET_DIR="$TARGET_ROOT/skills"
mkdir -p "$TARGET_DIR"

install_skill() {
  skill_name=$1
  source_skill="$SOURCE_DIR/$skill_name"
  target_skill="$TARGET_DIR/$skill_name"

  if [ ! -f "$source_skill/SKILL.md" ]; then
    echo "Skipping $skill_name: missing SKILL.md" >&2
    return
  fi

  rm -rf "$target_skill"
  mkdir -p "$target_skill"

  if command -v rsync >/dev/null 2>&1; then
    rsync -a --exclude '__pycache__/' --exclude '*.pyc' "$source_skill/" "$target_skill/"
  else
    cp -R "$source_skill/." "$target_skill/"
    find "$target_skill" -name '__pycache__' -type d -prune -exec rm -rf {} \; 2>/dev/null || true
    find "$target_skill" -name '*.pyc' -type f -delete 2>/dev/null || true
  fi

  echo "Installed $skill_name -> $target_skill"
}

install_skill gh-create-issue-branch
install_skill gh-create-pr
install_skill gh-review-pr

echo "Done. Restart Codex if the skills are not visible yet."
