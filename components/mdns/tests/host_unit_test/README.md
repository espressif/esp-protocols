# mDNS host unit tests and fuzzing

This directory builds the mDNS component as a Linux host binary with stubs for ESP-IDF networking. It supports two modes:

- **Unit tests** — Unity/CMock regression tests (ASan/UBSan enabled)
- **Fuzzing** — AFL++ harness that feeds random packets into the receive path

## Prerequisites

- ESP-IDF installed and `IDF_PATH` set (`. $IDF_PATH/export.sh`)
- `libbsd-dev`
- For unit tests: `ruby` (CMock code generation)
- For fuzzing: AFL++ (`afl-cc`, `afl-fuzz`) and `dnslib` (`pip install dnslib`)

From the repository root:

```bash
cd components/mdns/tests/host_unit_test
```

Run `idf.py reconfigure` once before building. This generates `build/config/` headers used by the host build.

## Unit tests

Available test suites (pass one to `-DUNIT_TESTS=`):

| Suite | Description |
|-------|-------------|
| `test_receiver` | Packet receive / parse path |
| `test_sender` | Packet send path |
| `test_browse` | Browse / TXT comparison regressions |

Example — build and run the receiver tests:

```bash
. $IDF_PATH/export.sh
idf.py reconfigure

mkdir -p build2 && cd build2
cmake -DUNIT_TESTS=test_receiver ..
cmake --build .
ctest --extra-verbose
```

Or run the binary directly:

```bash
./mdns_host_unit_test --test
```

Repeat with `-DUNIT_TESTS=test_sender` or `-DUNIT_TESTS=test_browse` in a separate build directory.

## Fuzzer tests

Build the AFL-instrumented harness (no `-DUNIT_TESTS`; uses `main.c`):

```bash
export IDF_PATH=/path/to/esp-idf   # required in the fuzz container

cd input && python generate_cases.py && cd ..

cmake -B build2 -S . -G Ninja -DCMAKE_C_COMPILER=afl-cc
cmake --build build2

afl-fuzz -i input -o out -- build2/mdns_host_unit_test
```

The harness reads packets from stdin and exercises IPv4/IPv6 and port 5353/53 combinations. Crashes are written to `out/default/crashes/`.

### Reproducing a crash

Build the non-unit-test binary with a normal compiler, then pass a crash file:

```bash
cmake -B build2 -S .
cmake --build build2
./build2/mdns_host_unit_test out/default/crashes/id_000000,...
```

With sanitizers enabled, ASan/UBSan report buffer overruns and undefined behaviour directly during unit tests and fuzzing.
