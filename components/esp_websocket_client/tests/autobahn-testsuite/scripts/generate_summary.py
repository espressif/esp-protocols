#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""
Autobahn Test Results Summary Generator

This script parses all JSON test results and generates a comprehensive summary.
"""

import glob
import json
import os
from collections import defaultdict
from datetime import datetime
from pathlib import Path


# ANSI color codes for terminal output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    MAGENTA = '\033[95m'
    BOLD = '\033[1m'
    END = '\033[0m'


# Test category descriptions
CATEGORIES = {
    '1': {
        'name': 'Framing',
        'critical': True,
        'description': 'Basic frame structure',
    },
    '2': {
        'name': 'Ping/Pong',
        'critical': True,
        'description': 'Control frames',
    },
    '3': {
        'name': 'Reserved Bits',
        'critical': True,
        'description': 'RSV validation',
    },
    '4': {
        'name': 'Opcodes',
        'critical': True,
        'description': 'Valid/invalid opcodes',
    },
    '5': {
        'name': 'Fragmentation',
        'critical': True,
        'description': 'Message fragments',
    },
    '6': {
        'name': 'UTF-8',
        'critical': False,
        'description': 'Text validation',
    },
    '7': {
        'name': 'Close Handshake',
        'critical': True,
        'description': 'Connection closing',
    },
    '9': {
        'name': 'Performance',
        'critical': False,
        'description': 'Large messages',
    },
    '10': {
        'name': 'Miscellaneous',
        'critical': True,
        'description': 'Edge cases',
    },
    '12': {
        'name': 'Compression',
        'critical': False,
        'description': 'RFC 7692',
    },
    '13': {
        'name': 'Compression',
        'critical': False,
        'description': 'RFC 7692',
    },
}


def get_category_from_test_id(test_id):
    """Extract category number from test ID (e.g., '1.1.1' -> '1')"""
    return test_id.split('.')[0]


def parse_test_results(reports_dir):
    """Parse all JSON test results"""
    results = []
    # Look for JSON files in reports directory and reports/clients subdirectory
    json_files = glob.glob(os.path.join(reports_dir, "*.json"))
    json_files.extend(glob.glob(os.path.join(reports_dir, "clients", "*.json")))

    for json_file in json_files:
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
                results.append({
                    'id': data.get('id', 'unknown'),
                    'behavior': data.get('behavior', 'UNKNOWN'),
                    'description': data.get('description', ''),
                    'duration': data.get('duration', 0),
                    'result': data.get('result', ''),
                })
        except Exception as e:
            print(f"Error parsing {json_file}: {e}")

    # Sort by test ID with proper numeric sorting
    def sort_key(result):
        parts = result['id'].split('.')
        # Convert to sortable format: pad numbers, keep strings
        sortable = []
        for p in parts:
            if p.isdigit():
                sortable.append((0, int(p)))  # numbers sort before strings
            else:
                sortable.append((1, p))  # strings sort after numbers
        return tuple(sortable)

    return sorted(results, key=sort_key)


def generate_summary(results):
    """Generate comprehensive summary statistics"""

    # Overall stats
    total = len(results)
    by_behavior = defaultdict(int)
    by_category = defaultdict(lambda: defaultdict(int))

    for result in results:
        behavior = result['behavior']
        by_behavior[behavior] += 1

        category = get_category_from_test_id(result['id'])
        by_category[category][behavior] += 1

    return {
        'total': total,
        'by_behavior': dict(by_behavior),
        'by_category': dict(by_category),
    }


def calculate_grade(pass_rate):
    """Calculate letter grade based on pass rate"""
    if pass_rate >= 90:
        return 'A', Colors.GREEN
    elif pass_rate >= 80:
        return 'B', Colors.GREEN
    elif pass_rate >= 70:
        return 'C', Colors.YELLOW
    elif pass_rate >= 60:
        return 'D', Colors.YELLOW
    else:
        return 'F', Colors.RED


def print_banner(text):
    """Print a banner"""
    print()
    print(f"{Colors.BLUE}{'=' * 80}{Colors.END}")
    print(f"{Colors.BLUE}{Colors.BOLD}{text:^80}{Colors.END}")
    print(f"{Colors.BLUE}{'=' * 80}{Colors.END}")
    print()


def print_summary_table(summary):
    """Print overall summary table"""
    print_banner("OVERALL TEST RESULTS")

    total = summary['total']
    by_behavior = summary['by_behavior']

    # Calculate percentages
    passed = by_behavior.get('OK', 0)
    failed = by_behavior.get('FAILED', 0)
    non_strict = by_behavior.get('NON-STRICT', 0)
    informational = by_behavior.get('INFORMATIONAL', 0)
    unimplemented = by_behavior.get('UNIMPLEMENTED', 0)

    pass_rate = (passed / total * 100) if total > 0 else 0
    fail_rate = (failed / total * 100) if total > 0 else 0

    grade, grade_color = calculate_grade(pass_rate)

    print(f'Total Tests:        {Colors.BOLD}{total}{Colors.END}')
    print()
    print(
        f'{Colors.GREEN}✅ PASSED:          {passed:3d}  '
        f'({pass_rate:5.1f}%){Colors.END}',
    )
    print(
        f'{Colors.RED}❌ FAILED:          {failed:3d}  '
        f'({fail_rate:5.1f}%){Colors.END}',
    )
    if non_strict > 0:
        print(
            f'{Colors.YELLOW}⚠️  NON-STRICT:       {non_strict:3d}  '
            f'({non_strict/total*100:5.1f}%){Colors.END}',
        )
    if informational > 0:
        print(
            f'{Colors.CYAN}ℹ️  INFORMATIONAL:    {informational:3d}  '
            f'({informational/total*100:5.1f}%){Colors.END}',
        )
    if unimplemented > 0:
        print(
            f'{Colors.MAGENTA}🔧 UNIMPLEMENTED:    {unimplemented:3d}  '
            f'({unimplemented/total*100:5.1f}%){Colors.END}',
        )

    print()
    print(
        f'Overall Grade:      {grade_color}{Colors.BOLD}{grade}{Colors.END}',
    )
    print()

    # Embedded client rating
    if pass_rate >= 70:
        rating = "Excellent"
        rating_color = Colors.GREEN
    elif pass_rate >= 55:
        rating = "Good"
        rating_color = Colors.GREEN
    elif pass_rate >= 40:
        rating = "Acceptable"
        rating_color = Colors.YELLOW
    else:
        rating = "Needs Improvement"
        rating_color = Colors.RED

    print(
        'Embedded Client Rating: '
        f'{rating_color}{Colors.BOLD}{rating}{Colors.END}',
    )
    print()


def print_category_breakdown(summary):
    """Print detailed category breakdown"""
    print_banner("RESULTS BY TEST CATEGORY")

    by_category = summary['by_category']

    # Header
    print(f"{'Category':<25} {'Total':>7} {'Pass':>7} {'Fail':>7} {'Rate':>8} {'Critical':>10} {'Grade':>7}")
    print(f"{'-'*25} {'-'*7} {'-'*7} {'-'*7} {'-'*8} {'-'*10} {'-'*7}")

    # Sort categories numerically
    sorted_categories = sorted(by_category.keys(), key=lambda x: int(x) if x.isdigit() else 999)

    for cat_num in sorted_categories:
        cat_stats = by_category[cat_num]
        cat_info = CATEGORIES.get(cat_num, {'name': f'Category {cat_num}', 'critical': True})

        total = sum(cat_stats.values())
        passed = cat_stats.get('OK', 0)
        failed = cat_stats.get('FAILED', 0)
        pass_rate = (passed / total * 100) if total > 0 else 0

        grade, grade_color = calculate_grade(pass_rate)

        # Format category name
        cat_name = f"{cat_num}.* {cat_info['name']}"
        critical = "Yes" if cat_info['critical'] else "No"

        # Color code the pass rate
        if pass_rate >= 70:
            rate_color = Colors.GREEN
        elif pass_rate >= 50:
            rate_color = Colors.YELLOW
        else:
            rate_color = Colors.RED

        print(f"{cat_name:<25} {total:>7} {passed:>7} {failed:>7} "
              f"{rate_color}{pass_rate:>7.1f}%{Colors.END} {critical:>10} "
              f"{grade_color}{grade:>7}{Colors.END}")

    print()


def print_failed_tests(results, limit=20):
    """Print list of failed tests"""
    print_banner("FAILED TESTS (First 20)")

    failed = [r for r in results if r['behavior'] == 'FAILED']

    if not failed:
        print(f'{Colors.GREEN}🎉 No failed tests!{Colors.END}\n')
        return

    print(
        'Total Failed: '
        f'{Colors.RED}{Colors.BOLD}{len(failed)}{Colors.END}\n',
    )

    for i, test in enumerate(failed[:limit], 1):
        print(f"{i:2d}. {Colors.RED}Test {test['id']:<12}{Colors.END} - {test['description'][:60]}")
        if test['result']:
            print(f"    Reason: {test['result'][:100]}")

    if len(failed) > limit:
        print(f"\n... and {len(failed) - limit} more failed tests")

    print()


def print_recommendations(summary):
    """Print recommendations based on results"""
    print_banner("RECOMMENDATIONS")

    by_category = summary['by_category']

    # Check critical categories
    critical_issues = []

    for cat_num, cat_info in CATEGORIES.items():
        if not cat_info['critical']:
            continue

        if cat_num not in by_category:
            continue

        cat_stats = by_category[cat_num]
        total = sum(cat_stats.values())
        passed = cat_stats.get('OK', 0)
        pass_rate = (passed / total * 100) if total > 0 else 0

        if pass_rate < 70:
            critical_issues.append(
                {
                    'category': f"{cat_num}.* {cat_info['name']}",
                    'pass_rate': pass_rate,
                    'description': cat_info['description'],
                },
            )

    if critical_issues:
        print(f"{Colors.RED}⚠️  Critical Issues Found:{Colors.END}\n")
        for issue in critical_issues:
            print(f"  • {issue['category']}: {issue['pass_rate']:.1f}% pass rate")
            print(f"    {issue['description']} - This requires attention\n")
    else:
        print(f"{Colors.GREEN}✅ All critical test categories are performing well!{Colors.END}\n")

    # UTF-8 check
    if '6' in by_category:
        cat_stats = by_category['6']
        total = sum(cat_stats.values())
        passed = cat_stats.get('OK', 0)
        pass_rate = (passed / total * 100) if total > 0 else 0

        if pass_rate < 50:
            print(
                f'{Colors.YELLOW}ℹ️  UTF-8 Validation (6.*): '
                f'{pass_rate:.1f}% pass rate{Colors.END}',
            )
            print(
                '   This is acceptable for embedded clients in '
                'trusted environments.',
            )
            print(
                '   Consider adding UTF-8 validation if operating in '
                'untrusted networks.\n',
            )

    print(f'{Colors.CYAN}💡 Next Steps:{Colors.END}\n')
    print('  1. Review failed tests in detail (see HTML report)')
    print('  2. Focus on critical categories (1-5, 7, 10)')
    print('  3. UTF-8 failures (category 6) are acceptable for embedded clients')
    print('  4. Compare your results with reference implementations\n')


def generate_markdown_summary(summary, results):
    """Generate markdown summary for GitHub Actions Job Summary"""
    total = summary['total']
    by_behavior = summary['by_behavior']
    by_category = summary['by_category']

    passed = by_behavior.get('OK', 0)
    failed = by_behavior.get('FAILED', 0)
    non_strict = by_behavior.get('NON-STRICT', 0)
    informational = by_behavior.get('INFORMATIONAL', 0)
    unimplemented = by_behavior.get('UNIMPLEMENTED', 0)

    pass_rate = (passed / total * 100) if total > 0 else 0
    grade, _ = calculate_grade(pass_rate)

    # Determine status emoji and color
    if pass_rate >= 90:
        status_emoji = "🟢"
    elif pass_rate >= 70:
        status_emoji = "🟡"
    else:
        status_emoji = "🔴"

    md = f"""# {status_emoji} Autobahn Test Suite Results

