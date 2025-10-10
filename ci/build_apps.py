# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
This file is used in CI for esp-protocols build tests
"""

import argparse
import os
import sys

from idf_build_apps import build_apps, find_apps, setup_logging
from idf_build_apps.constants import SUPPORTED_TARGETS

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Build all projects',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument('paths', nargs='+', help='Paths to the apps to build.')
    parser.add_argument(
        '-v',
        '--verbose',
        action='count',
        help='Increase the LOGGER level of the script. Can be specified multiple times.',
    )
    parser.add_argument(
        '-t',
        '--target',
        default='all',
        help='Build apps for given target',
    )
    parser.add_argument('-r', '--rules', nargs='*', default=['sdkconfig.ci=default', 'sdkconfig.ci.*=', '=default'], help='Rules how to treat configs')
    parser.add_argument('-m', '--manifests', nargs='*', default=[], help='list of manifest files')
    parser.add_argument('-d', '--delete', action='store_true', help='Delete build artifacts')
    parser.add_argument('-c', '--recursive', action='store_true', help='Build recursively')
    parser.add_argument('-l', '--linux', action='store_true', help='Include linux build (dont check warnings)')
    parser.add_argument('--preserve-all', action='store_true', help='Preserve the binaries for all apps when specified.')
    parser.add_argument('--pytest-apps', action='store_true', help='Only build apps required by pytest scripts.')
    args = parser.parse_args()

    IDF_PATH = os.environ['IDF_PATH']

    # Compose the ignore warning strings from the global list and from the environment
    ignore_warning_file = os.path.join(os.path.dirname(os.path.realpath(__file__)),'ignore_build_warnings.txt')
    ignore_warning = open(ignore_warning_file).read().rstrip('\n').split('\n')
    if 'EXPECTED_WARNING' in os.environ:
        ignore_warning += os.environ['EXPECTED_WARNING'].split('\n')
    if args.linux:
        SUPPORTED_TARGETS.append('linux')
        ignore_warning = 'warning: '  # Ignore all common warnings on linux builds
    setup_logging(2)
    apps = find_apps(
        args.paths,
        recursive=args.recursive,
        target=args.target,
        build_dir='build_@t_@w',
        config_rules_str=args.rules,
        build_log_filename='build_log.txt',
        size_json_filename=None,
        check_warnings=True,
        manifest_files=args.manifests,
        default_build_targets=SUPPORTED_TARGETS,
        manifest_rootpath='.',
    )

    sys.exit(
        build_apps(apps,
                   verbose=2,
                   dry_run=False,
                   keep_going=False,
                   no_preserve=args.delete,
                   ignore_warning_strs=ignore_warning)
    )
