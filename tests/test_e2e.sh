#!/bin/bash
#
# End-to-end integration test for pmtr
# Runs the actual pmtr binary and verifies behavior
#
set -e

PMTR="${1:-./src/pmtr}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/e2e_configs"
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

# Per-test cleanup to ensure test isolation
# Called at the start of each test to kill any orphaned processes from previous tests
test_cleanup() {
    pkill -f "pmtr.*$TEST_DIR" 2>/dev/null || true
    # Kill any sleep processes from our tests (sleep 60-69)
    pkill -9 -f "sleep 6[0-9]" 2>/dev/null || true
    pkill -9 -f "sleep 999" 2>/dev/null || true
}

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

    if "$PMTR" -t -c "$CONFIG_DIR/single_job.conf" 2>/dev/null; then
        pass "valid config accepted"
    else
        fail "valid config rejected"
    fi

    # pmtr -t always exits 0, so check stderr for parse error
    if "$PMTR" -t -c "$CONFIG_DIR/invalid.conf" 2>&1 | grep -q "parse failed"; then
        pass "invalid config rejected"
    else
        fail "invalid config accepted"
    fi
}

# Test 2: Basic job execution
test_job_execution() {
    echo "Test: basic job execution"
    test_cleanup

    "$PMTR" -F -c "$CONFIG_DIR/single_job.conf" &
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
    test_cleanup

    "$PMTR" -F -c "$CONFIG_DIR/multi_job.conf" &
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
    test_cleanup

    "$PMTR" -F -c "$CONFIG_DIR/disabled.conf" &
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
    test_cleanup

    # This test needs a dynamic config to track PIDs via file
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
    test_cleanup

    "$PMTR" -F -c "$CONFIG_DIR/once.conf" &
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
    test_cleanup

    # Copy initial config to temp location (will be modified)
    cp "$CONFIG_DIR/reload_before.conf" "$TEST_DIR/reload.conf"

    "$PMTR" -F -c "$TEST_DIR/reload.conf" &
    PMTR_PID=$!

    sleep 1

    if pgrep -f "sleep 66" > /dev/null; then
        pass "original job started"
    else
        fail "original job did not start"
    fi

    # Update config
    cp "$CONFIG_DIR/reload_after.conf" "$TEST_DIR/reload.conf"

    # Send SIGHUP to reload
    kill -HUP $PMTR_PID 2>/dev/null || true

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
    test_cleanup

    "$PMTR" -F -p "$TEST_DIR/pmtr.pid" -c "$CONFIG_DIR/single_job.conf" &
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

    pkill -9 -f "sleep 60" 2>/dev/null || true
}

# Test 9: Working directory
test_working_directory() {
    echo "Test: working directory (dir)"
    test_cleanup

    # Create a working directory and a job that writes to a relative path
    mkdir -p "$TEST_DIR/workdir"

    cat > "$TEST_DIR/dir.conf" << EOF
job {
    name dirtest
    dir $TEST_DIR/workdir
    cmd /bin/sh -c "pwd > output.txt; sleep 70"
}
EOF

    "$PMTR" -F -c "$TEST_DIR/dir.conf" &
    PMTR_PID=$!

    sleep 2

    if [ -f "$TEST_DIR/workdir/output.txt" ]; then
        DIR_CONTENT=$(cat "$TEST_DIR/workdir/output.txt")
        if [ "$DIR_CONTENT" = "$TEST_DIR/workdir" ]; then
            pass "job ran in correct working directory"
        else
            fail "job ran in wrong directory: $DIR_CONTENT"
        fi
    else
        fail "job did not create output file in working directory"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
    pkill -9 -f "sleep 70" 2>/dev/null || true
}

# Test 10: Environment variables
test_environment_variables() {
    echo "Test: environment variables (env)"
    test_cleanup

    cat > "$TEST_DIR/env.conf" << EOF
job {
    name envtest
    env TEST_VAR1=hello
    env TEST_VAR2=world
    cmd /bin/sh -c "echo \$TEST_VAR1-\$TEST_VAR2 > $TEST_DIR/env_output.txt; sleep 71"
}
EOF

    "$PMTR" -F -c "$TEST_DIR/env.conf" &
    PMTR_PID=$!

    sleep 2

    if [ -f "$TEST_DIR/env_output.txt" ]; then
        ENV_CONTENT=$(cat "$TEST_DIR/env_output.txt")
        if [ "$ENV_CONTENT" = "hello-world" ]; then
            pass "environment variables set correctly"
        else
            fail "environment variables wrong: got '$ENV_CONTENT', expected 'hello-world'"
        fi
    else
        fail "job did not create output file"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
    pkill -9 -f "sleep 71" 2>/dev/null || true
}

# Test 11: Stdout/stderr redirection
test_output_redirection() {
    echo "Test: stdout/stderr redirection (out/err)"
    test_cleanup

    cat > "$TEST_DIR/redir.conf" << EOF
job {
    name redirtest
    out $TEST_DIR/stdout.txt
    err $TEST_DIR/stderr.txt
    cmd /bin/sh -c "echo stdout_msg; echo stderr_msg >&2; sleep 72"
}
EOF

    "$PMTR" -F -c "$TEST_DIR/redir.conf" &
    PMTR_PID=$!

    sleep 2

    if [ -f "$TEST_DIR/stdout.txt" ] && grep -q "stdout_msg" "$TEST_DIR/stdout.txt"; then
        pass "stdout redirected correctly"
    else
        fail "stdout not redirected"
    fi

    if [ -f "$TEST_DIR/stderr.txt" ] && grep -q "stderr_msg" "$TEST_DIR/stderr.txt"; then
        pass "stderr redirected correctly"
    else
        fail "stderr not redirected"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
    pkill -9 -f "sleep 72" 2>/dev/null || true
}

