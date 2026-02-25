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
CYAN='\033[0;36m'
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

echo -e "${YELLOW}Step 2: Running Tests on ESP32${NC}"
echo

# Check for ESP32 port argument
ESP_PORT="${1:-}"
BUILD_ONLY="${2:-}"

if [ -z "$ESP_PORT" ]; then
    echo -e "${YELLOW}No serial port specified. Manual mode:${NC}"
    echo
    echo "To run tests automatically, provide the ESP32 serial port:"
    echo "  ./run_tests.sh /dev/ttyUSB0"
    echo
    echo "Or build only:"
    echo "  ./run_tests.sh /dev/ttyUSB0 build"
    echo
    echo "Manual steps:"
    echo "  1. Update WiFi credentials in testee/sdkconfig.defaults"
    echo "  2. Update Autobahn server URI to: ws://$HOST_IP:9001"
    echo "  3. Build and flash:"
    echo "     cd testee"
    echo "     idf.py build"
    echo "     idf.py -p PORT flash monitor"
    echo
    echo -e "${YELLOW}Press Enter when ESP32 testing is complete...${NC}"
    read
else
    echo "ESP32 serial port: $ESP_PORT"
    echo

    # Check if ESP-IDF is available
    if ! command -v idf.py > /dev/null 2>&1; then
        echo -e "${RED}Error: ESP-IDF not found in PATH${NC}"
        echo "Please source ESP-IDF environment: . \$HOME/esp/esp-idf/export.sh"
        exit 1
    fi

    # Check if port exists
    if [ ! -e "$ESP_PORT" ]; then
        echo -e "${RED}Error: Serial port $ESP_PORT does not exist${NC}"
        exit 1
    fi

    cd testee

    # Verify we're in the right directory
    if [ ! -f "sdkconfig.defaults" ] && [ ! -f "sdkconfig" ]; then
        echo -e "${RED}Error: Not in testee directory or sdkconfig files not found${NC}"
        echo "  Current directory: $(pwd)"
        exit 1
    fi

    # Update server URI in sdkconfig.defaults if needed
    if [ -f "sdkconfig.defaults" ]; then
        if ! grep -q "CONFIG_AUTOBAHN_SERVER_URI.*$HOST_IP" sdkconfig.defaults 2>/dev/null; then
            echo -e "${YELLOW}Updating server URI in sdkconfig.defaults...${NC}"
            # Backup original
            cp sdkconfig.defaults sdkconfig.defaults.bak 2>/dev/null || true
            # Update URI (simple approach - user should verify)
            if [[ "$OSTYPE" == "darwin"* ]]; then
                # macOS sed requires different syntax
                sed -i '' "s|CONFIG_AUTOBAHN_SERVER_URI=.*|CONFIG_AUTOBAHN_SERVER_URI=\"ws://$HOST_IP:9001\"|" sdkconfig.defaults 2>/dev/null || true
            else
                sed -i.bak "s|CONFIG_AUTOBAHN_SERVER_URI=.*|CONFIG_AUTOBAHN_SERVER_URI=\"ws://$HOST_IP:9001\"|" sdkconfig.defaults 2>/dev/null || true
            fi
        fi
    fi

    echo -e "${GREEN}Building testee...${NC}"
    idf.py build

    if [ "$BUILD_ONLY" = "build" ]; then
        echo -e "${GREEN}Build complete. Flash manually with:${NC}"
        echo "  idf.py -p $ESP_PORT flash monitor"
        cd ..
        exit 0
    fi

    echo -e "${GREEN}Flashing ESP32...${NC}"
    idf.py -p "$ESP_PORT" flash

    echo
    echo -e "${YELLOW}Waiting for ESP32 to boot (5 seconds)...${NC}"
    sleep 5

    # Check if CONFIG_WEBSOCKET_URI_FROM_STDIN is enabled
    # We're in testee directory, so check files here
    URI_FROM_STDIN=""
    if [ -f "sdkconfig" ] && grep -q "CONFIG_WEBSOCKET_URI_FROM_STDIN=y" sdkconfig 2>/dev/null; then
        URI_FROM_STDIN="ws://$HOST_IP:9001"
        echo -e "${GREEN}✓ CONFIG_WEBSOCKET_URI_FROM_STDIN is enabled (from sdkconfig)${NC}"
        echo "  URI will be sent via serial"
    elif [ -f "sdkconfig.defaults" ] && grep -q "CONFIG_WEBSOCKET_URI_FROM_STDIN=y" sdkconfig.defaults 2>/dev/null; then
        URI_FROM_STDIN="ws://$HOST_IP:9001"
        echo -e "${GREEN}✓ CONFIG_WEBSOCKET_URI_FROM_STDIN is enabled (from sdkconfig.defaults)${NC}"
        echo "  URI will be sent via serial"
    else
        echo -e "${YELLOW}⚠ CONFIG_WEBSOCKET_URI_FROM_STDIN is not enabled${NC}"
        echo "  ESP32 will use URI from sdkconfig.defaults"
        echo "  Make sure CONFIG_AUTOBAHN_SERVER_URI is set correctly"
    fi

    echo
    echo -e "${GREEN}Starting test execution...${NC}"
    echo
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Serial output (tests in progress):${NC}"
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo
    echo -e "${YELLOW}Note: Connection errors are expected for some test cases${NC}"
    echo "      (e.g., error handling tests). The testee will continue automatically."
    echo
    echo -e "${GREEN}Look for test case progress messages like:${NC}"
    echo "  'Starting test case X...'"
    echo "  'Test case X completed'"
    echo
    echo -e "${YELLOW}If no output appears, try:${NC}"
    echo "  - Press RESET button on ESP32"
    echo "  - Check WiFi credentials in sdkconfig.defaults"
    echo "  - Verify serial port: $ESP_PORT"
    echo
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    cd ..

    # Detect target from sdkconfig if available
    TARGET="esp32"
    if [ -f "testee/sdkconfig" ]; then
        if grep -q "CONFIG_IDF_TARGET=" testee/sdkconfig; then
            TARGET=$(grep "CONFIG_IDF_TARGET=" testee/sdkconfig | cut -d'"' -f2)
        fi
    fi

    # Use pytest-embedded to monitor and detect completion
    # Only send URI if CONFIG_WEBSOCKET_URI_FROM_STDIN is enabled
    # We use --app-path testee so pytest finds the build artifacts
    # We use --skip-autoflash y because we already flashed above
    PYTEST_ARGS="pytest_autobahn.py --target $TARGET --port $ESP_PORT --baud 115200 --app-path testee --skip-autoflash y"
    if [ ! -z "$URI_FROM_STDIN" ]; then
        PYTEST_ARGS="$PYTEST_ARGS --uri $URI_FROM_STDIN"
    fi

    if pytest $PYTEST_ARGS; then
        echo
        echo -e "${GREEN}✓ Tests completed successfully!${NC}"
        echo "  Check the web interface for detailed results"
    else
        echo
        echo -e "${YELLOW}⚠ Test execution ended (timeout or error)${NC}"
        echo "  This may be normal if:"
        echo "    - Tests are still running (check serial output)"
        echo "    - Connection errors occurred (some are expected)"
        echo "    - Timeout was reached"
        echo
        echo "  Check serial output above and web interface for details"
    fi
