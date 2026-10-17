#!/usr/bin/env bash
# Load test for Samples_Server: starts N headless net test bots against a server, waits for them to finish and
# prints how many connected and their ping round trips.
#
#   tools/load_test.sh <Sample_NetTest_Headless> <server-ip> [bots=20] [seconds=60] [port=9310]
#
# Each bot's log goes to load_test_logs/bot_<n>.log. Restart the server between runs: it doesn't forget
# clients yet, and it accepts at most 255 in its lifetime.

set -u

if [ $# -lt 2 ]; then
    echo "usage: $0 <Sample_NetTest_Headless> <server-ip> [bots=20] [seconds=60] [port=9310]"
    exit 1
fi

BOT_EXE="$1"
SERVER_IP="$2"
BOTS="${3:-20}"
SECONDS_TO_RUN="${4:-60}"
PORT="${5:-9310}"
LOG_DIR="load_test_logs"

if [ ! -x "$BOT_EXE" ]; then
    echo "not an executable: $BOT_EXE"
    exit 1
fi

rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"

echo "Starting $BOTS bots against $SERVER_IP:$PORT for $SECONDS_TO_RUN s..."
pids=()
for i in $(seq 1 "$BOTS"); do
    "$BOT_EXE" --serverIP="$SERVER_IP" --serverPort="$PORT" --duration="$SECONDS_TO_RUN" > "$LOG_DIR/bot_$i.log" 2>&1 &
    pids+=($!)
    # Small stagger, so the connects don't all land in the same server tick.
    sleep 0.05
done

failed=0
for pid in "${pids[@]}"; do
    wait "$pid" || failed=$((failed + 1))
done

# Lines look like: BOT SUMMARY client=3 connected=1 pings=30 pongs=30 rtt_ms(min/avg/max)=0.4/1.2/5.0 ...
summaries=$(grep -h "BOT SUMMARY" "$LOG_DIR"/bot_*.log | sed 's/\x1b\[[0-9;]*m//g')
connected=$(echo "$summaries" | grep -c "connected=1")
echo
echo "$summaries" | sed 's/.*BOT SUMMARY /  /'
echo
echo "Bots: $BOTS   connected: $connected   exited with an error: $failed"

if [ "$connected" -ne "$BOTS" ] || [ "$failed" -ne 0 ]; then
    echo "LOAD TEST FAILED (see $LOG_DIR/)"
    exit 1
fi
echo "LOAD TEST PASSED"
