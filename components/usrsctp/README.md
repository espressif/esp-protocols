# usrsctp

[![Component Registry](https://components.espressif.com/components/espressif/usrsctp/badge.svg)](https://components.espressif.com/components/espressif/usrsctp)

Userspace [SCTP](https://datatracker.ietf.org/doc/html/rfc4960) stack ([usrsctp](https://github.com/sctplab/usrsctp)) port for ESP-IDF.

Used for WebRTC data channels and other SCTP-based protocols on ESP32-series chips.

## Features

- Canonical [sctplab/usrsctp](https://github.com/sctplab/usrsctp) submodule pinned at `2e1ab10`
- ESP-IDF specific patches applied at configure time (see [`patches/README.md`](patches/README.md)):
  - LWIP routing for the userspace conn layer
  - Thread-safe netif API support
  - sin6 overflow fix
  - SPIRAM-backed worker-thread stacks (Kconfig-gated)
  - IDF Linux-target buildability

## Add to a project

```bash
idf.py add-dependency "vikramdattu/usrsctp^1.0.0"
```

Or in your project's `main/idf_component.yml`:

```yaml
dependencies:
  vikramdattu/usrsctp: "^1.0.0"
```

## Supported targets

| Target  | Status |
|---------|--------|
| esp32, esp32s2, esp32s3 | ✅ |
| esp32c3, esp32c5, esp32c6 | ✅ |
| esp32p4 | ✅ |
| linux (IDF Linux target) | ✅ (host build + `host_test/`) |

Requires ESP-IDF ≥ 5.4.

## Tests

- `test_apps/` — embedded Unity smoke (`usrsctp_init` / `usrsctp_finish`).
  Run with `pytest_usrsctp.py` against `esp32` / `esp32c3`.
- `host_test/` — IDF Linux-target binary that exercises socket creation /
  close lifecycle. Run with `pytest_usrsctp_linux.py`.

## Source

This wrapper bundles canonical [sctplab/usrsctp](https://github.com/sctplab/usrsctp) as a submodule, pinned at commit `2e1ab10`. ESP-specific patches live under [`patches/`](patches/) and are applied at configure time.

## License

BSD-3-Clause (see `LICENSE.md`), matching upstream usrsctp.