## Overall Results

| Metric | Value |
|--------|-------|
| **Total Tests** | {total} |
| **✅ Passed** | {passed} ({pass_rate:.1f}%) |
| **❌ Failed** | {failed} ({(failed/total*100) if total > 0 else 0:.1f}%) |
"""

    if non_strict > 0:
        md += f"| **⚠️ Non-Strict** | {non_strict} ({non_strict/total*100:.1f}%) |\n"
    if informational > 0:
        md += f"| **ℹ️ Informational** | {informational} ({informational/total*100:.1f}%) |\n"
    if unimplemented > 0:
        md += f"| **🔧 Unimplemented** | {unimplemented} ({unimplemented/total*100:.1f}%) |\n"

    md += f"""
| **Overall Grade** | **{grade}** |
| **Pass Rate** | {pass_rate:.1f}% |

### Progress Bar

<progress value="{passed}" max="{total}"></progress> {pass_rate:.1f}%

---

## Results by Category

| Category | Name | Total | Passed | Failed | Pass Rate | Critical |
|----------|------|-------|--------|--------|-----------|----------|
"""

    sorted_categories = sorted(by_category.keys(), key=lambda x: int(x) if x.isdigit() else 999)
    for cat_num in sorted_categories:
        cat_stats = by_category[cat_num]
        cat_info = CATEGORIES.get(cat_num, {'name': f'Category {cat_num}', 'critical': True})

        total_cat = sum(cat_stats.values())
        passed_cat = cat_stats.get('OK', 0)
        failed_cat = cat_stats.get('FAILED', 0)
        pass_rate_cat = (passed_cat / total_cat * 100) if total_cat > 0 else 0

        critical_mark = "✅ Yes" if cat_info['critical'] else "⚪ No"
        pass_mark = "✅" if pass_rate_cat >= 70 else "⚠️" if pass_rate_cat >= 50 else "❌"

        md += f"| **{cat_num}.** | {cat_info['name']} | {total_cat} | {passed_cat} | {failed_cat} | {pass_mark} {pass_rate_cat:.1f}% | {critical_mark} |\n"

    # Failed tests section
    failed_tests = [r for r in results if r['behavior'] == 'FAILED']
    if failed_tests:
        md += f"""
