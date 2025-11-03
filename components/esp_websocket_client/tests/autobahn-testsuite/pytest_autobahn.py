# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import re
import time

import pytest


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
    progress_pattern = re.compile(rb'Case (\d+)/(\d+)')
    completion_pattern = re.compile(rb'All tests completed\.')

    last_case = None
    start_time = time.time()
    # 40 minutes timeout as in monitor_serial.py
    timeout = 2400

    print('\n--- Waiting for ESP32 output (tests should start shortly) ---\n')

    while (time.time() - start_time) < timeout:
        try:
            # Expect either progress or completion
            # timeout=30 to allow checking total timeout loop regularly
            match = dut.expect([progress_pattern, completion_pattern], timeout=30)
        except Exception:
            # Timeout on single expect call, just continue loop
            continue

        # Check which pattern matched
        # Note: match is a match object from the regex
        if match.re == completion_pattern:
            print('\n✓ Test suite completed successfully!')
            return

        if match.re == progress_pattern:
            current_case = int(match.group(1))
            total_cases = int(match.group(2))

            if current_case != last_case:
                last_case = current_case
                print(f'Progress: Test case {current_case}/{total_cases} ({100*current_case//total_cases}%)')

    pytest.fail(f"Timeout of {timeout}s reached without completion")
