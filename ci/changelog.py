# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import os
import re

import sh


def main():
    parser = argparse.ArgumentParser(
        description='Generates a change log from cz commits')
    parser.add_argument('component')
    args = parser.parse_args()

    old_ref = os.environ.get('CZ_PRE_CURRENT_TAG_VERSION')
    new_tag = os.environ.get('CZ_PRE_NEW_TAG_VERSION')
    new_ref = os.environ.get('CZ_PRE_NEW_VERSION')
    component = args.component

    root_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

    git = sh.git.bake(_cwd=root_path)

    keys = ['breaking', 'major', 'feat', 'fix', 'update']
    items = {key: [] for key in keys}
    sections = {
        'feat': 'Features',
        'fix': 'Bug Fixes',
        'update': 'Updated',
        'breaking': 'Breaking changes',
        'major': 'Major changes'
    }
    brief_log = git.log('--oneline', '{}..HEAD'.format(old_ref), '--', 'components/' + component, _tty_out=False)
    for oneline in brief_log.splitlines():
        [commit, brief_msg] = oneline.split(' ', 1)
        if 'Merge' in str(brief_msg) or brief_msg.startswith('bump('):  # skip Merge commits and bumps
            continue
        find_title = re.findall(r'^(fix|feat|esp_modem|esp-modem|mdns)(\([^\)]+\))?:\s*(.+)$', str(brief_msg))
        item_type = ''             # default prefix
        item_message = brief_msg   # default body
        if find_title is not None and len(find_title) > 0:  # Use scopes & types
            item_type = find_title[0][0]      # prefix (fix/feat/ci)
            item_message = find_title[0][-1]  # title body

        # Add details in parentheses (commit-id and references from the FOOTER section)
        details = '[{}](https://github.com/espressif/esp-protocols/commit/{})'.format(commit, commit)
        msg_details = git.show('-s', commit, _tty_out=False)
        # check references
        if any(s in str(msg_details) for s in ['esp-protocols/issues', 'BREAKING CHANGE', 'MAJOR CHANGE']):
            # Closes <issue>
            closes = re.findall(r'Closes ([^\d]+/(\d+))', str(msg_details), re.MULTILINE)
            if closes and closes[0] is not None:
                details += ', [#{}]({})'.format(closes[0][1], closes[0][0])
            # Breaking changes
            find_breaking = re.findall(r'BREAKING CHANGE:\s*([^\n]*)', str(msg_details), re.MULTILINE)
            if find_breaking is not None and len(find_breaking) > 0:
                if find_breaking[0] != '':
                    items['breaking'].append('{} ([{}](https://github.com/espressif/esp-protocols/commit/{}))'.format(find_breaking[0], item_message, commit))
                else:
                    items['breaking'].append('{} ({})'.format(item_message, details))
                details += ', !BREAKING'
            # Major changes
            find_major = re.findall(r'MAJOR CHANGE:\s*([^\n]*)',
                                    str(msg_details), re.MULTILINE)
            if find_major is not None and len(
                    find_major) > 0 and find_major[0] != '':
                items['major'].append('{} ([{}](https://github.com/espressif/esp-protocols/commit/{}))'.format(find_major[0], item_message, commit))
        if item_type in ['fix', 'feat']:
            items[item_type].append('{} ({})'.format(item_message, details))
        else:
            items['update'].append('{} ({})'.format(item_message, details))
    # Generate changelog
    changelog = '## [{}](https://github.com/espressif/esp-protocols/commits/{})\n'.format(
        new_ref, new_tag)
    for section, item in items.items():
        if len(item) > 0:
            changelog += '\n### {}\n\n'.format(sections[section])
            for it in item:
                changelog += '- {}\n'.format(it)
    changelog += '\n'
    filename = os.path.join(root_path, 'components', component, 'CHANGELOG.md')
    # insert the actual changelog to the beginning of the file, just after the title (2nd line)
    with open(filename, 'r') as orig_changelog:
        changelog_title = orig_changelog.readline(
        )  # expect # Changelog title on the first line
        changelog_nl = orig_changelog.readline()
        orig_items = orig_changelog.read()
    with open(filename, 'w') as updated_changelog:
        updated_changelog.write(changelog_title)
        updated_changelog.write(changelog_nl)
        updated_changelog.write(changelog)
        updated_changelog.write(orig_items)
    git.add(filename)

    # write the current changelog entry to a local text file (removing links, captions and extra newlines)
    changelog = re.sub(r'\[([^\]]+)\]\([^\)]+\)', r'\1', changelog)
    changelog = re.sub(r'\#\#[\#\s]*(.+)', r'\1', changelog)
    changelog = re.sub(r'\n\n', '\n', changelog)
    with open(os.path.join(root_path, 'release_notes.txt'), 'w') as release_notes:
        release_notes.write(changelog)


if __name__ == '__main__':
    main()
