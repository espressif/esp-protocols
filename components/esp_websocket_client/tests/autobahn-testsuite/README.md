# Autobahn WebSocket Testsuite for esp_websocket_client

This directory contains the setup for testing `esp_websocket_client` against the [Autobahn WebSocket Testsuite](https://github.com/crossbario/autobahn-testsuite), which covers frame parsing, control frames, UTF-8 validation, and more.

The Autobahn Testsuite is the de facto standard for testing WebSocket protocol compliance. It runs over 500 test cases covering:
- Frame parsing and generation
- Text and binary messages
- Fragmentation
- Control frames (PING, PONG, CLOSE)
- UTF-8 validation
- Protocol violations
- Edge cases and error handling

## 📋 Prerequisites

1. **Docker** - For running the Autobahn testsuite server.
2. **ESP32 device** with WiFi capability.
3. **ESP-IDF** development environment.
4. **Network** - ESP32 and Docker host on the same network.

## 🚀 Quick Start

### 1. Start the Fuzzing Server

```bash
cd autobahn-testsuite
docker-compose up -d
```
Starts server on port 9001 and web interface on port 8080.

### 2. Configure ESP32 Client

**Required:** Set WiFi credentials in `testee/sdkconfig.defaults` or via `idf.py menuconfig`:

```
CONFIG_EXAMPLE_WIFI_SSID="YourWiFiSSID"
CONFIG_EXAMPLE_WIFI_PASSWORD="YourWiFiPassword"
```

**Optional:** The Server URI is automatically detected and sent by the test script (via `CONFIG_WEBSOCKET_URI_FROM_STDIN`).
To hardcode it instead, disable `CONFIG_WEBSOCKET_URI_FROM_STDIN` and set:
```
CONFIG_AUTOBAHN_SERVER_URI="ws://YOUR_HOST_IP:9001"
```

### 3. Run Tests (Automated)

The `run_tests.sh` script handles building, flashing, and monitoring:

```bash
./run_tests.sh /dev/ttyUSB0
```
*Note: Replace `/dev/ttyUSB0` with your serial port.*

**Using Pytest Manually:**
You can also run the monitoring step directly using pytest:
```bash
pytest pytest_autobahn.py --target esp32 --port /dev/ttyUSB0 --baud 115200 --app-path testee --skip-autoflash y
```

### 4. View Results

- **Web Interface:** `http://localhost:8080` (Click "Client Reports")
- **Summary Report:** Run `python3 scripts/generate_summary.py`

## 📂 Directory Structure

```
autobahn-testsuite/
├── docker-compose.yml           # Docker config
├── run_tests.sh                 # Automated runner script
├── pytest_autobahn.py           # Pytest monitoring script
├── config/                      # Server config
├── scripts/                     # Helper scripts
├── reports/                     # Test results
└── testee/                      # ESP32 client application
    ├── sdkconfig.defaults       # Default config
    ├── sdkconfig.ci.*           # CI configurations
    └── main/                    # Source code
```

## ⚙️ Configuration

### Testee
- `sdkconfig.ci.linux`: CI configuration for running the testee on a Linux host (host-based testing), with adjusted stack sizes and lock timeouts.
- `sdkconfig.ci.target.plain_tcp`: CI configuration for ESP32 target using Ethernet (IP101 PHY) and IPv6, without TLS.

### Fuzzing Server
Edit `config/fuzzingserver.json` to include/exclude cases (e.g., exclude performance/compression tests).

## 📊 Test Categories

| Category | Tests | Time | Critical? | Notes |
|----------|-------|------|-----------|-------|
| 1.* Framing | ~64 | 3m | ✅ Yes | Core compliance |
| 2.* Pings/Pongs | ~11 | 1m | ✅ Yes | Keepalive |
| 3.* Reserved Bits | ~7 | 1m | ✅ Yes | Extensions |
| 4.* Opcodes | ~10 | 1m | ✅ Yes | Protocol compliance |
| 5.* Fragmentation | ~20 | 2m | ✅ Yes | Large messages |
| 6.* UTF-8 | ~150 | 10m | ⚠️ Optional | strict validation often skipped |
| 7.* Close | ~35 | 3m | ✅ Yes | Clean disconnect |
| 9.* Performance | ~30 | 60m+ | ⚠️ Optional | Often excluded (resource intensive) |
| 10.* Misc | ~10 | 2m | ✅ Yes | Edge cases |
| 12-13.* Compression | ~200 | 30m | ❌ No | Typically not implemented |

## 💡 Tips



## 🧾 Generate Summary

After running the tests, Autobahn writes per-case JSON results under `reports/` (and/or `reports/clients/`).
You can generate a consolidated console + HTML summary using:

```bash
cd components/esp_websocket_client/tests/autobahn-testsuite
python3 scripts/generate_summary.py
```

This will:
- Print overall pass/fail statistics and per-category breakdown to the console
- Generate `reports/summary.html`

If you’re running in CI with GitHub Actions, the script will also write a markdown summary to the path in `GITHUB_STEP_SUMMARY` (when that env var is set).

## 📚 References

- [Autobahn Testsuite Documentation](https://crossbar.io/autobahn/)
- [RFC 6455 - The WebSocket Protocol](https://tools.ietf.org/html/rfc6455)
- [Autobahn Testsuite GitHub](https://github.com/crossbario/autobahn-testsuite)
