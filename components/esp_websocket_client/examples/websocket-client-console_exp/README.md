# ESP32 Remote Console over WebSocket

This project provides a remote console for ESP32, redirecting standard I/O (`stdin`, `stdout`, `stderr`) to a WebSocket via a VFS driver. It uses a Python WebSocket server (`console.py`) for remote interaction with the ESP32's command-line interface.

## Features
- WebSocket-based remote console with low-latency bidirectional communication.
- VFS driver redirects I/O to WebSocket, supporting standard C library functions.
- Console REPL for command execution, extensible via ESP-IDF console component.
- WebSocket URI configurable via NVS (default: `ws://192.168.50.231:8080`).

## Components
- **ESP32 Firmware**:
  - `main.c`: Initializes network, WebSocket client, VFS, and console.
  - `websocket_client_vfs.c/h`: VFS driver for WebSocket I/O redirection.
- **Python Server** (`console.py`): WebSocket server for console interaction using `websockets` and `aioconsole`.

## Prerequisites
- ESP-IDF v5.0+ configured.
- ESP32 board with Wi-Fi access.
- Python 3.7+ with `websockets` and `aioconsole` (`pip install websockets aioconsole`).
- Host machine on the same network as the ESP32.

## Setup

### ESP32
1. Clone the repository and navigate to the project directory.
2. Configure Wi-Fi:
   ```bash
   idf.py menuconfig
   ```
   - Set `Example Connection Configuration > WiFi SSID` and `WiFi Password`.
3. Build and flash:
   ```bash
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```

### Python Server
1. Run the WebSocket server:
   ```bash
   python console.py --host 0.0.0.0 --port 8080
   ```
   - Note the host’s IP (e.g., `192.168.50.231`) for ESP32 configuration.

## Usage
1. Start the Python server (`python console.py --host 0.0.0.0 --port 8080`).
2. Power on the ESP32; it connects to Wi-Fi and the WebSocket server.
3. In the Python server terminal, enter commands (e.g., `help`, `nvs`) and view responses.
4. Stop the server with `Ctrl+C` or reset the ESP32 to halt.

## Configuration
- **WebSocket URI**:
  - Default: `ws://192.168.50.231:8080`.
  - Or edit `DEFAULT_WS_URI` in `main.c` and reflash.
- **Wi-Fi**: Update via `menuconfig` or use provisioning (e.g., softAP).
