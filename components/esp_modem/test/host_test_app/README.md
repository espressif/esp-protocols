# Host Test App

End-to-end host tests for esp_modem using two Linux processes connected over a TCP socket:

- **test_app** – IDF Linux application (Catch2) that exercises esp_modem AT commands via a VFS socket DTE.
- **modem_sim** – Standalone TCP server that emulates a SIM7600-like modem, responding to AT commands.

When `LWIP_PATH` is not set, `esp_netif_linux` builds automatically as a stub (no lwIP download needed).

## Build & Run

```bash
# 1. Build the modem simulator (plain CMake, no IDF)
cmake -S modem_sim -B modem_sim/build && cmake --build modem_sim/build

# 2. Build the test app (IDF, Linux target)
source $IDF_PATH/export.sh
idf.py --preview set-target linux
idf.py build

# 3. Run both (simulator + tests)
./run_test.sh
```

`MODEM_SIM_PORT` (default `10000`) and `TEST_TIMEOUT` (default `30`s) can be overridden via environment variables.
