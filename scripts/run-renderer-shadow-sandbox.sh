#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BGFX_BUILD_DIR="build-sdl3-bgfx-physx-imgui-sdl3audio"
DILIGENT_BUILD_DIR="build-sdl3-diligent-physx-imgui-sdl3audio"

DURATION_SEC="${1:-20}"
GROUND_TILES="${2:-1}"
GROUND_EXTENT="${3:-20}"
HARD_TIMEOUT_SEC="$((DURATION_SEC + 3))"

RUN_TS="$(date -u +%Y%m%dT%H%M%SZ)"
LOG_DIR="/tmp/renderer-shadow-sandbox-${RUN_TS}"
mkdir -p "${LOG_DIR}"

run_backend() {
  local backend="$1"
  local build_dir="$2"
  local trace_channels="$3"
  local backend_video_driver_var="SANDBOX_VIDEO_DRIVER_${backend^^}"
  local backend_video_driver="${!backend_video_driver_var:-${SANDBOX_VIDEO_DRIVER:-}}"
  local binary="${REPO_ROOT}/${build_dir}/src/engine/renderer_shadow_sandbox"
  local log_file="${LOG_DIR}/shadow-sandbox-${backend}.log"
  local -a base_args=(
    timeout "${HARD_TIMEOUT_SEC}s"
    "${binary}"
    --backend-render "${backend}"
    --duration-sec "${DURATION_SEC}"
    --ground-tiles "${GROUND_TILES}"
    --ground-extent "${GROUND_EXTENT}"
    --shadow-map-size 1024
    --shadow-pcf 2
    --shadow-strength 0.85
    --trace "${trace_channels}"
    --verbose
  )
  local -a cmd=("${base_args[@]}")
  if [[ -n "${backend_video_driver}" ]]; then
    cmd+=(--video-driver "${backend_video_driver}")
  fi

  if [[ ! -x "${binary}" ]]; then
    echo "ERROR: missing executable ${binary}. Build first with ./bzbuild.py -c ${build_dir}" >&2
    exit 1
  fi

  printf '[shadow-sandbox] backend=%s command:' "${backend}"
  printf ' %q' "${cmd[@]}"
  printf '\n'

  set +e
  "${cmd[@]}" >"${log_file}" 2>&1
  local exit_code=$?
  set -e

  if [[ "${backend}" == "diligent" && ${exit_code} -ne 0 && -z "${backend_video_driver}" ]]; then
    if grep -q "Failed to create OS-specific surface" "${log_file}" &&
       grep -q "ERROR_INITIALIZATION_FAILED" "${log_file}"; then
      local retry_log_file="${LOG_DIR}/shadow-sandbox-${backend}-wayland-retry.log"
      local -a retry_cmd=("${base_args[@]}" --video-driver "wayland")
      echo "[shadow-sandbox] backend=${backend} retry reason=x11_swapchain_init_failed fallback=wayland"
      printf '[shadow-sandbox] backend=%s retry command:' "${backend}"
      printf ' %q' "${retry_cmd[@]}"
      printf '\n'

      set +e
      "${retry_cmd[@]}" >"${retry_log_file}" 2>&1
      exit_code=$?
      set -e
      log_file="${retry_log_file}"
    fi
  fi

  echo "[shadow-sandbox] backend=${backend} exit_code=${exit_code} log=${log_file}"
  echo "[shadow-sandbox] backend=${backend} diagnostic tail:"
  grep -E "\\[sandbox\\]" "${log_file}" | tail -n 3 || true
}

cd "${REPO_ROOT}"

echo "[shadow-sandbox] repo_root=${REPO_ROOT}"
echo "[shadow-sandbox] log_dir=${LOG_DIR}"
echo "[shadow-sandbox] duration_sec=${DURATION_SEC} hard_timeout_sec=${HARD_TIMEOUT_SEC} ground_tiles=${GROUND_TILES} ground_extent=${GROUND_EXTENT}"
echo "[shadow-sandbox] video_driver_default=${SANDBOX_VIDEO_DRIVER:-<auto>} bgfx=${SANDBOX_VIDEO_DRIVER_BGFX:-<default>} diligent=${SANDBOX_VIDEO_DRIVER_DILIGENT:-<default>}"

run_backend "bgfx" "${BGFX_BUILD_DIR}" "render.system,render.bgfx"
run_backend "diligent" "${DILIGENT_BUILD_DIR}" "render.system,render.diligent"

echo "[shadow-sandbox] summary log_dir=${LOG_DIR}"
