#!/usr/bin/env python3
"""
Autobahn Test Results Analyzer

Parses the JSON report from Autobahn testsuite and provides a summary.
"""

import json
import sys
from pathlib import Path
from collections import defaultdict

def analyze_results(report_path):
    """Analyze the Autobahn test results JSON file."""
    
    with open(report_path, 'r') as f:
        data = json.load(f)
    
    # Find the agent name (should be esp_websocket_client)
    agents = list(data.keys())
    if not agents:
        print("❌ No test results found in report")
        return 1
    
    agent = agents[0]
    results = data[agent]
    
    print(f"\n{'='*60}")
    print(f"Autobahn WebSocket Testsuite Results")
    print(f"Agent: {agent}")
    print(f"{'='*60}\n")
    
    # Count results by behavior
    counts = defaultdict(int)
    by_category = defaultdict(lambda: defaultdict(int))
    failures = []
    
    for case_id, result in results.items():
        behavior = result.get('behavior', 'UNKNOWN')
        counts[behavior] += 1
        
        # Extract category (e.g., "1" from "1.2.3")
        category = case_id.split('.')[0]
        by_category[category][behavior] += 1
        
        if behavior in ['FAILED', 'UNIMPLEMENTED']:
            failures.append({
                'case': case_id,
                'behavior': behavior,
                'description': result.get('description', 'N/A'),
                'result': result.get('behaviorClose', 'N/A')
            })
    
    # Print overall summary
    total = sum(counts.values())
    print(f"Overall Results ({total} tests):")
    print(f"  ✅ PASSED:         {counts['OK']:3d} ({counts['OK']/total*100:5.1f}%)")
    print(f"  ⚠️  NON-STRICT:    {counts['NON-STRICT']:3d} ({counts['NON-STRICT']/total*100:5.1f}%)")
    print(f"  ℹ️  INFORMATIONAL: {counts['INFORMATIONAL']:3d} ({counts['INFORMATIONAL']/total*100:5.1f}%)")
    print(f"  ❌ FAILED:         {counts['FAILED']:3d} ({counts['FAILED']/total*100:5.1f}%)")
    print(f"  ⭕ UNIMPLEMENTED:  {counts['UNIMPLEMENTED']:3d} ({counts['UNIMPLEMENTED']/total*100:5.1f}%)")
    print()
    
    # Print by category
    print(f"Results by Category:")
    print(f"  {'Cat':<4} {'Pass':>6} {'N-S':>6} {'Info':>6} {'Fail':>6} {'N/I':>6} {'Total':>6}")
    print(f"  {'-'*4} {'-'*6} {'-'*6} {'-'*6} {'-'*6} {'-'*6} {'-'*6}")
    
    for category in sorted(by_category.keys(), key=lambda x: int(x) if x.isdigit() else 999):
        cat_results = by_category[category]
        cat_total = sum(cat_results.values())
        print(f"  {category:>3}. "
              f"{cat_results['OK']:>6} "
              f"{cat_results['NON-STRICT']:>6} "
              f"{cat_results['INFORMATIONAL']:>6} "
              f"{cat_results['FAILED']:>6} "
              f"{cat_results['UNIMPLEMENTED']:>6} "
              f"{cat_total:>6}")
    
    print()
    
    # Print failures details
    if failures:
        print(f"\n❌ Failed/Unimplemented Tests ({len(failures)}):")
        print(f"  {'-'*60}")
        for fail in failures:
            print(f"  Case {fail['case']}: {fail['behavior']}")
            print(f"    Description: {fail['description'][:70]}")
            print(f"    Result: {fail['result']}")
            print()
    else:
        print("\n🎉 All tests passed or acceptable! No failures.")
    
    # Quality assessment
    print(f"\n{'='*60}")
    print("Quality Assessment:")
    
    pass_rate = (counts['OK'] + counts['NON-STRICT']) / total * 100
    fail_rate = counts['FAILED'] / total * 100
    
    if fail_rate == 0 and counts['OK'] > total * 0.8:
        print("  🌟 EXCELLENT - Production ready!")
    elif fail_rate < 5 and pass_rate > 85:
        print("  ✅ GOOD - Minor issues to address")
    elif fail_rate < 10 and pass_rate > 70:
        print("  ⚠️  FAIR - Several issues need fixing")
    else:
        print("  ❌ POOR - Significant protocol compliance issues")
    
    print(f"{'='*60}\n")
    
    return 0 if fail_rate < 5 else 1

def main():
    if len(sys.argv) > 1:
        report_path = Path(sys.argv[1])
    else:
        # Default location
        script_dir = Path(__file__).parent.parent
        report_path = script_dir / "reports" / "clients" / "index.json"
    
    if not report_path.exists():
        print(f"❌ Error: Report file not found: {report_path}")
        print("\nUsage: python analyze_results.py [path/to/index.json]")
        print(f"Default: {report_path}")
        return 1
    
    return analyze_results(report_path)

if __name__ == '__main__':
    sys.exit(main())

