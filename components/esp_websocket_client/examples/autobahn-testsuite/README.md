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

**Option A: Automated (Recommended)**

Use the `run_tests.sh` script for full automation:

```bash
# From autobahn-testsuite directory
./run_tests.sh /dev/ttyUSB0
```

This will:
- Build the testee
- Flash to ESP32
- Monitor serial output
- Detect test completion automatically
- Open results in browser

**Option B: Manual**

```bash
cd testee

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your ESP32's serial port.

**Using Pre-configured CI Builds**

For CI/CD or optimized builds, use the pre-configured sdkconfig files:

```bash
cd testee

# For Linux target
idf.py set-target linux
cp sdkconfig.ci.linux sdkconfig.defaults
idf.py build

# For ESP32 hardware target
idf.py set-target esp32
cp sdkconfig.ci.target.plain_tcp sdkconfig.defaults
# Configure WiFi/Ethernet, then:
idf.py build
```

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

**Option 1: Web Interface**
1. Open your web browser to: `http://localhost:8080` (not port 9001 - that's the WebSocket server)
2. Click on the **"Client Reports"** link on the page
3. You'll see a comprehensive breakdown of all test results:

**Option 2: Generate Summary Report (Recommended)**
```bash
cd autobahn-testsuite
python3 scripts/generate_summary.py
```
This generates a detailed console summary and an HTML summary at `reports/summary.html`.

**Option 3: Direct File Access**
The report is saved in `autobahn-testsuite/reports/clients/` directory. Open it directly:

```bash
# macOS
open reports/clients/index.html

# Linux
xdg-open reports/clients/index.html

# Or manually navigate to the file in your file browser
# Path: autobahn-testsuite/reports/clients/index.html
```

## 📂 Directory Structure

```
autobahn-testsuite/
├── docker-compose.yml           # Docker configuration
├── run_tests.sh                 # Automated test runner script
├── config/
│   └── fuzzingserver.json      # Testsuite server configuration
├── scripts/                     # Automation scripts
│   ├── monitor_serial.py       # Serial monitor with completion detection
│   ├── analyze_results.py      # Result analysis tools
│   └── generate_summary.py     # Generate test summary reports
├── reports/                     # Generated test reports (created automatically)
│   └── clients/
│       └── index.html          # Main report page
├── testee/                      # ESP32 testee client project
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults      # Default configuration
│   ├── sdkconfig.ci.linux      # CI config for Linux target builds
│   ├── sdkconfig.ci.target.plain_tcp  # CI config for ESP32 hardware (Ethernet)
│   └── main/
│       ├── autobahn_testee.c   # Main testee implementation
│       ├── CMakeLists.txt
│       └── Kconfig.projbuild
└── README.md                    # This file
```

## ⚙️ Configuration

### Testee Configuration

The testee project includes several pre-configured sdkconfig files:

#### 1. `sdkconfig.defaults` - Default Configuration
Base configuration for manual testing. Edit this file to set:
- WiFi credentials (`CONFIG_EXAMPLE_WIFI_SSID`, `CONFIG_EXAMPLE_WIFI_PASSWORD`)
- Server URI (`CONFIG_AUTOBAHN_SERVER_URI`)
- Other test-specific settings

#### 2. `sdkconfig.ci.linux` - Linux Target Configuration
Pre-configured for Linux host builds (used in CI/CD):

```bash
cd testee
idf.py set-target linux
cp sdkconfig.ci.linux sdkconfig.defaults
idf.py build
```

**Features:**
- Linux target (`CONFIG_IDF_TARGET_LINUX=y`)
- URI from stdin (`CONFIG_WEBSOCKET_URI_FROM_STDIN=y`)
- Optimized buffer sizes for testing
- Separate TX lock enabled

#### 3. `sdkconfig.ci.target.plain_tcp` - ESP32 Hardware Configuration
Pre-configured for ESP32 hardware with Ethernet (used in CI/CD):

```bash
cd testee
idf.py set-target esp32
cp sdkconfig.ci.target.plain_tcp sdkconfig.defaults
# Add your WiFi/Ethernet config
idf.py build
```

**Features:**
- ESP32 target
- URI from stdin (`CONFIG_WEBSOCKET_URI_FROM_STDIN=y`)
- Ethernet support (can be configured for WiFi)
- Plain TCP (no TLS)

**Note**: These CI configs are optimized for automated testing. For manual testing, use `sdkconfig.defaults` and configure via `idf.py menuconfig`.

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

