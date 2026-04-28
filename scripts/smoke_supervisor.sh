#!/bin/bash
# RPD stage 2 D-1 smoke: ez_router + smoke_supervised_child + SIGSTOP -> stale WARN
# Run on target. Assumes:
#   /opt/ez_router/out/ez_router  (built)
#   /tmp/smoke_supervised_child   (built)

LOG=/tmp/ezr_smoke.log
SCLOG=/tmp/scsl.log

pkill -9 -f "ez_router -debug" 2>/dev/null
pkill -9 -f smoke_supervised_child 2>/dev/null
sleep 1
rm -f /run/ez_router/ez_router.sock "$LOG" "$SCLOG"
mkdir -p /run/ez_router

cd /opt/ez_router/out || exit 1
setsid bash -c "./ez_router -debug > $LOG 2>&1" < /dev/null > /dev/null 2>&1 &
sleep 1

EZ_PID=$(pgrep -f "ez_router -debug" | tail -1)
# tail -1: setsid 父 bash 也会匹配 -f cmdline,真子进程在末尾
echo "ez_router PID=$EZ_PID"
if [ -z "$EZ_PID" ]; then
    echo "FAIL: ez_router not running"
    tail -20 "$LOG"
    exit 1
fi
ls -la /run/ez_router/ez_router.sock || {
    echo "FAIL: socket not created"
    tail -20 "$LOG"
    exit 1
}

setsid /tmp/smoke_supervised_child SMOKE-SUP-1 1000 < /dev/null > "$SCLOG" 2>&1 &
sleep 2

CHILD_PID=$(pgrep -f smoke_supervised_child | head -1)
echo "smoke child PID=$CHILD_PID"
if [ -z "$CHILD_PID" ]; then
    echo "FAIL: smoke child not running"
    cat "$SCLOG"
    kill -TERM "$EZ_PID" 2>/dev/null
    exit 1
fi

echo "--- ez_router log: register/heartbeat traces ---"
grep -E "registered|HEARTBEAT|REGISTER" "$LOG" | head -10

echo "--- SIGSTOP smoke child (PID=$CHILD_PID) ---"
kill -STOP "$CHILD_PID"

echo "waiting 8s for stale detection (timeout=5000ms)..."
sleep 8

echo "--- ez_router log: lines containing 'sup' or 'stale' ---"
grep -E "sup|stale" "$LOG"
echo "---"
echo "=== full log tail (last 40 lines) ==="
tail -40 "$LOG"

echo "--- cleanup ---"
kill -CONT "$CHILD_PID" 2>/dev/null
kill -TERM "$CHILD_PID" 2>/dev/null
sleep 1
kill -KILL "$CHILD_PID" 2>/dev/null
kill -TERM "$EZ_PID" 2>/dev/null
sleep 1
kill -KILL "$EZ_PID" 2>/dev/null
echo "DONE"
