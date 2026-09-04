# Using Mull with mDNS host unit tests

## What is Mull?

Mull is a **mutation testing** tool. It deliberately changes your compiled code in small ways (e.g. `<` → `<=`, `+` → `-`), then re-runs the tests.

- If a test **fails**, that mutant is *killed* — good: the suite noticed the breakage.
- If tests still **pass**, the mutant *survived* — the suite did not cover that logic well (or the change was harmless).

So Mull measures **test quality**, not whether the original code is correct.

## What it brings here

| Tool | Question it answers |
|------|---------------------|
| Unity + ASan | “Does this path crash or misbehave on real inputs?” |
| clang-tidy ArrayBound | “Could this loop write past an array even if we haven’t hit it yet?” |
| **Mull** | “If this check or operator were wrong, would our tests notice?” |

Use Mull to find weak spots in suites like `test_pcb` (PCB restart / service collection). Do not use it as a substitute for sanitizers or static analysis.

## How it is wired

1. **`mull.yml`** — which mutators to apply, and which files count (`mdns_*.c` only; stubs/mocks/tests excluded).
2. **Build with Mull’s clang plugin** — instruments the binary while compiling.
3. **`mull-runner`** — runs the instrumented `mdns_host_unit_test --test` once per mutant.

Helper script (from this directory):

```bash
# Needs Mull installed for your clang major (see https://github.com/mull-project/mull docs)
./scripts/run_mull.sh test_pcb
```

Other suites work the same: `./scripts/run_mull.sh test_receiver`, etc.

By default the script writes **`mull_reports/mdns_<suite>.sqlite`** (no long list of warnings on the console). Set `MULL_CONSOLE=1` if you also want IDE-style warnings printed.

```bash
sqlite3 mull_reports/mdns_test_receiver.sqlite \
  "SELECT filename, line_number, mutator FROM mutant WHERE status = 2 LIMIT 20;"
```

(`status = 2` means the mutated run still **Passed** → mutant *survived*. Killed mutants are `status <> 2`.)

Overrides if paths differ: `MULL_LLVM_MAJOR`, `MULL_IR_FRONTEND`, `MULL_RUNNER`, `CC`.

## Beginner expectations

- First runs will show **surviving mutants** — normal; treat the score as a signal to add assertions or coverage, not as a hard CI gate yet.
- Some survivors are **equivalent** (the change cannot affect observed behavior). That is OK.
- Prefer running Mull on a focused suite (`test_pcb`) before the whole world — faster feedback.