# Test 12: Wait flag (job blocks startup)
test_wait_flag() {
    echo "Test: wait flag (blocks startup)"
    test_cleanup

    # First job has wait flag and runs briefly, second job should start after
    cat > "$TEST_DIR/wait.conf" << EOF
job {
    name setup_job
    wait
    cmd /bin/sh -c "echo started > $TEST_DIR/setup_done.txt"
}
job {
    name main_job
    cmd /bin/sh -c "if [ -f $TEST_DIR/setup_done.txt ]; then echo ok > $TEST_DIR/main_check.txt; fi; sleep 73"
}
EOF

    rm -f "$TEST_DIR/setup_done.txt" "$TEST_DIR/main_check.txt"

    "$PMTR" -F -c "$TEST_DIR/wait.conf" &
    PMTR_PID=$!

    sleep 3

    if [ -f "$TEST_DIR/main_check.txt" ]; then
        pass "wait flag blocked second job until first completed"
    else
        fail "wait flag did not block second job (setup_done exists: $([ -f $TEST_DIR/setup_done.txt ] && echo yes || echo no))"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
    pkill -9 -f "sleep 73" 2>/dev/null || true
}

# Test 13: Job ordering
test_job_order() {
    echo "Test: job ordering (order keyword)"
    test_cleanup

    # Jobs with explicit order - lower order starts first
    cat > "$TEST_DIR/order.conf" << EOF
job {
    name third
    order 3
    cmd /bin/sh -c "date +%s%N >> $TEST_DIR/order.txt; sleep 74"
}
job {
    name first
    order 1
    cmd /bin/sh -c "date +%s%N >> $TEST_DIR/order.txt; sleep 75"
}
job {
    name second
    order 2
    cmd /bin/sh -c "date +%s%N >> $TEST_DIR/order.txt; sleep 76"
}
EOF

    rm -f "$TEST_DIR/order.txt"

    "$PMTR" -F -c "$TEST_DIR/order.conf" &
    PMTR_PID=$!

    sleep 2

    if [ -f "$TEST_DIR/order.txt" ]; then
        LINE_COUNT=$(wc -l < "$TEST_DIR/order.txt" | tr -d ' ')
        if [ "$LINE_COUNT" -ge 3 ]; then
            # Check timestamps are in ascending order (jobs started in order)
            SORTED=$(sort -n "$TEST_DIR/order.txt")
            ORIGINAL=$(cat "$TEST_DIR/order.txt")
            if [ "$SORTED" = "$ORIGINAL" ]; then
                pass "jobs started in correct order"
            else
                fail "jobs did not start in order"
            fi
        else
            fail "not all jobs recorded timestamps"
        fi
    else
        fail "order tracking file not created"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
    pkill -9 -f "sleep 7[456]" 2>/dev/null || true
}

# Test 14: Exit code 33 prevents respawn
test_exit_code_no_restart() {
    echo "Test: exit code 33 prevents respawn"
    test_cleanup

    cat > "$TEST_DIR/exit33.conf" << EOF
job {
    name exitjob
    cmd /bin/sh -c "echo \$\$ >> $TEST_DIR/exit33_pids.txt; exit 33"
}
EOF

    rm -f "$TEST_DIR/exit33_pids.txt"

    "$PMTR" -F -c "$TEST_DIR/exit33.conf" &
    PMTR_PID=$!

    # Wait for job to run and potentially respawn
    sleep 3

    if [ -f "$TEST_DIR/exit33_pids.txt" ]; then
        PID_COUNT=$(wc -l < "$TEST_DIR/exit33_pids.txt" | tr -d ' ')
        if [ "$PID_COUNT" -eq 1 ]; then
            pass "job with exit 33 was not respawned"
        else
            fail "job was respawned $PID_COUNT times (expected 1)"
        fi
    else
        fail "job did not run at all"
    fi

    kill -TERM $PMTR_PID 2>/dev/null || true
    sleep 1
}

# Test 15: Graceful shutdown terminates all jobs
test_graceful_shutdown() {
    echo "Test: graceful shutdown terminates all jobs"
    test_cleanup

    "$PMTR" -F -c "$CONFIG_DIR/multi_job.conf" &
    PMTR_PID=$!

    sleep 1

    # Verify jobs are running
    JOB1_BEFORE=$(pgrep -f "sleep 61" || true)
    JOB2_BEFORE=$(pgrep -f "sleep 62" || true)

    if [ -z "$JOB1_BEFORE" ] || [ -z "$JOB2_BEFORE" ]; then
        fail "jobs did not start"
        kill -9 $PMTR_PID 2>/dev/null || true
        return
    fi

    # Send SIGTERM for graceful shutdown
    kill -TERM $PMTR_PID

    # Wait for graceful shutdown (should be < 10 seconds)
    sleep 2

    # Check all processes are gone
    if ! kill -0 $PMTR_PID 2>/dev/null; then
        pass "pmtr terminated gracefully"
    else
        fail "pmtr still running after SIGTERM"
        kill -9 $PMTR_PID 2>/dev/null || true
    fi

    JOB1_AFTER=$(pgrep -f "sleep 61" || true)
    JOB2_AFTER=$(pgrep -f "sleep 62" || true)

    if [ -z "$JOB1_AFTER" ] && [ -z "$JOB2_AFTER" ]; then
        pass "all jobs terminated on shutdown"
    else
        fail "some jobs still running after shutdown"
        pkill -9 -f "sleep 6[12]" 2>/dev/null || true
    fi
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
test_working_directory
test_environment_variables
test_output_redirection
test_wait_flag
test_job_order
test_exit_code_no_restart
test_graceful_shutdown

echo ""
echo "=== Results: $PASSED passed, $FAILED failed ==="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
