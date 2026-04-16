#!/bin/bash
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Test harness: starts modem simulator, runs test app, collects results.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${MODEM_SIM_PORT:-10000}"
BATCH_SIZE="${MODEM_SIM_BATCH_SIZE:-0}"       # 0 = send whole response at once
BATCH_DELAY="${MODEM_SIM_BATCH_DELAY_MS:-1}"  # delay between chunks (ms)
SIM_BINARY="${SCRIPT_DIR}/modem_sim/build/modem_sim"
TEST_BINARY="${SCRIPT_DIR}/build/host_test_app.elf"
TEST_TIMEOUT="${TEST_TIMEOUT:-30}"

cleanup() {
    if [ -n "$SIM_PID" ]; then
        kill "$SIM_PID" 2>/dev/null
        kill -9 "$SIM_PID" 2>/dev/null
        wait "$SIM_PID" 2>/dev/null
    fi
}
trap cleanup EXIT

# --- Build modem_sim if needed ---
if [ ! -f "$SIM_BINARY" ]; then
    echo "Building modem_sim..."
    cmake -S "${SCRIPT_DIR}/modem_sim" -B "${SCRIPT_DIR}/modem_sim/build"
    cmake --build "${SCRIPT_DIR}/modem_sim/build"
fi

# --- Check test binary ---
if [ ! -f "$TEST_BINARY" ]; then
    echo "Error: test binary not found at $TEST_BINARY"
    echo "Build with: idf.py --preview set-target linux && idf.py build"
    exit 1
fi

# --- Start modem simulator ---
echo "Starting modem_sim on port $PORT (batch_size=$BATCH_SIZE, batch_delay=${BATCH_DELAY}ms)..."
"$SIM_BINARY" "$PORT" "$BATCH_SIZE" "$BATCH_DELAY" &
SIM_PID=$!

sleep 0.5
if ! kill -0 "$SIM_PID" 2>/dev/null; then
    echo "Error: modem_sim failed to start"
    exit 1
fi

# --- Run tests ---
echo "Running test app (timeout=${TEST_TIMEOUT}s)..."
export MODEM_SIM_PORT="$PORT"
timeout --signal=KILL "$TEST_TIMEOUT" "$TEST_BINARY"
TEST_RESULT=$?

if [ $TEST_RESULT -eq 137 ]; then
    echo "Error: test timed out after ${TEST_TIMEOUT}s"
fi

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "=== ALL TESTS PASSED ==="
else
    echo "=== TESTS FAILED (exit code: $TEST_RESULT) ==="
fi

exit $TEST_RESULT
