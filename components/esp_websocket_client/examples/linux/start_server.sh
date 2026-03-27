#!/bin/bash

pip install SimpleWebSocketServer
python3 $GITHUB_WORKSPACE/esp-protocols/components/esp_websocket_client/examples/target/websocket_server.py --port 8080 &
WS_SERVER_PID=$!
echo "WebSocket server started (PID $WS_SERVER_PID)"

# Wait until the server is accepting connections (up to 10 s)
for i in $(seq 1 10); do
    python3 -c "import socket; s=socket.create_connection(('localhost',8080),timeout=1); s.close()" 2>/dev/null && echo "Server ready" && break
    echo "Waiting for server... ($i)" && sleep 1
done

# Ensure the server is killed when the calling shell exits
trap "kill $WS_SERVER_PID 2>/dev/null || true" EXIT
