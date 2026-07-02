#!/usr/bin/env bash
set -euo pipefail

make all
rm -f nixel.db

./nixel-server &
SERVER_PID=$!
sleep 2

# check the server's own port — always up, always local, no external dependency
./nixel 127.0.0.1 9000 || true

sleep 2
kill "$SERVER_PID" 2>/dev/null || true
sleep 1

COUNT=$(sqlite3 nixel.db "SELECT COUNT(*) FROM checks;")
if [ "$COUNT" -ge 1 ]; then
    echo "PASS: $COUNT row(s) recorded"
    exit 0
else
    echo "FAIL: no rows recorded"
    exit 1
fi
