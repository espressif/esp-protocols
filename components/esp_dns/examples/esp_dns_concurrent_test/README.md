# esp_dns concurrent resolver test

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

This example stress-tests **`getaddrinfo()`** while the **esp_dns** component owns the lwIP **external resolve** hook. It is meant to surface concurrency bugs documented in [ISSUES_AND_PROGRESS.md](../../ISSUES_AND_PROGRESS.md):

- **Issue 1** — shared singleton resolver state (`response_buffer`, transaction id) under concurrent lookups.
- **Issue 4** — DNS-over-HTTPS builds the response in the same shared handle across overlapping HTTP requests.

## What it does

1. Connects Wi-Fi or Ethernet via the **net_connect** component (`net_connect()`).
2. Initializes **Cloudflare** DNS-over-TLS (`1dot1dot1dot1.cloudflare-dns.com:853`) with the **certificate bundle**.
3. Starts several FreeRTOS tasks that call `getaddrinfo()` on real hostnames at the same time; then prints success/failure counts.
4. Repeats the same pattern for **DNS-over-HTTPS** (`1dot1dot1dot1.cloudflare-dns.com:443`, path `dns-query`).
5. Logs explicit lines when failures occur, e.g. **Issue 1 reproduced** (DoT) / **Issue 4 reproduced** (DoH).

Tunable defines in `main/esp_dns_concurrent_test.c`:

- `NUM_WORKERS` — parallel tasks.
- `ITERATIONS_PER_TASK` — lookups per task after synchronized start.
- `WORKER_STACK_WORDS` — FreeRTOS stack depth (words).

## Requirements

- `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM=y` (set in `sdkconfig.defaults`).
- `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` for TLS to Cloudflare without a custom PEM.

## Build and flash

```bash
cd /path/to/esp-protocols/components/esp_dns/examples/esp_dns_concurrent_test
idf.py set-target esp32c6
idf.py -p PORT flash monitor
```

## Expected output

- Many successful hostname lines, or intermittent **`getaddrinfo()`** failures (status codes depend on lwIP / LWIP-compatible errno mapping).
- On failure batches, component logs may include **`ESP_DNS_DOT`** / **`ESP_DNS_DOH`** errors (e.g. failed to extract IP).
- Summary ends with **`Issue 1 reproduced`** and/or **`Issue 4 reproduced`** when at least one lookup failed in that mode.

If you see **no failures**, increase **`NUM_WORKERS`** and **`ITERATIONS_PER_TASK`** and run again.

## Troubleshooting

- **Network**: configure Wi-Fi/Ethernet credentials in **net_connect** options in `menuconfig`.
- **Certificate**: ensure certificate bundle options in `sdkconfig.defaults` match your IDF version (`menuconfig` → mbedTLS → certificate bundle). If you get `No matching trusted root certificate found` with Cloudflare or Google, make sure `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY=y` is enabled in your configuration (`menuconfig` -> `Component config` -> `mbedTLS` -> `Certificate Bundle` -> `Support cross-signed certificate verification`).