---

## ❌ Failed Tests ({len(failed_tests)} total)

"""
        for i, test in enumerate(failed_tests[:20], 1):
            md += f"{i}. **Test {test['id']}**: {test['description'][:80]}\n"
            if test['result']:
                md += f"   - Reason: {test['result'][:100]}\n"

        if len(failed_tests) > 20:
            md += f"\n*... and {len(failed_tests) - 20} more failed tests*\n"
    else:
        md += "\n---\n\n## 🎉 No Failed Tests!\n\n"

    # Recommendations
    md += "\n---\n\n## 💡 Recommendations\n\n"

    critical_issues = []
    for cat_num, cat_info in CATEGORIES.items():
        if not cat_info['critical']:
            continue
        if cat_num not in by_category:
            continue

        cat_stats = by_category[cat_num]
        total_cat = sum(cat_stats.values())
        passed_cat = cat_stats.get('OK', 0)
        pass_rate_cat = (passed_cat / total_cat * 100) if total_cat > 0 else 0

        if pass_rate_cat < 70:
            critical_issues.append({
                'category': f"{cat_num}.* {cat_info['name']}",
                'pass_rate': pass_rate_cat,
            })

    if critical_issues:
        md += "### ⚠️ Critical Issues Found:\n\n"
        for issue in critical_issues:
            md += f"- **{issue['category']}**: {issue['pass_rate']:.1f}% pass rate - Requires attention\n"
    else:
        md += "### ✅ All critical test categories are performing well!\n\n"

    md += """
