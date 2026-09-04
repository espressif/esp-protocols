# mDNS AFL++ fuzzing notes

Checklist from `fuzzing_info.txt` (AFL++ effectiveness tips), applied to
`components/mdns/tests/host_unit_test`. Each item states what we do here and why.

Preferred compiler: `afl-clang-fast` / `afl-cc` (LLVM persistent + deferred forkserver).

Required ASan env when running `afl-fuzz` or reproducing crashes:

```bash
export ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:symbolize=0:detect_stack_use_after_return=1:max_malloc_fill_size=$((1<<30))"
export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1"
```

## (1) Configure sanitizers and assertions

CMake already links the fuzz binary with `-fsanitize=address,undefined`, `-g3`, and frame pointers. That matches the tip’s default of ASan+UBSan for C targets. Runtime sensitivity (abort-on-error, use-after-return, full malloc fill) must be set via `ASAN_OPTIONS` / `UBSAN_OPTIONS` as above—AFL++ will refuse to run if abort/halt are missing when ASan is linked. We do not enable `_GLIBCXX_DEBUG` because this harness is pure C.

## (2) Prefer the persistent mode

`main.c` uses `__AFL_LOOP(10000)` with deferred init (`__AFL_FUZZ_INIT` / `__AFL_INIT`) and the shared-memory testcase buffer when built with `afl-clang-fast`. That avoids re-execing the process per input and is roughly an order of magnitude faster than plain fork+exec. Crash reproduction builds (normal `cc`) skip these macros and take a crash file path instead.

## (3) Include source files, not header files

Not adopted for this tree. The host harness already has a deliberate multi-file CMake build (mdns sources + Linux stubs + generated `sdkconfig` headers), and pulling `.c` files into one TU would fight IDF’s include layout without unlocking static-only APIs we need—the public receive entry is already `mdns_packet_push` → `mdns_priv_receive_action`. Keep the existing build; use stubs for symbol substitution instead of `#include "foo.c"`.

## (4) Isolate fuzz tests from each other

Persistent mode reuses process-global mdns state (responder, queries/browsers, cache). After each input we call `mdns_priv_cache_clear()` so cache entries from a previous packet cannot change coverage or hide bugs on the next one. Full deinit/init every iteration would be stronger isolation but much slower; AFL still recycles the process periodically, which resets remaining globals.

## (5) Do not test directly on the fuzz test buffer

The harness `realloc`s an exact-size `input_copy` and `memcpy`s the AFL buffer into it before calling `send_packet`. That way ASan can catch reads past the logical length—AFL’s SHM backing store is larger than `len`, so passing `buf` straight through would miss overflows. The networking stub also copies into a `malloc(len)` packet payload; the harness copy is intentional defense in depth for tip (5).

## (6) Don’t bother freeing memory

Partially followed. We do not free the persistent `input_copy` between iterations (only grow via `realloc`), and we avoid noisy per-packet teardown. We *do* clear the mdns cache each iteration (see (4)) because unbounded cache growth would OOM or make crashes unreproducible—tip (6)’s “skip destructors” advice loses to isolation for this stateful protocol stack.

## (7) Use a memory file descriptor to back named paths

Not applicable. The fuzz surface is an in-memory API (`mdns_packet_push(addr, port, if, data, len)`), not a pathname/`open` interface. No `memfd_create` / `/proc/self/fd/N` wrapper is required.

## (8) Configure the target for smaller buffers

Not changed in production headers. The harness explicitly caps every path (AFL SHM, legacy stdin, crash repro) at `FUZZ_MAX_INPUT_LEN` (`MDNS_MAX_PACKET_SIZE` / 1460) so fuzzing and reproduction stay aligned on the realistic UDP/mDNS bound. Shrinking that further mostly invents artificial TX-path limits rather than exposing RX bugs faster. Name/TXT limits (`MDNS_NAME_MAX_LEN`, `MDNS_TXT_MAX_LEN`) could be reduced under a future fuzz-only compile switch if corpus depth stalls on those paths; document-only for now.

---

## Harness split: `receive` vs `browse`

Unit tests already isolate receiver vs browser with different mocks. Fuzzing benefits from the same split so AFL’s coverage map is not dominated by whichever path mutates first:

| `-DFUZZ_TARGET=` | Context | What it stresses |
|------------------|---------|------------------|
| `receive` (default) | Responder + async queries | Parse, questions/answers, query matching |
| `browse` | Responder + `mdns_browse_new` | Browse/cache/TXT comparison path |

Build separate output dirs and run two `afl-fuzz` instances (or one after the other). Each input still hits a single IPv4/IPv6 × port 53/5353 combo derived from a XOR of the packet bytes (full DNS payload preserved for the seed corpus).

```bash
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
