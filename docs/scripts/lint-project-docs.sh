#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$DOCS_DIR/.." && pwd)"
PROJECT_DIR="$DOCS_DIR/projects"

fail=0

check_contains() {
  local file="$1"
  local pattern="$2"
  local label="$3"
  if ! rg -q -- "$pattern" "$file"; then
    echo "[FAIL] $file missing: $label"
    fail=1
  fi
}

echo "[lint] Checking required project sections"
while IFS= read -r file; do
  check_contains "$file" '^## Project Snapshot$' '## Project Snapshot'
  check_contains "$file" '^- Current owner:' 'Current owner field'
  check_contains "$file" '^- Status:' 'Status field'
  check_contains "$file" '^- Immediate next task:' 'Immediate next task field'
  check_contains "$file" '^- Validation gate:' 'Validation gate field'
done < <(find "$PROJECT_DIR" -maxdepth 1 -type f -name '*.md' \
  ! -name 'README.md' \
  ! -name 'AGENTS.md' \
  ! -name 'PROJECT_TEMPLATE.md' \
  ! -name 'ASSIGNMENTS.md' | sort)

echo "[lint] Checking assignment board coverage"
project_count=$(find "$PROJECT_DIR" -maxdepth 1 -type f -name '*.md' \
  ! -name 'README.md' \
  ! -name 'AGENTS.md' \
  ! -name 'PROJECT_TEMPLATE.md' \
  ! -name 'ASSIGNMENTS.md' | wc -l)
assignment_rows=$(rg -c '^\| `[^`]+\.md` \|' "$PROJECT_DIR/ASSIGNMENTS.md" || true)
if [[ "$assignment_rows" -ne "$project_count" ]]; then
  echo "[FAIL] $PROJECT_DIR/ASSIGNMENTS.md rows ($assignment_rows) != project doc count ($project_count)"
  fail=1
fi

retired_patterns=(
  'docs/projects/content-mount-abstraction-playbook.md'
  'docs/projects/core-engine-infrastructure-playbook.md'
  'docs/projects/engine-backend-testing-playbook.md'
  'docs/projects/netcode-responsiveness-playbook.md'
  'docs/projects/server-testing-playbook.md'
  'docs/projects/ui-integration-playbook.md'
  'docs/projects/content_mount_track.md'
  'docs/projects/gameplay_netcode_track.md'
  'docs/projects/physics_backend_track.md'
  'docs/projects/renderer_parity_track.md'
  'docs/projects/server_network_track.md'
  'docs/projects/testing_ci_docs_track.md'
  'docs/projects/ui_overlay_track.md'
)

echo "[lint] Checking for retired path references in active docs"
for pattern in "${retired_patterns[@]}"; do
  if rg -n \
    --glob 'AGENTS.md' \
    --glob 'docs/*.md' \
    --glob 'docs/projects/*.md' \
    --glob '!docs/archive/**' \
    -- "$pattern" "$ROOT_DIR" >/tmp/lint_match.txt 2>/dev/null; then
    echo "[FAIL] Found retired reference: $pattern"
    cat /tmp/lint_match.txt
    fail=1
  fi
done
rm -f /tmp/lint_match.txt

if [[ "$fail" -ne 0 ]]; then
  echo "[lint] FAILED"
  exit 1
fi

echo "[lint] OK"
