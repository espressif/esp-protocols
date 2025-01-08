# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# This script replaces the `@pytest` decorators in the test files
# based on the value of the `config` parameter in the `@pytest` decorator.
# to reuse mqtt test cases for mosquitto broker.

import re
import sys


def replace_decorators(in_file: str, out_file: str) -> None:
    with open(in_file, 'r') as file:
        content = file.read()

    # we replace config decorators to differentiate between local mosquitto based tests
    pattern = r"@pytest\.mark\.parametrize\(\s*'config'\s*,\s*\[\s*'(.*?)'\s*\]\s*,.*\)"

    def replacement(match):
        config_value = match.group(1)
        if config_value == 'local_broker':
            return '@pytest.mark.test_mosquitto'
        else:
            return '@pytest.mark.test_mqtt'

    # Replace occurrences
    updated_content = re.sub(pattern, replacement, content)

    with open(out_file, 'w') as file:
        file.write(updated_content)


# Main function to handle arguments
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python replace_decorators.py <in_file> <out_file>')
        sys.exit(1)

    in_file = sys.argv[1]
    out_file = sys.argv[2]
    replace_decorators(in_file, out_file)
    print(f'Replacements completed')