The Autobahn Testsuite is organized into test categories, each focusing on a specific aspect of the WebSocket protocol (RFC 6455). Here's a detailed breakdown:

#### **1.* - Framing** (~64 tests, ~3 minutes)
**Critical for**: Core protocol compliance

Tests the fundamental WebSocket frame structure and parsing:
- Valid frame structures (FIN, RSV bits, opcode, mask, payload length)
- Frame masking requirements (clients must mask, servers must not)
- Payload length encoding (7-bit, 16-bit, 64-bit formats)
- Reserved bits validation (RSV1, RSV2, RSV3 must be 0 unless extension negotiated)
- Frame boundary detection

**Common issues**: Incorrect masking behavior, wrong payload length calculation, not validating reserved bits

---

#### **2.* - Pings/Pongs** (~11 tests, ~1 minute)
**Critical for**: Connection keepalive, detecting stale connections

Tests control frame handling for connection management:
- PING frames must be automatically replied with PONG
- PONG frames can be unsolicited (sent proactively)
- PING payload must be echoed exactly in PONG response
- PING/PONG handling during message fragmentation
- Large PING payloads (up to 125 bytes)

**Common issues**: Not responding to PING automatically, modifying PING payload in PONG, not handling PING during fragmented messages

---

#### **3.* - Reserved Bits** (~7 tests, ~1 minute)
**Critical for**: Future protocol extensions (e.g., compression)

Tests validation of reserved frame header bits:
- RSV1, RSV2, RSV3 bits must be 0 unless extension negotiated
- Frames with reserved bits set should be rejected
- Connection must close with proper error code (1002 - protocol error)

**Common issues**: Not validating reserved bits, accepting frames with RSV bits set when no extension negotiated

---

#### **4.* - Opcodes** (~10 tests, ~1 minute)
**Critical for**: Protocol compliance, future extensions

Tests valid and invalid opcode handling:
- **Valid opcodes**: 0x0 (continuation), 0x1 (text), 0x2 (binary), 0x8 (close), 0x9 (ping), 0xA (pong)
- **Invalid opcodes**: 0x3-0x7 (reserved data), 0xB-0xF (reserved control)
- Connection must close on invalid opcode

**Common issues**: Accepting reserved opcodes, not closing connection on invalid opcode

---

#### **5.* - Fragmentation** (~20 tests, ~2 minutes)
**Critical for**: Handling large messages, streaming data

Tests message fragmentation and continuation frames:
- Single fragment messages (FIN=1)
- Multiple fragment messages (FIN=0, then FIN=1)
- Continuation frames (opcode 0x0)
- Interleaving control frames (PING/PONG/CLOSE) during fragmentation
- Fragmentation of both text and binary messages

