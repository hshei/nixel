#!/usr/bin/env bash
set -euo pipefail

make all
rm -f nixel.db

# start the ingest server
./nixel-server &
SERVER_PID=$!
sleep 2

# write a temp config: check the server's own port on a short interval.
# 127.0.0.1:9000 is always up and local, so the test has no external dependency.
CONF=$(mktemp)
cat > "$CONF" <<EOF
interval 1
127.0.0.1 9000
EOF

# start the agent as a daemon, let it run a couple of check cycles, then stop it
./nixel "$CONF" &
AGENT_PID=$!
sleep 3
kill "$AGENT_PID" 2>/dev/null || true

sleep 1
kill "$SERVER_PID" 2>/dev/null || true
rm -f "$CONF"
sleep 1

COUNT=$(sqlite3 nixel.db "SELECT COUNT(*) FROM checks;")
if [ "$COUNT" -ge 1 ]; then
    echo "PASS: $COUNT row(s) recorded"
    exit 0
else
    echo "FAIL: no rows recorded"
    exit 1
fi
