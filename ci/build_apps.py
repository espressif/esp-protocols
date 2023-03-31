# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
This file is used in CI for esp-protocols build tests
"""

import argparse
import os
import sys
from pathlib import Path

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
    args = parser.parse_args()

    IDF_PATH = os.environ['IDF_PATH']

    print(args.paths)
    setup_logging(2)
    apps = find_apps(
        args.paths,
        recursive=True,
        target=args.target,
        build_dir='build_@t_@w',
        config_rules_str=[
            'sdkconfig.ci=default', 'sdkconfig.ci.*=', '=default'
        ],
        build_log_path='build_log.txt',
        size_json_path='size.json',
        check_warnings=True,
        preserve=True,
        manifest_files=[
            str(p) for p in Path('.').glob('**/.build-test-rules.yml')
        ],
        default_build_targets=SUPPORTED_TARGETS,
        manifest_rootpath='.',
    )

    for app in apps:
        print(app)

    sys.exit(
        build_apps(apps,
                   dry_run=False,
                   keep_going=False,
                   ignore_warning_strs=os.environ['EXPECTED_WARNING']
                   if 'EXPECTED_WARNING' in os.environ else None))
