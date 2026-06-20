# Changelog

## [0.9.5~1](https://github.com/espressif/esp-protocols/commits/usrsctp-v0.9.5_1)

### Features

- First release. Wraps [sctplab/usrsctp](https://github.com/sctplab/usrsctp) as an ESP-IDF component. Submodule pinned at commit `2e1ab10`.
- Five ESP-IDF-specific patches applied at build-configure time — see [`patches/README.md`](patches/README.md). The biggest one (0001) is the LWIP routing layer for usrsctp's userspace `conn` transport; upstream PR is in flight at [sctplab/usrsctp#743](https://github.com/sctplab/usrsctp/pull/743) and will fold in once it lands.
- Embedded smoke test (`test_apps/`) on esp32 + esp32c3; host-side socket-lifecycle test (`host_test/`) on the IDF Linux target.
