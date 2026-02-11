#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BGFX_BUILD_DIR="build-sdl3-bgfx-physx-imgui-sdl3audio"
DILIGENT_BUILD_DIR="build-sdl3-diligent-physx-imgui-sdl3audio"

RUN_TS="$(date -u +%Y%m%dT%H%M%SZ)"
LOG_DIR="/tmp/vq2-renderer-evidence-${RUN_TS}"
CONFIG_FILE="${LOG_DIR}/vq2-user-config.json"

mkdir -p "${LOG_DIR}"

cat > "${CONFIG_FILE}" <<EOF
{
  "DataDir": "${REPO_ROOT}/data"
}
EOF

find_backend_related_pids() {
  local binary="$1"
  local backend="$2"
  ps -eo pid=,args= | awk -v bin="${binary}" -v backend="${backend}" '
    index($0, bin " --backend-render " backend " --backend-ui imgui") > 0 { print $1 }
  '
}

ensure_no_lingering_children() {
  local binary="$1"
  local backend="$2"
  local -a pids=()

  mapfile -t pids < <(find_backend_related_pids "${binary}" "${backend}")
  if [[ ${#pids[@]} -eq 0 ]]; then
    printf '%s\n' "none"
    return 0
  fi

  kill -TERM "${pids[@]}" 2>/dev/null || true
  sleep 1
  mapfile -t pids < <(find_backend_related_pids "${binary}" "${backend}")
  if [[ ${#pids[@]} -eq 0 ]]; then
    printf '%s\n' "cleaned_term"
    return 0
  fi

  kill -KILL "${pids[@]}" 2>/dev/null || true
  sleep 1
  mapfile -t pids < <(find_backend_related_pids "${binary}" "${backend}")
  if [[ ${#pids[@]} -eq 0 ]]; then
    printf '%s\n' "cleaned_kill"
    return 0
  fi

  echo "ERROR: lingering processes remain for backend ${backend}: ${pids[*]}" >&2
  return 1
}

run_backend_capture() {
  local backend="$1"
  local trace_channels="$2"
  local build_dir="$3"
  local binary="${REPO_ROOT}/${build_dir}/bz3"
  local log_file="${LOG_DIR}/vq2-${backend}-${RUN_TS}.log"
  local -a cmd=(
    timeout 20s
    "${binary}"
    --backend-render "${backend}"
    --backend-ui imgui
    --strict-config=true
    --config "${CONFIG_FILE}"
    -v
    -t "${trace_channels}"
  )

  if [[ ! -x "${binary}" ]]; then
    echo "ERROR: missing executable ${binary}. Run ./bzbuild.py -c ${build_dir} first." >&2
    exit 1
  fi

  printf '[vq2] backend=%s command:' "${backend}"
  printf ' %q' "${cmd[@]}"
  printf '\n'
  echo "[vq2] backend=${backend} log=${log_file}"

  set +e
  "${cmd[@]}" >"${log_file}" 2>&1
  local exit_code=$?
  set -e
  echo "[vq2] backend=${backend} exit_code=${exit_code}"

  local cleanup_status
  cleanup_status="$(ensure_no_lingering_children "${binary}" "${backend}")"
  echo "[vq2] backend=${backend} child_process_status=${cleanup_status}"

  BACKEND_EXIT_CODE="${exit_code}"
  BACKEND_CLEANUP_STATUS="${cleanup_status}"
  BACKEND_LOG_FILE="${log_file}"
}

cd "${REPO_ROOT}"

echo "[vq2] repo_root=${REPO_ROOT}"
echo "[vq2] log_dir=${LOG_DIR}"
echo "[vq2] config_file=${CONFIG_FILE}"
echo "[vq2] build_dirs=${BGFX_BUILD_DIR},${DILIGENT_BUILD_DIR}"

BACKEND_EXIT_CODE=0
BACKEND_CLEANUP_STATUS="none"
BACKEND_LOG_FILE=""

run_backend_capture "bgfx" "engine.app,render.mesh,render.bgfx" "${BGFX_BUILD_DIR}"
BGFX_EXIT_CODE="${BACKEND_EXIT_CODE}"
BGFX_CHILD_STATUS="${BACKEND_CLEANUP_STATUS}"
BGFX_LOG_FILE="${BACKEND_LOG_FILE}"

run_backend_capture "diligent" "engine.app,render.mesh,render.diligent" "${DILIGENT_BUILD_DIR}"
DILIGENT_EXIT_CODE="${BACKEND_EXIT_CODE}"
DILIGENT_CHILD_STATUS="${BACKEND_CLEANUP_STATUS}"
DILIGENT_LOG_FILE="${BACKEND_LOG_FILE}"

echo "[vq2] summary bgfx_exit_code=${BGFX_EXIT_CODE}"
echo "[vq2] summary diligent_exit_code=${DILIGENT_EXIT_CODE}"
echo "[vq2] summary bgfx_child_process_status=${BGFX_CHILD_STATUS}"
echo "[vq2] summary diligent_child_process_status=${DILIGENT_CHILD_STATUS}"
echo "[vq2] summary bgfx_log=${BGFX_LOG_FILE}"
echo "[vq2] summary diligent_log=${DILIGENT_LOG_FILE}"
echo "[vq2] summary log_dir=${LOG_DIR}"
