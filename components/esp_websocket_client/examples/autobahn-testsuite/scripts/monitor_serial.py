#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""
Serial monitor script for Autobahn test suite.
Monitors ESP32 serial output and detects test completion.
"""

import argparse
import re
import sys
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            'Monitor ESP32 serial output for Autobahn test completion'
        ),
    )

    parser.add_argument(
        '--port',
        '-p',
        default='/dev/ttyUSB0',
        help='Serial port (default: /dev/ttyUSB0)',
    )
    parser.add_argument(
        '--baud',
        '-b',
        type=int,
        default=115200,
        help='Baud rate (default: 115200)',
    )
    parser.add_argument(
        '--timeout',
        '-t',
        type=int,
        default=2400,
        help='Timeout in seconds (default: 2400 = 40 minutes)',
    )
    parser.add_argument(
        '--completion-pattern',
        '-c',
        default=r'All tests completed\.',
        help=(
            'Regex pattern to detect completion '
            '(default: "All tests completed.")'
        ),
    )
    parser.add_argument(
        '--uri',
        '-u',
        default=None,
        help=(
            'Server URI to send via serial (stdin). If provided, will send this '
            'URI after opening port.'
        ),
    )

    args = parser.parse_args()

    port = args.port
    timeout_seconds = args.timeout
    completion_pattern = re.compile(args.completion_pattern)

    print(f'Opening serial port: {port} at {args.baud} baud')
    try:
        ser = serial.Serial(port, args.baud, timeout=1)
        print('Serial port opened successfully')

        try:
            # If URI is provided, send it via serial (stdin)
            if args.uri:
                print(f'Sending server URI: {args.uri}')
                print('Waiting for ESP32 to be ready...')
                # Wait longer for ESP32 to boot and initialize
                time.sleep(5)
                # Send URI followed by newline
                ser.write(f'{args.uri}\n'.encode('utf-8'))
                ser.flush()
                print('URI sent successfully')
                print('Note: If ESP32 is not configured with CONFIG_WEBSOCKET_URI_FROM_STDIN,')
                print('      it will use the URI from sdkconfig instead.')

            buffer = ''
            start_time = time.time()
            last_activity_time = start_time
            last_status_time = start_time
            test_case_pattern = re.compile(r'Case (\d+)/(\d+)')
            last_test_case = None
            initial_wait_time = 10  # Wait up to 10 seconds for initial output
            exit_code = 0

            print('\n--- Waiting for ESP32 output (tests should start shortly) ---\n')
            print('If no output appears, the ESP32 may be:')
            print('  - Still booting (wait a few more seconds)')
            print('  - Connecting to WiFi')
            print('  - Not configured to read URI from stdin')
            print()

            while True:
                elapsed = time.time() - start_time
                if elapsed > timeout_seconds:
                    print(
                        '\n⚠ Timeout after '
                        f'{timeout_seconds}s - tests may still be running',
                    )
                    exit_code = 1
                    break

                if ser.in_waiting:
                    data = ser.read(ser.in_waiting).decode(
                        'utf-8',
                        errors='ignore',
                    )
                    buffer += data
                    sys.stdout.write(data)
                    sys.stdout.flush()
                    last_activity_time = time.time()

                    # Track test case progress
                    test_match = test_case_pattern.search(data)
                    if test_match:
                        current_case = int(test_match.group(1))
                        total_cases = int(test_match.group(2))
                        if current_case != last_test_case:
                            last_test_case = current_case
                            print(
                                f'\n[Progress: Test case {current_case}/{total_cases} '
                                f'({100*current_case//total_cases}%)]',
                                file=sys.stderr
                            )

                    # Check for completion message
                    if completion_pattern.search(buffer):
                        print('\n✓ Test suite completed successfully!')
                        time.sleep(5)  # Wait a bit more for any final output
                        exit_code = 0
                        break
                else:
                    # Show periodic status if no activity for a while
                    time_since_activity = time.time() - last_activity_time
                    time_since_status = time.time() - last_status_time

                    # More frequent updates during initial wait
                    status_interval = 10 if elapsed < initial_wait_time else 30

                    if time_since_activity > status_interval and time_since_status > status_interval:
                        elapsed_min = int(elapsed // 60)
                        elapsed_sec = int(elapsed % 60)
                        if elapsed < initial_wait_time:
                            print(
                                f'\n[Status: Waiting for initial output... '
                                f'({elapsed_sec}s elapsed)]',
                                file=sys.stderr
                            )
                        else:
                            print(
                                f'\n[Status: Waiting for output... '
                                f'({elapsed_min}m {elapsed_sec}s elapsed)]',
                                file=sys.stderr
                            )
                        last_status_time = time.time()

                        # After initial wait, suggest checking configuration
                        if elapsed >= initial_wait_time and last_activity_time == start_time:
                            print(
                                '\n[Warning: No output received yet. Possible issues:]',
                                file=sys.stderr
                            )
                            print(
                                '  - ESP32 not configured with CONFIG_WEBSOCKET_URI_FROM_STDIN',
                                file=sys.stderr
                            )
                            print(
                                '  - WiFi not connected (check credentials)',
                                file=sys.stderr
                            )
                            print(
                                '  - Wrong serial port or baud rate',
                                file=sys.stderr
                            )
                            print(
                                '  - ESP32 firmware not flashed correctly',
                                file=sys.stderr
                            )
                            print(
                                '\n  Try: Press RESET button on ESP32 or check serial connection',
                                file=sys.stderr
                            )

                time.sleep(0.1)

        finally:
            # Ensure serial port is closed even if an exception occurs
            if ser.is_open:
                ser.close()

        # Exit with appropriate code after cleanup
        sys.exit(exit_code)
    except serial.SerialException as e:
        print(f'Error opening serial port: {e}', file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print('\nInterrupted by user')
        sys.exit(1)


if __name__ == '__main__':
    main()
