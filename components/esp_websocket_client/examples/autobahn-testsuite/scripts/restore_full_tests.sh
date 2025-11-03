#!/bin/bash

# Restore Full Test Configuration

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
cd "$SCRIPT_DIR"

echo "Restoring full test configuration..."

# Stop server
docker-compose down

# Restore original config
if [ -f config/fuzzingserver.json.backup ]; then
    mv config/fuzzingserver.json.backup config/fuzzingserver.json
    echo "✓ Original config restored"
else
    # Create default full config
    cat > config/fuzzingserver.json << 'EOF'
{
   "url": "ws://0.0.0.0:9001",
   "options": {
      "failByDrop": false
   },
   "outdir": "/reports",
   "webport": 8080,
   "cases": ["*"],
   "exclude-cases": [
      "9.*",
      "12.*",
      "13.*"
   ],
   "exclude-agent-cases": {}
}
EOF
    echo "✓ Default full config created"
fi

echo
echo "Full test configuration restored (~300 tests)"
echo "Start server with: docker-compose up -d"

