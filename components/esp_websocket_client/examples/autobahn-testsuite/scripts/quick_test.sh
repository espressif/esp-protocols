#!/bin/bash

# Quick Test Script - Runs a minimal set of tests for rapid validation
# This is useful for development/debugging without waiting for all 300+ tests

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "Quick Test Mode - Core Tests Only"
echo "=========================================="
echo

# Stop any existing server
docker-compose down 2>/dev/null || true

# Copy quick config
cp config/fuzzingserver-quick.json config/fuzzingserver.json.backup 2>/dev/null || true
cp config/fuzzingserver-quick.json config/fuzzingserver.json

echo "Starting fuzzing server with quick test config..."
echo "  Tests: Core protocol tests (~150 cases)"
echo "  Excludes: UTF-8 validation, performance, compression"
echo

docker-compose up -d

echo
echo "✓ Quick test server started"
echo
echo "Run your ESP32 testee client now."
echo "Estimated time: 5-10 minutes"
echo
echo "View results at: http://localhost:8080"
echo
echo "To restore full test config:"
echo "  ./scripts/restore_full_tests.sh"

