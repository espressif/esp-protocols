#!/bin/bash

# In CI, GITHUB_WORKSPACE points to the checkout root (where ci/ lives).
# Locally, fall back to git rev-parse to find the repo root.
if [ -n "${GITHUB_WORKSPACE}" ]; then
    REPO_ROOT="${GITHUB_WORKSPACE}/esp-protocols"
else
    REPO_ROOT="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
fi

SCRIPT_DIR="${REPO_ROOT}/components/esp_websocket_client/examples/linux"
python3 -m pip install -r "${REPO_ROOT}/ci/requirements.txt"
python3 "${REPO_ROOT}/components/esp_websocket_client/examples/target/websocket_server.py" --port 8080 &
WS_SERVER_PID=$!
echo "WebSocket server started (PID $WS_SERVER_PID)"
sleep 1

# Ensure the server is killed when the calling shell exits
trap "kill $WS_SERVER_PID 2>/dev/null || true" EXIT
