# usrsctp — get_started

Minimal sanity-check application for [`usrsctp`](https://github.com/vikramdattu/usrsctp). Calls `usrsctp_init()` / `usrsctp_finish()`. Used by the repo's CI to verify the component compiles and links across supported targets; also a copy-paste starting point.

## Build

```bash
idf.py set-target esp32p4   # or esp32, esp32s3, esp32c3, esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

The example consumes the parent component via `override_path: ../../..` in `main/idf_component.yml`, so it always tracks the in-tree source. Replace with a registry version to consume the published component instead:

```yaml
dependencies:
  vikramdattu/usrsctp: "^1.0.0"
```
