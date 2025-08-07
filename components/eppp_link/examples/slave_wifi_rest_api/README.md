# ESP32 [eppp\_link\_slave](../slave/) Example – Modifications and REST Wi-Fi Control

[See the original example here](../slave/)

This document describes the key modifications and usage instructions for the enhanced [slave](../slave/) example, enabling REST-based Wi-Fi configuration and monitoring.

---

## Overview

This version extends the original [slave](../slave/) PPP example with a full-featured REST API for Wi-Fi management. All new functionality is backward compatible; Wi-Fi can still be set via Kconfig, but runtime control is now possible from your browser or scripts.

---

## Key Changes

### 1. REST API for Wi-Fi

* Added HTTP server (`esp_http_server`) and JSON parsing (`cJSON`).
* REST endpoints:

  * `GET /wifi/status` — current status (connected/disconnected, SSID, RSSI).
  * `GET /wifi/scan` — scan available Wi-Fi networks.
  * `POST /wifi/connect` — connect to network (JSON: `{ "ssid": ..., "password": ... }`).
  * `POST /wifi/disconnect` — disconnect from Wi-Fi.
  * `OPTIONS /*` — CORS preflight for all URIs.

### 2. Dynamic Wi-Fi Reconfiguration

* Supports runtime change of Wi-Fi credentials without reboot or reflash.
* Connect/disconnect sequence is carefully synchronized using event groups for stability.
* If no default SSID is set, the device waits for configuration via REST.

### 3. Event Management Improvements

* Uses EventGroup bits to distinguish connection, failure, and disconnection events.
* Manual reconnect flag to suppress auto-retry logic during REST-initiated reconnects.

### 4. CORS Support

* All REST responses include `Access-Control-Allow-Origin: *` for easy browser access.

---

## How to Use

### Build & Flash

* Add `esp_http_server` and `cJSON` to your project.
* Build and flash as usual with ESP-IDF.

### Default Startup

* If Wi-Fi SSID and password are set in `sdkconfig`, ESP32 tries to connect at startup.
* If no SSID set, device waits for Wi-Fi config from REST client.

### REST API Usage

Interact from browser, curl, or your own UI:

* **Get status:**

  ```bash
  curl http://<esp32_ip>/wifi/status
  ```
* **Scan networks:**

  ```bash
  curl http://<esp32_ip>/wifi/scan
  ```
* **Connect:**

  ```bash
  curl -X POST http://<esp32_ip>/wifi/connect -H "Content-Type: application/json" -d '{"ssid":"YOUR_SSID","password":"YOUR_PASS"}'
  ```
* **Disconnect:**

  ```bash
  curl -X POST http://<esp32_ip>/wifi/disconnect
  ```

> See [README.md](./README.md) for a ready-to-use web UI!

---

## Implementation Notes

* All Wi-Fi connect/disconnect operations are handled asynchronously and robustly.
* If REST connect/disconnect is called in the middle of another operation, API will wait for proper sequence.
* All actions are CORS-enabled for direct use from web apps.
* Wi-Fi scan returns SSID, RSSI, and auth mode for each visible AP (max 16).

---

## Example: Integrating with Your UI

You can use the provided web UI ([See documentation for the web panel](webPanel/README.md)
) or integrate these endpoints into any admin dashboard, mobile app, or automation script.

---

## Credits

* Enhanced by \glarionenko, \2025
* Based on original code © 2023-2025 Espressif Systems (Shanghai) CO LTD
