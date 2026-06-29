#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Static analysis check: detect potential self-deadlock paths in mDNS locking.

The mDNS component uses a single service lock (MDNS_SERVICE_LOCK /
mdns_priv_service_lock).  Public API functions acquire this lock, then call
internal (_priv_) helpers.  The service_task() loop also holds the lock while
dispatching actions.

A self-deadlock occurs when code *already running under the lock* calls a
public API function that tries to re-acquire the same lock.  This script
builds a call graph from tree-sitter parses of every .c file in the mDNS
component and checks for such paths.

Exit code 0  = no issues found
Exit code 1  = potential deadlock path(s) detected

Usage:
    python3 check_lock_reentry.py [path/to/mdns/component]

Requires: pip install tree-sitter tree-sitter-c
"""

from __future__ import annotations

import sys
from pathlib import Path

try:
    import tree_sitter_c as tsc
    from tree_sitter import Language, Parser
except ImportError:
    print("ERROR: tree-sitter and tree-sitter-c are required.")
    print("  pip install tree-sitter tree-sitter-c")
    sys.exit(2)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Tokens that acquire the service lock
LOCK_FUNCTIONS = frozenset({
    "mdns_priv_service_lock",
    "MDNS_SERVICE_LOCK",
})

# Tokens that release the service lock
UNLOCK_FUNCTIONS = frozenset({
    "mdns_priv_service_unlock",
    "MDNS_SERVICE_UNLOCK",
})

# Functions whose *sole purpose* is to wrap lock or unlock -- these are
# excluded from the lock/unlock balance check because they intentionally
# contain only one side of the pair.
LOCK_WRAPPER_FUNCTIONS = frozenset({
    "mdns_priv_service_lock",
    "mdns_priv_service_unlock",
})

# ---------------------------------------------------------------------------
# Tree-sitter helpers
# ---------------------------------------------------------------------------

C_LANG = Language(tsc.language())


def parse_file(parser: Parser, path: Path) -> "tree_sitter.Tree":
    source = path.read_bytes()
    return parser.parse(source), source


def iter_call_expressions(node):
    """Yield all call_expression nodes under *node*."""
    if node.type == "call_expression":
        yield node
    for child in node.children:
        yield from iter_call_expressions(child)


def get_callee_name(call_node) -> str | None:
    """Return the function name from a call_expression, or None if indirect."""
    fn = call_node.child_by_field_name("function")
    if fn is None:
        # tree-sitter-c: first child of call_expression is the function
        for child in call_node.children:
            if child.type == "identifier":
                return child.text.decode()
            break
    elif fn.type == "identifier":
        return fn.text.decode()
    return None


def extract_functions(tree, source: bytes, filepath: str):
    """
    Return a dict  { func_name: FuncInfo }  for every function defined in the
    translation unit.

    FuncInfo has:
      - .calls: set of callee names
      - .acquires_lock: True if the function calls a LOCK_FUNCTION
      - .releases_lock: True if the function calls an UNLOCK_FUNCTION
      - .filepath: source file path
      - .line: definition line number (1-based)
      - .locked_calls: set of callee names called *between* lock and unlock
    """

    class FuncInfo:
        __slots__ = (
            "name", "calls", "acquires_lock", "releases_lock",
            "filepath", "line", "locked_calls",
        )

        def __init__(self, name, filepath, line):
            self.name = name
            self.calls: set[str] = set()
            self.acquires_lock = False
            self.releases_lock = False
            self.filepath = filepath
            self.line = line
            self.locked_calls: set[str] = set()

        def __repr__(self):
            return f"FuncInfo({self.name}, lock={self.acquires_lock})"

    funcs: dict[str, FuncInfo] = {}

    for node in tree.root_node.children:
        if node.type != "function_definition":
            continue

        # Get function name
        decl = node.child_by_field_name("declarator")
        if decl is None:
            continue
        name_node = decl
        while name_node.type != "identifier":
            found = False
            for child in name_node.children:
                if child.type in ("identifier", "function_declarator"):
                    name_node = child
                    found = True
                    break
            if not found:
                break
        if name_node.type != "identifier":
            continue

        func_name = name_node.text.decode()
        line = node.start_point[0] + 1  # 1-based
        info = FuncInfo(func_name, filepath, line)

        body = node.child_by_field_name("body")
        if body is None:
            continue

        # Walk all call expressions in the body
        # Also track lock/unlock regions for locked_calls
        _extract_calls_and_lock_regions(body, info)

        funcs[func_name] = info

    return funcs


def _extract_calls_and_lock_regions(body_node, info):
    """
    Walk the compound_statement and extract:
      - info.calls: all callee names
      - info.acquires_lock / releases_lock
      - info.locked_calls: callees between lock() and unlock()

    We use a simple linear scan of top-level statements. This correctly handles
    the common pattern:

        lock();
        foo();          // <-- locked_call
        bar();          // <-- locked_call
        unlock();

    It won't perfectly model all control flow (goto, early returns), but for
    this codebase the linear pattern is the norm.
    """
    in_locked_region = False

    for stmt in body_node.children:
        calls_in_stmt = list(iter_call_expressions(stmt))

        for call in calls_in_stmt:
            callee = get_callee_name(call)
            if callee is None:
                continue

            info.calls.add(callee)

            if callee in LOCK_FUNCTIONS:
                info.acquires_lock = True
                in_locked_region = True
                continue

            if callee in UNLOCK_FUNCTIONS:
                info.releases_lock = True
                in_locked_region = False
                continue

            if in_locked_region:
                info.locked_calls.add(callee)


# ---------------------------------------------------------------------------
# Call-graph analysis
# ---------------------------------------------------------------------------

def find_reentry_paths(
    all_funcs: dict[str, "FuncInfo"],
) -> list[tuple[list[str], str]]:
    """
    For each function that directly holds the lock (has locked_calls), walk the
    call graph starting from every locked_call.  If we reach a function that
    acquires the lock, report it.

    Returns a list of (call_chain, lock_acquirer) tuples.
    """
    # Set of functions that acquire the lock
    lock_acquirers = {
        name for name, f in all_funcs.items() if f.acquires_lock
    }

    problems: list[tuple[list[str], str, str]] = []

    for holder_name, holder in all_funcs.items():
        if not holder.locked_calls:
            continue

        # BFS from each locked_call
        for start_callee in holder.locked_calls:
            visited: set[str] = set()
            # queue entries: (current_func, path_so_far)
            queue: list[tuple[str, list[str]]] = [
                (start_callee, [holder_name, start_callee])
            ]

            while queue:
                current, path = queue.pop(0)

                if current in visited:
                    continue
                visited.add(current)

                if current in lock_acquirers:
                    problems.append((
                        path,
                        current,
                        f"{holder.filepath}:{holder.line}",
                    ))
                    break  # found one path from this start_callee

                if current in all_funcs:
                    for callee in all_funcs[current].calls:
                        if callee not in visited:
                            queue.append((callee, path + [callee]))

    return problems


# ---------------------------------------------------------------------------
# Lock / unlock balance check
# ---------------------------------------------------------------------------

def check_lock_balance(all_funcs: dict[str, "FuncInfo"]) -> list[str]:
    """
    Check that every function which calls lock() also calls unlock()
    and vice versa.  Known wrapper functions are excluded.
    """
    errors = []
    for name, f in all_funcs.items():
        if name in LOCK_WRAPPER_FUNCTIONS:
            continue
        if f.acquires_lock and not f.releases_lock:
            errors.append(
                f"  {f.filepath}:{f.line}: {name}() calls lock but never unlock"
            )
        if f.releases_lock and not f.acquires_lock:
            errors.append(
                f"  {f.filepath}:{f.line}: {name}() calls unlock but never lock"
            )
    return errors


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) > 1:
        mdns_dir = Path(sys.argv[1])
    else:
        mdns_dir = Path(__file__).resolve().parent.parent

    if not mdns_dir.is_dir():
        print(f"ERROR: directory not found: {mdns_dir}")
        sys.exit(2)

    c_files = sorted(mdns_dir.glob("*.c"))
    if not c_files:
        print(f"ERROR: no .c files found in {mdns_dir}")
        sys.exit(2)

    parser = Parser(C_LANG)

    # Parse all files and collect function info
    all_funcs: dict[str, "FuncInfo"] = {}
    for cfile in c_files:
        tree, source = parse_file(parser, cfile)
        funcs = extract_functions(tree, source, str(cfile.relative_to(mdns_dir.parent.parent)))
        # Merge (later files overwrite on name collision, but function names
        # are unique across the mDNS component in practice)
        all_funcs.update(funcs)

    print(f"Parsed {len(c_files)} files, found {len(all_funcs)} functions")

    exit_code = 0

    # --- Check 1: lock/unlock balance ---
    balance_errors = check_lock_balance(all_funcs)
    if balance_errors:
        exit_code = 1
        print(f"\nERROR: lock/unlock balance issues ({len(balance_errors)}):")
        for e in balance_errors:
            print(e)

    # --- Check 2: re-entry paths ---
    problems = find_reentry_paths(all_funcs)
    if problems:
        exit_code = 1
        # De-duplicate by (holder, acquirer)
        seen = set()
        unique_problems = []
        for path, acquirer, location in problems:
            key = (path[0], acquirer)
            if key not in seen:
                seen.add(key)
                unique_problems.append((path, acquirer, location))

        print(f"\nERROR: potential self-deadlock path(s) found ({len(unique_problems)}):\n")
        for path, acquirer, location in unique_problems:
            chain = " -> ".join(path)
            print(f"  {location}")
            print(f"    {chain}")
            acq = all_funcs.get(acquirer)
            if acq:
                print(f"    ^^^ {acquirer}() re-acquires the service lock"
                      f" at {acq.filepath}:{acq.line}")
            print()
    else:
        print("\nNo self-deadlock paths found.")

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
