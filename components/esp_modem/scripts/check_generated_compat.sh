#!/bin/bash
# Verify generated command outputs match committed files, ignoring C API
# documentation comments that are intentionally maintained by hand.

set -euo pipefail

script_dir="$(dirname "$(realpath "$0")")"
component_dir="$(realpath "$script_dir/..")"
repo_root="$(git -C "$component_dir" rev-parse --show-toplevel)"

cd "$repo_root"

C_API_HEADER="components/esp_modem/command/include/esp_modem_api.h"
GENERATED_PREFIX="components/esp_modem/"

strip_comments() {
    python3 - <<'PY'
import re
import sys

text = sys.stdin.read()
pattern = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)
lines = [line.rstrip() for line in re.sub(pattern, "", text).splitlines()]
print("\n".join(line for line in lines if line.strip()))
PY
}

changed_files="$(git diff --name-only -- "$GENERATED_PREFIX")"
if [ -z "$changed_files" ]; then
    echo "Generated commands are up to date"
    exit 0
fi

echo "Changed files after generation:"
echo "$changed_files"

other_changes="$(echo "$changed_files" | grep -v "^${C_API_HEADER}$" || true)"
if [ -n "$other_changes" ]; then
    echo "Unexpected changes in generated files:"
    git diff -- $other_changes
    exit 1
fi

committed="$(git show "HEAD:${C_API_HEADER}" | strip_comments)"
generated="$(strip_comments < "${C_API_HEADER}")"

if [ "$committed" = "$generated" ]; then
    echo "C API header differs only in comments (ignored)"
    exit 0
fi

echo "C API header has non-comment differences:"
diff -u <(printf '%s\n' "$committed") <(printf '%s\n' "$generated") || true
exit 1
