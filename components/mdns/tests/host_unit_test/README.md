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

See [fuzzing.md](fuzzing.md) for the AFL++ effectiveness checklist and rationale.

Build separate harnesses for **receive** (parse/query) and **browse** (cache/TXT). Prefer `afl-clang-fast`. Set sanitizer options before fuzzing or reproducing:

```bash
export IDF_PATH=/path/to/esp-idf   # required in the fuzz container
export ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:symbolize=0:detect_stack_use_after_return=1:max_malloc_fill_size=$((1<<30))"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1"

cd input && python generate_cases.py && cd ..

cmake -B build_fuzz_recv -S . -G Ninja \
  -DCMAKE_C_COMPILER=afl-clang-fast -DFUZZ_TARGET=receive
cmake --build build_fuzz_recv
afl-fuzz -i input -o out_recv -- build_fuzz_recv/mdns_host_unit_test

cmake -B build_fuzz_browse -S . -G Ninja \
  -DCMAKE_C_COMPILER=afl-clang-fast -DFUZZ_TARGET=browse
cmake --build build_fuzz_browse
afl-fuzz -i input -o out_browse -- build_fuzz_browse/mdns_host_unit_test
```

Each execution feeds one packet (exact-size copy) into `mdns_packet_push`, with IPv4/IPv6 and port 53/5353 derived from the input. Crashes land under `out_*/default/crashes/`.

### Reproducing a crash

Build the same `FUZZ_TARGET` with a normal compiler, keep `ASAN_OPTIONS` set, then pass the crash file:

```bash
cmake -B build_repro -S . -G Ninja -DFUZZ_TARGET=receive
cmake --build build_repro
./build_repro/mdns_host_unit_test out_recv/default/crashes/id_000000,...
```

With sanitizers enabled, ASan/UBSan report buffer overruns and undefined behaviour directly during unit tests and fuzzing.
