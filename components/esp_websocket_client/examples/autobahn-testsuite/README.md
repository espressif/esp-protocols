# Autobahn WebSocket Testsuite for esp_websocket_client

This directory contains the setup for testing `esp_websocket_client` against the industry-standard [Autobahn WebSocket Testsuite](https://github.com/crossbario/autobahn-testsuite).

The Autobahn Testsuite is the de facto standard for testing WebSocket protocol compliance. It runs over 500 test cases covering:
- Frame parsing and generation
- Text and binary messages
- Fragmentation
- Control frames (PING, PONG, CLOSE)
- UTF-8 validation
- Protocol violations
- Edge cases and error handling

## 📋 Prerequisites

1. **Docker** - For running the Autobahn testsuite server
   - **Apple Silicon Macs (M1/M2/M3)**: The image runs via Rosetta 2 emulation (already configured)
   - **Intel Macs / Linux**: Native support
2. **ESP32 device** with WiFi capability
3. **ESP-IDF** development environment
4. **Network** - ESP32 and Docker host on the same network

## 🚀 Quick Start

### Step 1: Start the Autobahn Fuzzing Server

```bash
cd autobahn-testsuite
docker-compose up
```

This will:
- Start the Autobahn fuzzing server on port 9001
- Start a web server on port 8080 for viewing reports
- Mount the `reports/` directory for test results

You should see output like:
```
Autobahn WebSockets 0.7.4/0.10.9 Fuzzing Server (Port 9001)
Ok, will run 521 test cases for any clients connecting
```

### Step 2: Configure the ESP32 Testee Client

1. Find your Docker host IP address:
   ```bash
   # On Linux/Mac
   ifconfig
   # or
   ip addr show
   
   # Look for your local network IP (e.g., 192.168.1.100)
   ```

2. Update the configuration:
   ```bash
   cd testee
   idf.py menuconfig
   ```

3. Configure:
   - **Example Connection Configuration** → Set your WiFi SSID and password
   - **Autobahn Testsuite Configuration** → Set the server URI (e.g., `ws://192.168.1.100:9001`)

   Or edit `sdkconfig.defaults` directly:
   ```
   CONFIG_EXAMPLE_WIFI_SSID="YourWiFiSSID"
   CONFIG_EXAMPLE_WIFI_PASSWORD="YourWiFiPassword"
   CONFIG_AUTOBAHN_SERVER_URI="ws://192.168.1.100:9001"
   ```

### Step 3: Build and Flash the Testee

```bash
cd testee

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your ESP32's serial port.

### Step 4: Watch the Tests Run

The ESP32 will:
1. Connect to WiFi
2. Query the fuzzing server for the number of test cases
3. Run each test case sequentially
4. Echo back all received messages (as required by the testsuite)
5. Generate a final report

You'll see output like:
```
I (12345) autobahn_testee: ========== Test Case 1/300 ==========
I (12346) autobahn_testee: Running test case 1: ws://192.168.1.100:9001/runCase?case=1&agent=esp_websocket_client
I (12450) autobahn_testee: WEBSOCKET_EVENT_CONNECTED
...
I (12550) autobahn_testee: Test case 1 completed
```

### Step 5: View the Results

Once all tests complete, view the HTML report:

1. Open your web browser to: `http://<docker-host>:8080`
2. Click on the generated report
3. You'll see a comprehensive breakdown of all test results:
   - ✅ **Pass** - Compliant behavior
   - ⚠️ **Non-Strict** - Minor deviation (usually acceptable)
   - ❌ **Fail** - Protocol violation
   - ℹ️ **Informational** - Notes about behavior

The report will also be saved in `autobahn-testsuite/reports/clients/` directory.

## 📂 Directory Structure

```
autobahn-testsuite/
├── docker-compose.yml           # Docker configuration
├── config/
│   └── fuzzingserver.json      # Testsuite server configuration
├── reports/                     # Generated test reports (created automatically)
│   └── clients/
│       └── index.html          # Main report page
├── testee/                      # ESP32 testee client project
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   └── main/
│       ├── autobahn_testee.c   # Main testee implementation
│       ├── CMakeLists.txt
│       └── Kconfig.projbuild
└── README.md                    # This file
```

## ⚙️ Configuration

### Fuzzing Server Configuration

Edit `config/fuzzingserver.json` to customize test behavior:

```json
{
   "url": "ws://0.0.0.0:9001",
   "outdir": "/reports",
   "cases": ["*"],
   "exclude-cases": [
      "9.*",    // Excludes performance/mass tests
      "12.*",   // Excludes compression tests (permessage-deflate)
      "13.*"    // Excludes compression tests
   ]
}
```

**Note**: Test cases 12.* and 13.* are excluded by default as they test WebSocket compression (RFC7692), which is typically not implemented in embedded clients.

### Test Case Categories

- **1.*** - Framing
- **2.*** - Pings/Pongs
- **3.*** - Reserved Bits
- **4.*** - Opcodes
- **5.*** - Fragmentation
- **6.*** - UTF-8 Handling
- **7.*** - Close Handling
- **9.*** - Performance & Limits (excluded by default)
- **10.*** - Miscellaneous
- **12.*** - WebSocket Compression (excluded by default)
- **13.*** - WebSocket Compression (excluded by default)

## 🔧 Troubleshooting

### Apple Silicon Mac (M1/M2/M3) Platform Warning

If you see a warning like:
```
The requested image's platform (linux/amd64) does not match the detected host platform (linux/arm64/v8)
```

**This is normal and expected!** The Autobahn testsuite image only supports amd64, but Docker Desktop for Mac will automatically run it through Rosetta 2 emulation. The `platform: linux/amd64` line in `docker-compose.yml` handles this.

**Note**: There may be a slight performance overhead due to emulation, but it's typically not noticeable for this use case.

### ESP32 Can't Connect to Server

1. Verify Docker host IP is correct:
   ```bash
   docker inspect ws-fuzzing-server | grep IPAddress
   ```

2. Check firewall rules allow connections on port 9001

3. Ensure ESP32 and Docker host are on same network

4. Test connectivity:
   ```bash
   # From another machine on same network
   curl -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" \
        -H "Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==" \
        -H "Sec-WebSocket-Version: 13" \
        http://<docker-host>:9001/getCaseCount
   ```

### Tests Timeout or Hang

1. Increase network timeout in `autobahn_testee.c`:
   ```c
   websocket_cfg.network_timeout_ms = 30000;  // 30 seconds
   ```

2. Check WiFi signal strength

3. Monitor memory usage - insufficient heap can cause issues

### Some Tests Fail

This is normal! The testsuite is very strict and tests edge cases. Common issues:

- **UTF-8 validation** - May fail if not strictly validating text frames
- **Close frame handling** - May fail if close reason not properly echoed
- **Reserved bits** - May fail if not properly validating reserved bits

Review the HTML report to understand specific failures and determine if they're critical.

## 📊 Understanding Results

### Result Categories

- **Passed**: Implementation is compliant
- **Non-Strict**: Minor deviation, usually acceptable
- **Failed**: Protocol violation detected
- **Informational**: Observation about behavior

### Common Issues

1. **Text frame UTF-8 validation**
   - Testsuite sends invalid UTF-8 in text frames
   - Client should reject these

2. **Close frame payload**
   - Close frames can have a status code + reason
   - Client should echo back close frames properly

3. **Fragmentation**
   - Tests various fragmentation scenarios
   - Client must properly handle continuation frames

## 🎯 Goal

A high-quality WebSocket implementation should:
- Pass all core protocol tests (1.* through 11.*)
- Have minimal "Non-Strict" results
- Have no "Failed" results in critical areas

## 📚 References

- [Autobahn Testsuite Documentation](https://crossbar.io/autobahn/)
- [RFC 6455 - The WebSocket Protocol](https://tools.ietf.org/html/rfc6455)
- [Autobahn Testsuite GitHub](https://github.com/crossbario/autobahn-testsuite)

## 🐛 Reporting Issues

If you find protocol compliance issues with `esp_websocket_client`, please report them with:
1. The specific test case number that failed
2. The test report HTML output
3. ESP32 logs during the test
4. Your configuration (sdkconfig)

## 📝 License

This testsuite setup is provided under the same license as esp_websocket_client.
The Autobahn Testsuite itself is licensed under Apache 2.0.

