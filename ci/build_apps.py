# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
    args = parser.parse_args()

    IDF_PATH = os.environ['IDF_PATH']

    ignore_warning = os.environ['EXPECTED_WARNING'] if 'EXPECTED_WARNING' in os.environ else None
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
        build_log_path='build_log.txt',
        size_json_path='size.json' if not args.linux else None,
        check_warnings=True,
        preserve=not args.delete,
        manifest_files=args.manifests,
        default_build_targets=SUPPORTED_TARGETS,
        manifest_rootpath='.',
    )

    for app in apps:
        print(app)

    sys.exit(
        build_apps(apps,
                   dry_run=False,
                   keep_going=False,
                   ignore_warning_strs=ignore_warning)
    )
