#!/bin/bash
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
#
# Starts a local WebSocket echo server before running the host test executable.
# Sourced by run-host-tests.yml via the pre_run_executable_script hook so that
# the server process is cleaned up when the parent shell exits.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 -m pip install -r "${SCRIPT_DIR}/requirements.txt"
python3 "${SCRIPT_DIR}/../target/websocket_server.py" --port 8080 &
WS_SERVER_PID=$!
echo "WebSocket server started (PID $WS_SERVER_PID)"
sleep 1

# Ensure the server is killed when the calling shell exits
trap "kill $WS_SERVER_PID 2>/dev/null || true" EXIT
