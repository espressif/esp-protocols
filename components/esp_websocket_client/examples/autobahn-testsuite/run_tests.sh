#!/bin/bash

# Autobahn Testsuite Runner Script
# This script automates the process of running WebSocket protocol compliance tests

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}Autobahn WebSocket Testsuite Runner${NC}"
echo -e "${BLUE}======================================${NC}"
echo

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo -e "${RED}Error: Docker is not running${NC}"
    echo "Please start Docker and try again"
    exit 1
fi

# Function to get host IP
get_host_ip() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        ifconfig | grep "inet " | grep -v 127.0.0.1 | awk '{print $2}' | head -1
    else
        # Linux
        hostname -I | awk '{print $1}'
    fi
}

HOST_IP=$(get_host_ip)

echo -e "${GREEN}Step 1: Starting Autobahn Fuzzing Server${NC}"
echo "Host IP detected: $HOST_IP"
echo

# Start the fuzzing server
docker-compose up -d

# Wait for server to be ready
echo "Waiting for fuzzing server to start..."
sleep 5

# Check if container is running
if ! docker ps | grep -q ws-fuzzing-server; then
    echo -e "${RED}Error: Fuzzing server failed to start${NC}"
    docker-compose logs
    exit 1
fi

echo -e "${GREEN}✓ Fuzzing server started successfully${NC}"
echo "  WebSocket endpoint: ws://$HOST_IP:9001"
echo "  Web interface: http://$HOST_IP:8080"
echo

echo -e "${YELLOW}Step 2: Configure and Flash ESP32${NC}"
echo
echo "Before running the testee client on ESP32:"
echo "  1. Update WiFi credentials in testee/sdkconfig.defaults"
echo "  2. Update Autobahn server URI to: ws://$HOST_IP:9001"
echo "  3. Build and flash:"
echo "     cd testee"
echo "     idf.py build"
echo "     idf.py -p PORT flash monitor"
echo
echo -e "${YELLOW}Press Enter when ESP32 testing is complete...${NC}"
read

echo
echo -e "${GREEN}Step 3: Viewing Results${NC}"
echo

# Open the report in browser
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    open "http://localhost:8080"
elif command -v xdg-open > /dev/null; then
    # Linux
    xdg-open "http://localhost:8080"
else
    echo "Open http://localhost:8080 in your browser to view results"
fi

echo
echo -e "${BLUE}Test reports are available at:${NC}"
echo "  Browser: http://localhost:8080"
echo "  Directory: $SCRIPT_DIR/reports/clients/"
echo

echo -e "${YELLOW}To stop the fuzzing server:${NC}"
echo "  docker-compose down"
echo

echo -e "${GREEN}Testing complete!${NC}"

