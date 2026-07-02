#!/usr/bin/env bash
set -euo pipefail

# build
make all

# start server in background, give it a moment
rm -f nixel.db
./nixel-server &
SERVER_PID=$!
sleep 1

# run the agent against a known-up host
./nixel example.com 443

# stop the server
kill "$SERVER_PID" 2>/dev/null || true
sleep 1

# assert a row was written
COUNT=$(sqlite3 nixel.db "SELECT COUNT(*) FROM checks;")
if [ "$COUNT" -ge 1 ]; then
    echo "PASS: $COUNT row(s) recorded"
    exit 0
else
    echo "FAIL: no rows recorded"
    exit 1
fi
