#!/bin/bash
#
# End-to-end integration test for pmtr
# Runs the actual pmtr binary and verifies behavior
#
set -e

PMTR="${1:-./src/pmtr}"
TMPDIR="${TMPDIR:-/tmp}"
TEST_DIR="$TMPDIR/pmtr-e2e-$$"
PASSED=0
FAILED=0

cleanup() {
    # Kill any remaining pmtr processes
    pkill -f "pmtr.*$TEST_DIR" 2>/dev/null || true
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

mkdir -p "$TEST_DIR"

pass() {
    echo "  PASS: $1"
    PASSED=$((PASSED + 1))
}

fail() {
    echo "  FAIL: $1"
    FAILED=$((FAILED + 1))
}

# Test 1: Config test mode (-t flag)
test_config_validation() {
    echo "Test: config validation (-t flag)"

    cat > "$TEST_DIR/valid.conf" << 'EOF'
job {
    name test_job
    cmd /bin/sleep 100
}
EOF

    if "$PMTR" -t -c "$TEST_DIR/valid.conf" 2>/dev/null; then
        pass "valid config accepted"
    else
        fail "valid config rejected"
    fi

    cat > "$TEST_DIR/invalid.conf" << 'EOF'
job {
    name test_job
    cmd /bin/true
EOF

    # pmtr -t always exits 0, so check stderr for parse error
    if "$PMTR" -t -c "$TEST_DIR/invalid.conf" 2>&1 | grep -q "parse failed"; then
        pass "invalid config rejected"
    else
        fail "invalid config accepted"
    fi
}

# Test 2: Basic job execution
test_job_execution() {
    echo "Test: basic job execution"

    cat > "$TEST_DIR/job.conf" << EOF
job {
    name sleeper
    cmd /bin/sleep 60
}
EOF

    # Start pmtr in foreground, backgrounded
    "$PMTR" -F -c "$TEST_DIR/job.conf" &
    PMTR_PID=$!

    # Wait for job to start
    sleep 1

    # Check if sleep process is running
    if pgrep -f "sleep 60" > /dev/null; then
        pass "job started successfully"
    else
        fail "job did not start"
    fi

    # Send SIGTERM to pmtr
    kill -TERM $PMTR_PID 2>/dev/null || true

    # Wait for shutdown
    sleep 1

    # Check pmtr exited
    if ! kill -0 $PMTR_PID 2>/dev/null; then
        pass "pmtr exited on SIGTERM"
    else
        fail "pmtr did not exit"
        kill -9 $PMTR_PID 2>/dev/null || true
    fi

    # Check job was terminated
    if ! pgrep -f "sleep 60" > /dev/null; then
        pass "job terminated on shutdown"
    else
        fail "job still running after shutdown"
        pkill -9 -f "sleep 60" 2>/dev/null || true
    fi
}

# Test 3: Multiple jobs
test_multiple_jobs() {
    echo "Test: multiple jobs"

    cat > "$TEST_DIR/multi.conf" << EOF
job {
    name job1
    cmd /bin/sleep 61
}
job {
    name job2
    cmd /bin/sleep 62
}
EOF

    "$PMTR" -F -c "$TEST_DIR/multi.conf" &
    PMTR_PID=$!

    sleep 1

    JOB1_RUNNING=$(pgrep -f "sleep 61" || true)
    JOB2_RUNNING=$(pgrep -f "sleep 62" || true)

    if [ -n "$JOB1_RUNNING" ] && [ -n "$JOB2_RUNNING" ]; then
        pass "both jobs started"
    else
        fail "not all jobs started (job1=$JOB1_RUNNING, job2=$JOB2_RUNNING)"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1

    pkill -9 -f "sleep 6[12]" 2>/dev/null || true
}

# Test 4: Disabled job
test_disabled_job() {
    echo "Test: disabled job"

    cat > "$TEST_DIR/disabled.conf" << EOF
job {
    name enabled_job
    cmd /bin/sleep 63
}
job {
    name disabled_job
    cmd /bin/sleep 64
    disable
}
EOF

    "$PMTR" -F -c "$TEST_DIR/disabled.conf" &
    PMTR_PID=$!

    sleep 1

    ENABLED_RUNNING=$(pgrep -f "sleep 63" || true)
    DISABLED_RUNNING=$(pgrep -f "sleep 64" || true)

    if [ -n "$ENABLED_RUNNING" ]; then
        pass "enabled job started"
    else
        fail "enabled job did not start"
    fi

    if [ -z "$DISABLED_RUNNING" ]; then
        pass "disabled job not started"
    else
        fail "disabled job should not have started"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1

    pkill -9 -f "sleep 6[34]" 2>/dev/null || true
}

# Test 5: Job respawn
test_job_respawn() {
    echo "Test: job respawn"

    # Use a marker file to track respawns instead of pgrep
    cat > "$TEST_DIR/respawn.conf" << EOF
job {
    name respawner
    cmd /bin/sh -c "echo \$\$ >> $TEST_DIR/pids.txt; sleep 999"
}
EOF

    rm -f "$TEST_DIR/pids.txt"

    "$PMTR" -F -c "$TEST_DIR/respawn.conf" &
    PMTR_PID=$!

    # pmtr has a 10-second anti-spin delay (SHORT_DELAY) - if job runs < 10s,
    # respawn is delayed. Wait 11s so respawn is immediate after kill.
    sleep 11

    if [ ! -f "$TEST_DIR/pids.txt" ]; then
        fail "job did not start initially"
        kill -TERM $PMTR_PID 2>/dev/null || true
        return
    fi

    INITIAL_PID=$(head -1 "$TEST_DIR/pids.txt")
    pass "job started with PID $INITIAL_PID"

    # Kill the job (pmtr should respawn it immediately since job ran > 10s)
    kill -9 $INITIAL_PID 2>/dev/null || true

    # Wait for pmtr to notice and respawn
    sleep 2

    # Check for new PID in file
    PID_COUNT=$(wc -l < "$TEST_DIR/pids.txt" | tr -d ' ')

    if [ "$PID_COUNT" -ge 2 ]; then
        NEW_PID=$(tail -1 "$TEST_DIR/pids.txt")
        pass "job respawned with new PID $NEW_PID"
    else
        fail "job did not respawn (only $PID_COUNT PIDs recorded)"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1

    # Cleanup any remaining processes
    kill -9 $INITIAL_PID 2>/dev/null || true
    [ -n "$NEW_PID" ] && kill -9 $NEW_PID 2>/dev/null || true
}

# Test 6: Once flag (no respawn)
test_once_flag() {
    echo "Test: once flag (no respawn)"

    cat > "$TEST_DIR/once.conf" << EOF
job {
    name run_once
    cmd /bin/true
    once
}
EOF

    "$PMTR" -F -c "$TEST_DIR/once.conf" &
    PMTR_PID=$!

    sleep 2

    # Job should have run and exited, pmtr should still be running
    # but not respawning the job

    if kill -0 $PMTR_PID 2>/dev/null; then
        pass "pmtr still running after once job completed"
    else
        fail "pmtr exited unexpectedly"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
}

# Test 7: SIGHUP config reload
test_config_reload() {
    echo "Test: config reload (SIGHUP)"

    cat > "$TEST_DIR/reload.conf" << EOF
job {
    name original_job
    cmd /bin/sleep 66
}
EOF

    "$PMTR" -F -c "$TEST_DIR/reload.conf" &
    PMTR_PID=$!

    sleep 1

    if pgrep -f "sleep 66" > /dev/null; then
        pass "original job started"
    else
        fail "original job did not start"
    fi

    # Update config
    cat > "$TEST_DIR/reload.conf" << EOF
job {
    name new_job
    cmd /bin/sleep 67
}
EOF

    # Send SIGHUP to reload
    kill -HUP $PMTR_PID

    sleep 2

    # Old job should be gone, new job should be running
    OLD_RUNNING=$(pgrep -f "sleep 66" || true)
    NEW_RUNNING=$(pgrep -f "sleep 67" || true)

    if [ -z "$OLD_RUNNING" ]; then
        pass "old job terminated after reload"
    else
        fail "old job still running after reload"
    fi

    if [ -n "$NEW_RUNNING" ]; then
        pass "new job started after reload"
    else
        fail "new job did not start after reload"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1

    pkill -9 -f "sleep 6[67]" 2>/dev/null || true
}

# Test 8: Pidfile creation
test_pidfile() {
    echo "Test: pidfile creation"

    cat > "$TEST_DIR/pid.conf" << EOF
job {
    name pidtest
    cmd /bin/sleep 68
}
EOF

    "$PMTR" -F -p "$TEST_DIR/pmtr.pid" -c "$TEST_DIR/pid.conf" &
    PMTR_PID=$!

    sleep 1

    if [ -f "$TEST_DIR/pmtr.pid" ]; then
        PIDFILE_CONTENT=$(cat "$TEST_DIR/pmtr.pid")
        if [ "$PIDFILE_CONTENT" = "$PMTR_PID" ]; then
            pass "pidfile contains correct PID"
        else
            fail "pidfile has wrong PID (got $PIDFILE_CONTENT, expected $PMTR_PID)"
        fi
    else
        fail "pidfile not created"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1

    if [ ! -f "$TEST_DIR/pmtr.pid" ]; then
        pass "pidfile removed on exit"
    else
        fail "pidfile not cleaned up"
    fi

    pkill -9 -f "sleep 68" 2>/dev/null || true
}

# Run all tests
echo "=== pmtr end-to-end tests ==="
echo ""

test_config_validation
test_job_execution
test_multiple_jobs
test_disabled_job
test_job_respawn
test_once_flag
test_config_reload
test_pidfile

echo ""
echo "=== Results: $PASSED passed, $FAILED failed ==="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
