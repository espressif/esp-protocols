#!/usr/bin/env bash
# Optional mutation-testing pass for a host unit-test suite using Mull.
#
# Prerequisites:
#   - clang matching the installed mull-ir-frontend / mull-runner (see ext/mull)
#   - Mull plugins on the library path (e.g. /usr/lib/mull-ir-frontend-18)
#   - IDF_PATH exported; ruby + libbsd as for normal unit tests
#
# Usage:
#   ./scripts/run_mull.sh test_pcb
#   ./scripts/run_mull.sh test_receiver
set -euo pipefail

SUITE="${1:-test_pcb}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT}/build_mull_${SUITE}"

# Prefer an explicit LLVM major if set; otherwise try common versions.
LLVM_MAJOR="${MULL_LLVM_MAJOR:-18}"
FRONTEND="${MULL_IR_FRONTEND:-/usr/lib/mull-ir-frontend-${LLVM_MAJOR}}"
RUNNER="${MULL_RUNNER:-mull-runner-${LLVM_MAJOR}}"

if [[ ! -f "${FRONTEND}" ]]; then
    echo "Mull IR frontend not found at ${FRONTEND}" >&2
    echo "Install Mull for clang-${LLVM_MAJOR}, or set MULL_IR_FRONTEND / MULL_LLVM_MAJOR." >&2
    echo "Source tree with docs: ${ROOT}/../../../../ext/mull" >&2
    exit 1
fi

if ! command -v "${RUNNER}" >/dev/null 2>&1; then
    echo "mull-runner not found (${RUNNER}). Set MULL_RUNNER if needed." >&2
    exit 1
fi

export CC="${CC:-clang-${LLVM_MAJOR}}"
if ! command -v "${CC}" >/dev/null 2>&1; then
    CC=clang
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" \
    -DUNIT_TESTS="${SUITE}" \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_C_FLAGS="-O0 -fpass-plugin=${FRONTEND} -g -grecord-command-line -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

export MULL_CONFIG="${ROOT}/mull.yml"

REPORT_DIR="${ROOT}/mull_reports"
mkdir -p "${REPORT_DIR}"
REPORT_NAME="mdns_${SUITE}"

# Default: SQLite report on disk (no flood of IDE warnings).
# Set MULL_CONSOLE=1 to also print clang-style survivor warnings.
REPORTERS=(--reporters SQLite)
if [[ "${MULL_CONSOLE:-0}" == "1" ]]; then
    REPORTERS+=(--reporters IDE)
fi

"${RUNNER}" --allow-surviving \
    "${REPORTERS[@]}" \
    --report-dir "${REPORT_DIR}" \
    --report-name "${REPORT_NAME}" \
    "${BUILD_DIR}/mdns_host_unit_test" -- --test

echo
echo "Report: ${REPORT_DIR}/${REPORT_NAME}.sqlite"
echo "Browse survivors, e.g.:"
echo "  sqlite3 ${REPORT_DIR}/${REPORT_NAME}.sqlite \"SELECT filename, line_number, mutator FROM mutant WHERE status = 2 LIMIT 20;\""