### Next Steps:
1. Review failed tests in detail (see uploaded HTML report)
2. Focus on critical categories (1-5, 7, 10)
3. UTF-8 failures (category 6) are acceptable for embedded clients
4. Compare results with reference implementations

---
*Generated by Autobahn Test Suite Summary Generator*
"""

    return md


def generate_html_summary(summary, results, output_file):
    """Generate an HTML summary file"""
    total = summary['total']
    passed = summary['by_behavior'].get('OK', 0)
    failed = summary['by_behavior'].get('FAILED', 0)

    pass_rate = (passed / total * 100) if total > 0 else 0
    fail_rate = (failed / total * 100) if total > 0 else 0
    grade, _ = calculate_grade(pass_rate)

    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>ESP WebSocket Client - Autobahn Test Summary</title>
    <style>
        body {{ font-family: 'Segoe UI', Arial, sans-serif;
               margin: 40px;
               background: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; background: white;
                     padding: 30px; border-radius: 8px;
                     box-shadow: 0 2px 4px rgba(0,0,0,0.1); }}
        h1 {{ color: #333; border-bottom: 3px solid #4CAF50; padding-bottom: 10px; }}
        h2 {{ color: #555; margin-top: 30px; border-bottom: 2px solid #ddd; padding-bottom: 8px; }}
        .stats {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }}
        .stat-card {{ background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; border-radius: 8px; text-align: center; }}
        .stat-card.passed {{ background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); }}
        .stat-card.failed {{ background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%); }}
        .stat-card h3 {{ margin: 0; font-size: 36px; }}
        .stat-card p {{ margin: 5px 0 0 0; opacity: 0.9; }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        th {{ background: #667eea; color: white; padding: 12px; text-align: left; }}
        td {{ padding: 10px; border-bottom: 1px solid #ddd; }}
        tr:hover {{ background: #f5f5f5; }}
        .pass {{ color: #11998e; font-weight: bold; }}
        .fail {{ color: #eb3349; font-weight: bold; }}
        .grade {{ font-size: 24px; font-weight: bold; padding: 10px 20px;
                  border-radius: 5px; display: inline-block; }}
        .grade-a, .grade-b {{ background: #11998e; color: white; }}
        .grade-c, .grade-d {{ background: #f39c12; color: white; }}
        .grade-f {{ background: #eb3349; color: white; }}
        .progress-bar {{ background: #ddd; border-radius: 10px; overflow: hidden;
                        height: 30px; margin: 10px 0; }}
        .progress-fill {{ background: linear-gradient(90deg, #11998e 0%,
                                                      #38ef7d 100%);
                         height: 100%; display: flex; align-items: center;
                         justify-content: center; color: white;
                         font-weight: bold; transition: width 0.3s; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🔬 ESP WebSocket Client - Autobahn Test Suite Results</h1>
        <p style="color: #666; font-size: 14px;">Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>

        <h2>📊 Overall Results</h2>
        <div class="stats">
            <div class="stat-card">
                <h3>{total}</h3>
                <p>Total Tests</p>
            </div>
            <div class="stat-card passed">
                <h3>{passed}</h3>
                <p>Passed ({pass_rate:.1f}%)</p>
            </div>
            <div class="stat-card failed">
                <h3>{failed}</h3>
                <p>Failed ({fail_rate:.1f}%)</p>
            </div>
        </div>

        <div class="progress-bar">
            <div class="progress-fill" style="width: {pass_rate:.1f}%">
                {pass_rate:.1f}% Pass Rate
            </div>
        </div>

        <div style="text-align: center; margin: 30px 0;">
            <span class="grade grade-{grade.lower()}">
                Grade: {grade}
            </span>
        </div>

        <h2>📋 Results by Category</h2>
        <table>
            <thead>
                <tr>
                    <th>Category</th>
                    <th>Description</th>
                    <th>Total</th>
                    <th>Passed</th>
                    <th>Failed</th>
                    <th>Pass Rate</th>
                    <th>Critical</th>
                </tr>
            </thead>
            <tbody>
"""

    # Add category rows
    sorted_categories = sorted(summary['by_category'].keys(), key=lambda x: int(x) if x.isdigit() else 999)
    for cat_num in sorted_categories:
        cat_stats = summary['by_category'][cat_num]
        cat_info = CATEGORIES.get(cat_num, {'name': f'Category {cat_num}', 'critical': True, 'description': 'Unknown'})

        total = sum(cat_stats.values())
        passed = cat_stats.get('OK', 0)
        failed = cat_stats.get('FAILED', 0)
        pass_rate = (passed / total * 100) if total > 0 else 0

        html += f"""
                <tr>
                    <td><strong>{cat_num}.*</strong></td>
                    <td>{cat_info['name']} - {cat_info['description']}</td>
                    <td>{total}</td>
                    <td class="pass">{passed}</td>
                    <td class="fail">{failed}</td>
                    <td>
                        <span class="{'pass' if pass_rate >= 70 else 'fail'}">{pass_rate:.1f}%</span>
                    </td>
                    <td>{'✅ Yes' if cat_info['critical'] else '⚪ No'}</td>
                </tr>
"""

    html += """
            </tbody>
        </table>

        <h2>❌ Failed Tests</h2>
        <table>
            <thead>
                <tr>
                    <th>Test ID</th>
                    <th>Description</th>
                    <th>Result</th>
                </tr>
            </thead>
            <tbody>
"""

    failed_tests = [r for r in results if r['behavior'] == 'FAILED']
    for test in failed_tests[:50]:  # Limit to 50 in HTML
        html += f"""
                <tr>
                    <td><strong>{test['id']}</strong></td>
                    <td>{test['description']}</td>
                    <td style="font-size: 12px; color: #666;">{test['result'][:100]}</td>
                </tr>
"""

    if len(failed_tests) > 50:
        html += f"""
                <tr>
                    <td colspan="3" style="text-align: center; color: #666;">
                        ... and {len(failed_tests) - 50} more failed tests (see individual reports)
                    </td>
                </tr>
"""

    html += """
            </tbody>
        </table>

        <h2>💡 Recommendations</h2>
        <div style="background: #e8f5e9; padding: 20px; border-radius: 8px; border-left: 4px solid #4CAF50;">
            <p><strong>For Embedded WebSocket Clients:</strong></p>
            <ul>
                <li>✅ Focus on passing critical categories: 1.* (Framing), 2.* (Ping/Pong), 5.* (Fragmentation), 7.* (Close)</li>
                <li>⚠️ UTF-8 validation failures (6.*) are acceptable in trusted environments</li>
                <li>🎯 Target >70% overall pass rate for production use</li>
                <li>🔍 Review individual test reports for specific implementation issues</li>
            </ul>
        </div>

        <div style="margin-top: 30px; padding: 20px; background: #f5f5f5; border-radius: 8px; text-align: center;">
            <p style="color: #666; margin: 0;">
                <strong>View Detailed Reports:</strong>
                <a href="index.html">Open Full Autobahn Report</a>
            </p>
        </div>
    </div>
</body>
</html>
"""

    with open(output_file, 'w') as f:
        f.write(html)

    print(
        f'{Colors.GREEN}✅ HTML summary generated: {output_file}{Colors.END}\n',
    )


