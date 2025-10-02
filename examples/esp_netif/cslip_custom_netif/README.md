| Supported Targets | ESP32 | ESP32-C3 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# CSLIP device client (initial)

This example mirrors the SLIP custom netif example, but prepares for CSLIP (Van Jacobson TCP/IP header compression over SLIP). In this initial version, the data path behaves like plain SLIP while exposing a minimal CSLIP-facing API and structure so compression can be added incrementally.

It demonstrates creating a custom network interface and attaching it to `esp_netif` with an external component (`cslip_modem`) similar to the `slip_modem` used in the SLIP example.

Notes:
- Compression is not yet implemented; packets are forwarded as standard SLIP.
- The structure allows later integration of VJ compression/decompression.

## How to use

Hardware and setup are identical to the SLIP example. You can point a Linux host to the serial port and use `slattach` with `-p slip` for initial testing.

Build and flash:
- idf.py set-target <chip>
- idf.py menuconfig (configure Example Configuration)
- idf.py -p <PORT> flash monitor

Troubleshooting and behavior should match the SLIP example until compression is added.
