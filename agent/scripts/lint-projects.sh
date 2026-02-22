#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_DIR="${ROOT_DIR}/projects"
ASSIGNMENTS_FILE="${PROJECT_DIR}/ASSIGNMENTS.md"

usage() {
  cat <<'EOF'
Usage: ./scripts/lint-projects.sh

Validates overseer project tracking docs under 'projects':
- required sections/fields in project docs
- assignment board row count coverage
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

fail=0

if [[ ! -d "${PROJECT_DIR}" ]]; then
  echo "[lint] FAIL: missing directory: ${PROJECT_DIR}"
  exit 1
fi

if [[ ! -f "${ASSIGNMENTS_FILE}" ]]; then
  echo "[lint] FAIL: missing assignments file: ${ASSIGNMENTS_FILE}"
  exit 1
fi

if ! command -v rg >/dev/null 2>&1; then
  echo "[lint] FAIL: ripgrep (rg) is required"
  exit 1
fi

check_contains() {
  local file="$1"
  local pattern="$2"
  local label="$3"
  if ! rg -q -- "${pattern}" "${file}"; then
    echo "[lint] FAIL: ${file} missing ${label}"
    fail=1
  fi
}

list_project_docs() {
  find "${PROJECT_DIR}" -type f -name '*.md' \
    ! -name 'README.md' \
    ! -name 'AGENTS.md' \
    ! -name 'ASSIGNMENTS.md' \
    ! -path "${PROJECT_DIR}/ARCHIVE/*" | sort
}

echo "[lint] Checking required project sections"
while IFS= read -r file; do
  check_contains "${file}" '^## Project Snapshot$' 'section `## Project Snapshot`'
  check_contains "${file}" '^- Current owner:' 'field `Current owner`'
  check_contains "${file}" '^- Status:' 'field `Status`'
  check_contains "${file}" '^- Immediate next task:' 'field `Immediate next task`'
  check_contains "${file}" '^- Validation gate:' 'field `Validation gate`'
done < <(list_project_docs)

echo "[lint] Checking assignment board coverage"
project_count="$(list_project_docs | wc -l | tr -d '[:space:]')"
assignment_rows="$(rg -c '^\| `[^`]+\.md` \|' "${ASSIGNMENTS_FILE}" || true)"

if [[ "${assignment_rows}" -ne "${project_count}" ]]; then
  echo "[lint] FAIL: ${ASSIGNMENTS_FILE} rows (${assignment_rows}) != project doc count (${project_count})"
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "[lint] FAILED"
  exit 1
fi

echo "[lint] OK"
