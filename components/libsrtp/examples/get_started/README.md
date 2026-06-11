# libsrtp — get_started

Minimal sanity-check application for [`libsrtp`](https://github.com/espressif/esp-protocols/tree/master/components/libsrtp). Initializes libsrtp via `srtp_init()` and shuts it down. Used by the repo's CI to verify the component compiles and links across supported targets; also useful as a copy-paste starting point.

## Build

```bash
idf.py set-target esp32p4   # or esp32, esp32s3, esp32c3, esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

The example consumes the parent component via `override_path: ../../..` in `main/idf_component.yml`, so it always tracks the in-tree source. Replace with a registry version to consume the published component instead:

```yaml
dependencies:
  espressif/libsrtp: "^2.8.0"
```
