#!/usr/bin/env bash
#
# Pin vendor/llama.cpp to a Kepler / sm_30 -compatible commit so that the
# project's vendored llama.cpp can be built with its CUDA backend on a
# CUDA 10.x toolchain (e.g. Quadro K5100M + CUDA 10.1).
#
# Background:
#   * llama.cpp dropped support for compute capability 3.0 (Kepler) in its
#     CUDA backend in late 2023. The last known-good tag for sm_30 is around
#     b1500 (early November 2023), which still includes the legacy DMMV
#     kernel path that doesn't rely on cc>=5.0 intrinsics.
#   * We don't track vendor/llama.cpp in git; users opt in by running this
#     script, which produces a detached-HEAD checkout of the pinned tag.
#
# Usage (from the repository root):
#     scripts/setup_llama_cpp_kepler.sh
#     cmake -S . -B build -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON
#     cmake --build build -j
#
# Environment overrides:
#     LLAMA_CPP_REMOTE   git URL to clone (default: upstream ggerganov/llama.cpp)
#     LLAMA_CPP_TAG      git ref to check out (default: b1500)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR_DIR="${REPO_ROOT}/vendor/llama.cpp"
LLAMA_CPP_REMOTE="${LLAMA_CPP_REMOTE:-https://github.com/ggerganov/llama.cpp.git}"
LLAMA_CPP_TAG="${LLAMA_CPP_TAG:-b1500}"

mkdir -p "${REPO_ROOT}/vendor"

if [[ -d "${VENDOR_DIR}/.git" ]]; then
    echo "[llama.cpp-kepler] vendor/llama.cpp already present; fetching tag ${LLAMA_CPP_TAG}..."
    cd "${VENDOR_DIR}"
    git fetch --tags --force origin
else
    echo "[llama.cpp-kepler] cloning ${LLAMA_CPP_REMOTE} into ${VENDOR_DIR}..."
    git clone "${LLAMA_CPP_REMOTE}" "${VENDOR_DIR}"
    cd "${VENDOR_DIR}"
fi

git checkout --detach "${LLAMA_CPP_TAG}"

echo
echo "[llama.cpp-kepler] vendor/llama.cpp pinned to ${LLAMA_CPP_TAG}."
echo "[llama.cpp-kepler] Now configure with:"
echo "    cmake -S . -B build -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON"
echo "    cmake --build build -j"