**Common issues**: Not properly tracking fragmentation state, rejecting control frames during fragmentation (they're allowed!), starting new message before previous completed

---

#### **6.* - UTF-8 Handling** (~150+ tests, ~10 minutes)
**Important for**: Security, preventing injection attacks
**Often excluded**: Many embedded implementations skip strict UTF-8 validation

Tests strict UTF-8 validation for TEXT frames:
- Valid UTF-8 sequences
- Invalid UTF-8: wrong start bytes, invalid continuations
- Overlong encodings (security issue - can bypass validation)
- UTF-8 boundaries in fragmented messages
- Invalid code points (surrogates, beyond U+10FFFF)

**Common issues**: Not validating UTF-8 at all, accepting invalid UTF-8 sequences, not handling UTF-8 boundaries in fragments correctly

**Note**: Many embedded implementations skip strict UTF-8 validation due to memory/CPU constraints. This is acceptable for trusted environments but risky for public-facing services.

---

#### **7.* - Close Handshake** (~35 tests, ~3 minutes)
**Critical for**: Clean disconnection, resource cleanup

Tests proper connection closing:
- Close frame structure (2-byte status code + optional UTF-8 reason)
- Valid and invalid close codes (1000-1015, plus some reserved ranges)
- Close frame must be echoed back
- No data frames allowed after close frame
- Close during message fragmentation
- Close frame payload validation (must be valid UTF-8 if present)

**Common issues**: Not echoing close frame, accepting invalid close codes, not validating close payload

---

#### **9.* - Limits/Performance** (~30 tests, 60+ minutes)
**Usually excluded**: Time-consuming and memory-intensive
**Why excluded**: Not practical for embedded devices

Tests performance under stress:
- Large message sizes (64KB, 256KB, 1MB, 16MB)
- Many small messages (1000+ messages in rapid succession)
- Rapid connection/disconnection cycles
- Binary message performance at scale

**Why section 9 is excluded**:
1. **Time consuming**: These tests can take hours to complete, making them impractical for regular testing
2. **Memory constraints**: Embedded devices (like ESP32) have limited RAM. Tests with 16MB messages will fail or cause crashes
3. **Not always relevant**: Resource-constrained devices are not expected to handle multi-megabyte messages or thousands of rapid connections
4. **Development workflow**: Excluding these allows faster iteration during development while still testing core protocol compliance

The `quick_test.sh` script and default `fuzzingserver.json` both exclude section 9.* for these reasons. If you need to test performance limits, you can manually include them, but be prepared for very long test runs.

---

#### **10.* - Miscellaneous** (~10 tests, ~2 minutes)
**Critical for**: Robustness in real-world conditions

Tests various edge cases:
- Delayed frame sending
- Slow message delivery
- Network behavior simulation
- Timeout handling

---

#### **12.* & 13.* - WebSocket Compression** (~200+ tests, ~30 minutes total)
**Usually excluded**: Optional extension, not commonly implemented
**Why excluded**: RFC 7692 (permessage-deflate) is optional

Tests WebSocket compression extension (RFC 7692):
- Deflate compression parameters negotiation
- Context takeover (reusing compression context)
- Compression with message fragmentation
- Invalid compression data handling
- Compression performance

**Why excluded**:
1. **Optional extension**: RFC 7692 is not required for WebSocket compliance
2. **Complexity**: Adds significant implementation complexity and memory overhead
3. **Rarely implemented**: Most embedded WebSocket clients don't implement compression
4. **Limited benefit**: Compression is rarely needed on local networks where embedded devices typically operate

---

### Test Configuration Summary

| Category | Tests | Time | Critical? | Default Exclusion |
|----------|-------|------|-----------|-------------------|
| 1.*      | ~64   | 3m   | ✅ Yes    | No                |
| 2.*      | ~11   | 1m   | ✅ Yes    | No                |
| 3.*      | ~7    | 1m   | ✅ Yes    | No                |
| 4.*      | ~10   | 1m   | ✅ Yes    | No                |
| 5.*      | ~20   | 2m   | ✅ Yes    | No                |
| 6.*      | ~150  | 10m  | ⚠️ Optional | Sometimes (UTF-8) |
| 7.*      | ~35   | 3m   | ✅ Yes    | No                |
| 9.*      | ~30   | 60m+ | ⚠️ Optional | **Yes (Performance)** |
| 10.*     | ~10   | 2m   | ✅ Yes    | No                |
| 12.*     | ~100  | 15m  | ❌ No     | Yes (Compression) |
| 13.*     | ~100  | 15m  | ❌ No     | Yes (Compression) |

**Legend**:
- ✅ **Critical**: Essential for protocol compliance
- ⚠️ **Optional**: Nice to have but often excluded for practical reasons
- ❌ **Not applicable**: Optional extension, rarely implemented

### Recommended Test Configurations

**Quick Test** (used by `quick_test.sh`):
- Includes: 1.*, 2.*, 3.*, 4.*, 5.*, 7.*, 10.*
- Excludes: 6.* (UTF-8), 9.* (Performance), 12.*, 13.* (Compression)
- **~150 tests, 5-10 minutes**

**Standard Test** (default `fuzzingserver.json`):
- Includes: All categories
- Excludes: 9.* (Performance), 12.*, 13.* (Compression)
- **~300 tests, 15-30 minutes**

**Full Compliance Test**:
- Includes: All categories
- Excludes: None
- **~500+ tests, 1-2 hours** (includes performance and compression)

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

4. Check if ESP32 is still responding:
   ```bash
   # Try to see serial output
   idf.py -p /dev/ttyUSB0 monitor
   ```

### No Serial Output After Flash

1. **Check serial port**:
   ```bash
   # macOS
   ls /dev/cu.*

   # Linux
   ls /dev/ttyUSB* /dev/ttyACM*
   ```

2. **Press RESET button** on ESP32 - it may need a reset after flashing

3. **Check baud rate** - should be 115200

4. **Verify flash succeeded**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   # Should see "Hash of data verified" message
   ```

### Finding Your Serial Port

**macOS:**
```bash
ls /dev/cu.* | grep -i usb
# Common: /dev/cu.usbserial-10, /dev/cu.SLAB_USBtoUART
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# Common: /dev/ttyUSB0, /dev/ttyACM0
```

**Windows:**
- Check Device Manager → Ports (COM & LPT)
- Use COM port number (e.g., `COM3`)

### Some Tests Fail

This is normal! The testsuite is very strict and tests edge cases. Common issues:

- **UTF-8 validation** - May fail if not strictly validating text frames
- **Close frame handling** - May fail if close reason not properly echoed
- **Reserved bits** - May fail if not properly validating reserved bits

Review the HTML report to understand specific failures and determine if they're critical.

## 🛠️ Automation Scripts

The `scripts/` directory contains helpful automation tools:

- **`monitor_serial.py`** - Monitors ESP32 serial output and detects test completion automatically
  - Useful for CI/CD pipelines
  - Supports timeout and completion pattern detection
  - Can send server URI via serial for `CONFIG_WEBSOCKET_URI_FROM_STDIN` builds

- **`generate_summary.py`** - Generates detailed summary reports from test results
  - Analyzes HTML/JSON reports
  - Provides pass/fail statistics
  - Useful for tracking compliance over time

- **`analyze_results.py`** - Analyzes test results and identifies patterns
  - Helps identify common failure modes
  - Useful for debugging protocol issues

- **`run_tests.sh`** - Main automation script (in root directory)
  - Handles full test workflow: build, flash, monitor, results

## ⏱️ Test Duration

Typical test execution times:
- **Full test suite (300+ cases)**: 20-40 minutes
- **Quick test (cases 1-16)**: 2-5 minutes
- **Individual test case**: 1-10 seconds

Factors affecting duration:
- Network latency
- WiFi signal strength
- ESP32 processing speed
- Test case complexity

## 🔄 Common Workflows

### Quick Test Run
Test a subset of cases quickly:
```bash
# Edit config/fuzzingserver.json to include only specific cases
# Then run normally
./run_tests.sh /dev/ttyUSB0
```

### Re-run After Code Changes
```bash
# Just rebuild and flash (no need to restart server)
cd testee
idf.py build
idf.py -p /dev/ttyUSB0 flash
# Monitor manually or use monitor_serial.py
```

### View Results Without Re-running

**Option 1: Web Interface (if server is running)**
```bash
# Open the Autobahn web interface
open http://localhost:8080

# Click on "Client Reports" link to view test results
```

**Option 2: Generate Summary Report (Recommended)**
```bash
# From the autobahn-testsuite directory
cd autobahn-testsuite
python3 scripts/generate_summary.py

# The script automatically:
# - Finds reports in the reports/ directory
# - Generates a console summary with statistics
# - Creates an HTML summary at reports/summary.html
# - Opens the summary in your browser

# Output includes:
# - Overall pass/fail statistics
# - Category breakdown (Framing, Ping/Pong, etc.)
# - List of failed tests with reasons
# - Recommendations for improvement
```

**Option 3: View Reports Directly**
```bash
# Open the JSON report directly
open reports/clients/index.html

# Or view the generated summary
open reports/summary.html
```

### Stop and Restart Tests
If you need to interrupt tests:
1. Press `Ctrl+C` in the monitor
2. The server will keep running
3. Simply re-flash and restart - the server will continue from where it left off

## 📊 Understanding Results

### Result Categories

- **Passed**: Implementation is compliant
- **Non-Strict**: Minor deviation, usually acceptable
- **Failed**: Protocol violation detected
- **Informational**: Observation about behavior

### What to Focus On

**Critical (should pass):**
- Framing tests (1.*)
- Basic opcodes (4.*)
- Close handling (7.*)

**Important (should mostly pass):**
- Pings/Pongs (2.*)
- Fragmentation (5.*)
- UTF-8 handling (6.*)

**Acceptable to have issues:**
- Reserved bits (3.*) - if not validating
- Performance tests (9.*) - often excluded
- Compression (12.*, 13.*) - typically not implemented

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

## 💡 Tips & Best Practices

1. **Start with a subset** - Test cases 1-16 first to verify basic functionality
2. **Monitor heap** - Watch for memory leaks during long test runs
3. **Save reports** - Keep HTML reports for comparison after code changes
4. **Use automated script** - `run_tests.sh` handles most edge cases automatically
5. **Check logs** - Serial output shows which test cases are running/failing

## 🎯 Goal

A high-quality WebSocket implementation should:
- Pass all core protocol tests (1.* through 11.*)
- Have minimal "Non-Strict" results
- Have no "Failed" results in critical areas

**Typical good results:**
- ✅ 95%+ Pass rate on core tests (1.*, 2.*, 4.*, 5.*, 7.*)
- ⚠️ Some Non-Strict results are acceptable
- ❌ Zero Failed results in critical areas (framing, opcodes, close)

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
