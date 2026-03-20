# Modem Simulator Component

A Wi-Fi modem simulator that extends ESP-AT with PPP server capabilities, turning ESP32 into a fully functional Wi-Fi modem. Perfect for testing AT commands and PPP connections without real hardware dependencies.

## What it does

- Extends ESP-AT firmware with PPP server functionality
- Provides DATA mode for raw IP communication
- Enables existing communication stacks (MQTT, HTTP, custom protocols) to work over Wi-Fi
- Ideal for testing ESP-Modem library and CI reliability

## Quick Start

```bash
cd common_components/modem_sim
./install.sh
source export.sh
idf.py build
```

## Custom Platform/Module

```bash
./install.sh PLATFORM_ESP32S3 WROOM-32
```

## Configuration

The `sdkconfig.defaults` includes:
- Wi-Fi and Bluetooth enabled
- PPP server support
- AT commands for HTTP/MQTT
- 4MB flash configuration

## Project Structure

```
modem_sim/
├── install.sh              # Installation script
├── export.sh               # Environment setup
├── sdkconfig.defaults      # Default configuration
├── pppd_cmd/               # Custom PPP commands
└── modem_sim_esp32/        # Generated ESP-AT build
```

## Use Cases

- Testing ESP-Modem library without real hardware
- Quick Wi-Fi connectivity for existing communication stacks
- CI/CD testing with reliable modem simulation
- Development and debugging of AT command implementations
