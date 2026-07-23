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
| `test_receiver` | Packet receive / parse path (`mdns_pcb` + `mdns_send` mocked) |
| `test_sender` | Packet send path (`mdns_pcb` mocked) |
| `test_browse` | Browse / TXT comparison regressions |
| `test_pcb` | Real PCB restart/probe service collection (`mdns_send` mocked) |

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

Repeat with `-DUNIT_TESTS=test_sender`, `test_browse`, or `test_pcb` in a separate build directory.

### What unit tests catch (and what they do not)

ASan/UBSan already catch **definite** out-of-bounds accesses and UB when a buggy path runs. That is why `test_pcb` exists: older suites mocked `mdns_pcb`, so `restart_pcb()` never executed under sanitizers.

They do **not** replace static analysis for *potential* ArrayBound issues (e.g. clang-tidy `clang-analyzer-security.ArrayBound` on count-then-fill VLA loops). Those warnings fire when the analyzer cannot prove a fill loop stays in bounds — often without a concrete ASan crash on a stable service list. Keep clang-tidy (or similar) in CI for that class of defect.

## Mutation testing (Mull)

[Mull](https://mull-project.com) (https://github.com/mull-project/mull) mutates the compiled binary and re-runs tests. Surviving mutants highlight weak assertions or uncovered logic. It complements ASan: good for “would our tests notice if this `<` became `<=`?”, not for inventing race/list-growth scenarios that static ArrayBound found.

Config: `mull.yml` (scopes mutators to `components/mdns/mdns_*.c`).

```bash
# Install Mull for your clang major (see ext/mull/docs/Installation.rst), then:
./scripts/run_mull.sh test_pcb
```

Override paths if needed: `MULL_LLVM_MAJOR`, `MULL_IR_FRONTEND`, `MULL_RUNNER`, `CC`.

Expect some surviving / equivalent mutants on tight bound checks when the service list length matches the VLA size; use the score as a coverage-quality signal, not a pass/fail gate until the suite is broader.

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