def main():
    """Main entry point"""
    script_dir = Path(__file__).parent
    reports_dir = script_dir.parent / 'reports'

    # Check if GitHub Actions summary file path is provided
    github_summary_file = os.environ.get('GITHUB_STEP_SUMMARY')

    if not reports_dir.exists():
        print(
            f'{Colors.RED}Error: Reports directory not found: '
            f'{reports_dir}{Colors.END}',
        )
        return 1

    print(
        f'{Colors.CYAN}📖 Parsing test results from: '
        f'{reports_dir}{Colors.END}',
    )
    results = parse_test_results(str(reports_dir))

    if not results:
        print(f'{Colors.RED}Error: No test results found{Colors.END}')
        return 1

    print(
        f'{Colors.GREEN}✅ Parsed {len(results)} '
        f'test results{Colors.END}',
    )

    summary = generate_summary(results)

    # Print console summary
    print_summary_table(summary)
    print_category_breakdown(summary)
    print_failed_tests(results)
    print_recommendations(summary)

    # Generate HTML summary
    html_output = reports_dir / 'summary.html'
    generate_html_summary(summary, results, str(html_output))

    print(f'{Colors.CYAN}🌐 Open the summary in your browser:{Colors.END}')
    print(f'   file://{html_output.absolute()}\n')

    # Generate markdown summary for GitHub Actions
    if github_summary_file:
        md_summary = generate_markdown_summary(summary, results)
        with open(github_summary_file, 'w') as f:
            f.write(md_summary)
        print(f'{Colors.GREEN}✅ GitHub Actions summary written to: {github_summary_file}{Colors.END}')

    return 0


if __name__ == '__main__':
    exit(main())
