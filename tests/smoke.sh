#!/usr/bin/env bash
set -euo pipefail

make all
rm -f nixel.db

# start the ingest server
./nixel-server &
SERVER_PID=$!
sleep 2

# two temp configs pointed at different local targets, so we can prove
# BOTH agents' results land in the DB concurrently — not just one.
# 127.0.0.1:9000 is the server itself (always up). The second target
# distinguishes agent B's stream from agent A's.
CONF_A=$(mktemp)
CONF_B=$(mktemp)
cat > "$CONF_A" <<EOF
interval 1
127.0.0.1 9000
EOF
cat > "$CONF_B" <<EOF
interval 1
127.0.0.1 9001
EOF

# start both agents as daemons, let them run a few cycles, then stop them
./nixel "$CONF_A" &
AGENT_A=$!
./nixel "$CONF_B" &
AGENT_B=$!
sleep 4
kill "$AGENT_A" "$AGENT_B" 2>/dev/null || true

sleep 1
kill "$SERVER_PID" 2>/dev/null || true
rm -f "$CONF_A" "$CONF_B"
sleep 1

# both distinct targets must have recorded rows -> both connections were live at once
PORTS=$(sqlite3 nixel.db "SELECT COUNT(DISTINCT port) FROM checks WHERE port IN ('9000','9001');")
TOTAL=$(sqlite3 nixel.db "SELECT COUNT(*) FROM checks;")
if [ "$PORTS" -eq 2 ] && [ "$TOTAL" -ge 2 ]; then
    echo "PASS: $TOTAL row(s) from 2 concurrent agents"
    exit 0
else
    echo "FAIL: expected rows from 2 agents, got ports=$PORTS total=$TOTAL"
    exit 1
fi