fi

echo
echo -e "${GREEN}Step 3: Viewing Results${NC}"
echo

# Define report paths
REPORT_FILE="$SCRIPT_DIR/reports/clients/index.html"
WEB_INTERFACE="http://localhost:8080"

# Check if reports directory exists and has content
if [ -d "$SCRIPT_DIR/reports/clients" ] && [ "$(ls -A $SCRIPT_DIR/reports/clients/*.json 2>/dev/null)" ]; then
    echo -e "${GREEN}✓ Test reports found${NC}"
    echo

    # Generate and display summary
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Generating Test Summary...${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    if python3 "$SCRIPT_DIR/scripts/generate_summary.py" 2>/dev/null; then
        echo
        echo -e "${GREEN}✓ Summary generated successfully${NC}"
    else
        echo -e "${YELLOW}⚠ Could not generate summary (reports may still be processing)${NC}"
        echo "  Run manually: cd $SCRIPT_DIR && python3 scripts/generate_summary.py"
    fi
else
    echo -e "${YELLOW}⚠ No test reports found yet${NC}"
    echo "  Reports may still be generating, or tests may not have completed"
    echo "  Check the web interface or wait a few moments and run:"
    echo "    cd $SCRIPT_DIR && python3 scripts/generate_summary.py"
fi

echo
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Test Reports Available:${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
if [[ -f "$REPORT_FILE" ]]; then
    echo "  📄 Direct file: file://$REPORT_FILE"
fi
echo "  🌐 Web interface: $WEB_INTERFACE (click 'Client Reports' link)"
echo "  📁 Directory: $SCRIPT_DIR/reports/clients/"
if [[ -f "$SCRIPT_DIR/reports/summary.html" ]]; then
    echo "  📊 Summary: file://$SCRIPT_DIR/reports/summary.html"
fi
echo

echo -e "${YELLOW}To stop the fuzzing server:${NC}"
echo "  docker-compose down"
echo

echo -e "${GREEN}Testing complete!${NC}"
