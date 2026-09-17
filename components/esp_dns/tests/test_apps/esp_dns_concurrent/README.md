# esp_dns concurrent resolver test

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

This test app stress-tests **`getaddrinfo()`** while the **esp_dns** component owns the lwIP **external resolve** hook. It exercises concurrent DoT/DoH lookups to catch regressions in per-query resolver state.

## What it does

1. Connects Wi-Fi or Ethernet via the **net_connect** component (`net_connect()`).
2. Initializes **Cloudflare** DNS-over-TLS (`1dot1dot1dot1.cloudflare-dns.com:853`) with the **certificate bundle**.
3. Starts several FreeRTOS tasks that call `getaddrinfo()` on real hostnames at the same time; then prints success/failure counts.
4. Repeats the same pattern for **DNS-over-HTTPS** (`1dot1dot1dot1.cloudflare-dns.com:443`, path `dns-query`).
5. Logs summary lines when lookup failures occur in a batch.

Tunable defines in `main/esp_dns_concurrent_test.c`:

- `NUM_WORKERS` — parallel tasks.
- `ITERATIONS_PER_TASK` — lookups per task after synchronized start.
- `WORKER_STACK_WORDS` — FreeRTOS stack depth (words).

## Requirements

- `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM=y` (set in `sdkconfig.defaults`).
- `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` for TLS to Cloudflare without a custom PEM.

## Build and flash

```bash
cd /path/to/esp-protocols/components/esp_dns/tests/test_apps/esp_dns_concurrent
idf.py set-target esp32c6
idf.py -p PORT flash monitor
```

## Expected output

- Many successful hostname lines under concurrent DoT/DoH.
- On failure batches, component logs may include **`ESP_DNS_DOT`** / **`ESP_DNS_DOH`** errors (e.g. failed to extract IP).

If you see unexpected failures after the concurrency fix, increase **`NUM_WORKERS`** and **`ITERATIONS_PER_TASK`** and run again.

## Troubleshooting

- **Network**: configure Wi-Fi/Ethernet credentials in **net_connect** options in `menuconfig`.
- **Certificate**: ensure certificate bundle options in `sdkconfig.defaults` match your IDF version (`menuconfig` → mbedTLS → certificate bundle). Cross-signed chain verification is enabled by default in this test app's `sdkconfig.defaults`. Disable it in menuconfig only if you do not need cross-signed chain support.
