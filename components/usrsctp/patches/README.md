# Patches Applied to usrsctp

Patches applied on top of the canonical
[sctplab/usrsctp](https://github.com/sctplab/usrsctp) submodule at build
configure time. The submodule pins commit `2e1ab10` (`Update cifuzz.yml`),
which is the immediate pre-patch base — patches apply cleanly without
adaptation.

The component CMakeLists applies them automatically (idempotent: a
dry-run check skips already-applied patches).

## Ledger

| # | Patch | Category | Notes |
|---|-------|----------|-------|
| 0001 | Added LWIP support | aligned (candidate) | Routes usrsctp's userspace socket layer over lwIP instead of POSIX BSD sockets. Required for ESP-IDF / FreeRTOS targets. Upstream candidate: needs sctplab review. |
| 0002 | Fix overflow error: Allocate for larger sin6 structure | bug-fix | Stack-overflow guard around `sin6` allocations; small + safe; aligned-with-upstream. |
| 0003 | ESP_PLATFORM: Added support for thread_safe netif API implementations | ESP-IDF specific | Uses ESP-IDF's thread-safe lwIP netif APIs (`esp_netif_*` analogues) where canonical usrsctp would call `getifaddrs(3)`. Stays. |
| 0004 | feat(esp): allocate usrsctp thread stacks from SPIRAM | ESP-IDF specific | Targets memory-constrained chips (P4-EYE, C5/C6) by routing usrsctp's internal pthreads' stacks to SPIRAM. Stays. |
| 0005 | Fix non-LWIP / Linux-target buildability in 'Added LWIP support' | bug-fix | Compiles cleanly for IDF Linux target (CONFIG_IDF_TARGET_LINUX) where lwIP isn't pulled in. Should fold back into 0001 upstream. |

## Future upstream sync

The submodule pin (`2e1ab10`) is older than `sctplab/master`. To pull in
newer upstream fixes:

1. `git fetch origin && git checkout origin/master` inside `usrsctp/`.
2. Try `git am --3way --keep-cr ../patches/*.patch` and resolve any
   conflicts (most likely in 0001 since it touches a lot of files).
3. Re-generate patches via `git format-patch <new-base>..HEAD -o ../patches/`.
4. Bump the recorded submodule SHA in the parent and update this ledger.

## Upstream submission plan

- **LWIP support** (this directory's `0001` + `0005` squashed):
  Will be submitted as a fresh PR on `sctplab/usrsctp`, superseding the
  stale [#583](https://github.com/sctplab/usrsctp/pull/583) by
  `@ycyang1229` (opened 2021-05-03, last activity from us in 2021).
  Attribution: `Original-Author: ycyang1229` + `Co-authored-by: Vikram
  Dattu <vikram.dattu@espressif.com>`. The upstream version will be
  strictly LWIP-only (no `ESP_PLATFORM` ifdef, no IDF-specific glue).
- **sin6 overflow fix** (this directory's `0002`): independent bug
  fix, submit as a separate small PR on `sctplab/usrsctp`.
- **ESP-IDF-specific patches** (`0003` thread-safe netif, `0004` SPIRAM
  stacks): stay downstream — not generic enough for upstream.

When the upstream PRs land, drop the corresponding local patches and
bump the submodule SHA.
