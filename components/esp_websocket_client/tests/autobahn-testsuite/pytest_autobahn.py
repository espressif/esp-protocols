# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import re
import time

import pytest

# Give up after this long without any serial output. A single performance case
# (9.5.1) may take up to 1000 s, so this must be longer than one case.
IDLE_TIMEOUT = 1200
# Hard cap for the whole suite (performance cases 9.* on slow targets)
TOTAL_TIMEOUT = 4 * 3600


@pytest.mark.supported_targets
@pytest.mark.generic
def test_autobahn(dut, request):
    """
    Monitors ESP32 serial output and detects test completion.
    Equivalent to monitor_serial.py but using pytest-embedded.
    """
    uri = request.config.getoption("--uri")

    # If URI is provided, send it via serial (stdin)
    if uri:
        # Wait for ESP32 to be ready (booting/connecting)
        time.sleep(5)
        dut.write(f'{uri}\n')
        print(f'Sent URI: {uri}')

    # regex patterns
    line_pattern = re.compile(rb'([^\r\n]*)\r?\n')
    progress_pattern = re.compile(rb'Case (\d+)/(\d+)')
    completion_pattern = re.compile(rb'All tests completed\.')

    last_case = None
    start_time = time.time()
    last_activity = start_time

    print(f'\n--- Waiting for ESP32 output (idle timeout {IDLE_TIMEOUT}s, total timeout {TOTAL_TIMEOUT}s) ---\n')

    while True:
        now = time.time()
        if now - start_time >= TOTAL_TIMEOUT:
            pytest.fail(f'Total timeout of {TOTAL_TIMEOUT}s reached without completion (last case: {last_case})')
        if now - last_activity >= IDLE_TIMEOUT:
            pytest.fail(f'No output for {IDLE_TIMEOUT}s, giving up (last case: {last_case})')

        try:
            # Short expect timeout so the idle/total timeouts are checked regularly
            match = dut.expect(line_pattern, timeout=30)
        except Exception:
            continue

        # Any output counts as activity
        last_activity = time.time()
        line = match.group(1)

        if completion_pattern.search(line):
            print('\n✓ Test suite completed successfully!')
            return

        progress = progress_pattern.search(line)
        if progress:
            current_case = int(progress.group(1))
            total_cases = int(progress.group(2))

            if current_case != last_case:
                last_case = current_case
                print(f'Progress: Test case {current_case}/{total_cases} ({100*current_case//total_cases}%)')
