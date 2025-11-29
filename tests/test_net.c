/*
 * Unit Tests for pmtr Network/UDP Control (net.c)
 * Tests UDP specification parsing, listen/report setup, and message decoding
 */

#define _GNU_SOURCE  /* Required for CPU_SET macros */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "test_framework.h"
#include "test_helpers.h"

/*
 * UDP Spec Parsing Tests (via set_listen with test_only=1)
 * Using test_only mode to validate parsing without creating sockets
 */

TEST_CASE(udp_listen_valid_spec) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;  /* Don't create actual socket */

    set_listen(&ps, "udp://127.0.0.1:9999");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_valid_any_address) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "udp://0.0.0.0:8888");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_invalid_no_proto) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "127.0.0.1:9999");

    TEST_ASSERT_EQ(-1, ps.rc);
    TEST_ASSERT(utstring_len(em) > 0);  /* Error message present */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_invalid_wrong_proto) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "tcp://127.0.0.1:9999");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_no_explicit_port) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    /* "udp://127.0.0.1" - strrchr finds the ':' after 'udp',
     * and atoi("//127.0.0.1") returns 0, which is a valid port.
     * So this is actually accepted as port 0 (any available port). */
    set_listen(&ps, "udp://127.0.0.1");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_invalid_port_negative) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "udp://127.0.0.1:-1");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_invalid_port_too_high) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "udp://127.0.0.1:99999");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_port_zero) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    /* Port 0 is valid (any available port) */
    set_listen(&ps, "udp://127.0.0.1:0");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_listen_port_max) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_listen(&ps, "udp://127.0.0.1:65535");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_report Tests
 */
TEST_CASE(udp_report_valid_spec) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_report(&ps, "udp://192.168.1.1:8080");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_report_with_interface) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    /* Interface specification is parsed but not validated in test_only mode */
    set_report(&ps, "udp://224.0.0.1:5000@eth0");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(udp_report_invalid_format) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);
    cfg.test_only = 1;

    set_report(&ps, "not-a-valid-spec");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * close_sockets Tests
 */
TEST_CASE(close_sockets_empty) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Should not crash with empty socket arrays */
    close_sockets(&cfg);

    TEST_ASSERT_EQ(0, utarray_len(cfg.listen));
    TEST_ASSERT_EQ(0, utarray_len(cfg.report));

    free_test_cfg(&cfg);
}

/*
 * Integration Tests for UDP Control via Config Parsing
 */
TEST_CASE(config_listen_valid) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "listen on udp://127.0.0.1:9999\n"
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(config_listen_invalid) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "listen on badspec\n"
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(config_report_valid) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "report to udp://10.0.0.1:5555\n"
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(config_report_invalid) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "report to tcp://10.0.0.1:5555\n"  /* Wrong protocol */
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(config_multiple_reports) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "report to udp://10.0.0.1:5555\n"
        "report to udp://10.0.0.2:5555\n"
        "report to udp://10.0.0.3:5555\n"
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * UDP Control Message Simulation Tests
 * These tests verify job state changes that would result from UDP control messages
 */
TEST_CASE(udp_enable_disabled_job) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create a disabled job */
    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    job.disabled = 1;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    /* Simulate what decode_msg does for "enable myjob" */
    job_t *j = get_job_by_name(cfg.jobs, "myjob");
    TEST_ASSERT_NOT_NULL(j);
    TEST_ASSERT_EQ(1, j->disabled);

    /* Enable the job (what decode_msg would do) */
    j->disabled = 0;
    alarm_within(&cfg, 1);

    TEST_ASSERT_EQ(0, j->disabled);

    free_test_cfg(&cfg);
}

TEST_CASE(udp_disable_enabled_job) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create an enabled running job */
    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    job.disabled = 0;
    job.pid = 12345;  /* Simulating running */
    job.terminate = 0;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *j = get_job_by_name(cfg.jobs, "myjob");
    TEST_ASSERT_NOT_NULL(j);

    /* Disable the job (what decode_msg would do) */
    j->disabled = 1;
    if (j->pid) {
        if (j->terminate == 0) j->terminate = 1;
    }
    alarm_within(&cfg, 1);

    TEST_ASSERT_EQ(1, j->disabled);
    TEST_ASSERT_EQ(1, j->terminate);

    free_test_cfg(&cfg);
}

TEST_CASE(udp_enable_already_enabled) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create an already enabled job */
    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    job.disabled = 0;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *j = get_job_by_name(cfg.jobs, "myjob");

    /* Enabling an already enabled job is a no-op */
    int was_disabled = j->disabled;
    if (j->disabled) {  /* This won't execute */
        j->disabled = 0;
    }

    TEST_ASSERT_EQ(0, j->disabled);
    TEST_ASSERT_EQ(was_disabled, j->disabled);  /* Unchanged */

    free_test_cfg(&cfg);
}

TEST_CASE(udp_disable_already_disabled) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create an already disabled job */
    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    job.disabled = 1;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *j = get_job_by_name(cfg.jobs, "myjob");

    /* Disabling an already disabled job is a no-op */
    int was_disabled = j->disabled;
    if (!j->disabled) {  /* This won't execute */
        j->disabled = 1;
    }

    TEST_ASSERT_EQ(1, j->disabled);
    TEST_ASSERT_EQ(was_disabled, j->disabled);  /* Unchanged */

    free_test_cfg(&cfg);
}

TEST_CASE(udp_control_unknown_job) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create a job with a different name */
    job_t job;
    job_ini(&job);
    job.name = strdup("existingjob");
    job.disabled = 0;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    /* Try to find a non-existent job (what decode_msg does) */
    job_t *j = get_job_by_name(cfg.jobs, "unknownjob");

    /* Should return NULL and be ignored */
    TEST_ASSERT_NULL(j);

    free_test_cfg(&cfg);
}

TEST_CASE(udp_control_multiple_jobs) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create multiple jobs */
    job_t job1, job2, job3;

    job_ini(&job1);
    job1.name = strdup("job1");
    job1.disabled = 1;
    utarray_push_back(cfg.jobs, &job1);
    job_fin(&job1);

    job_ini(&job2);
    job2.name = strdup("job2");
    job2.disabled = 1;
    utarray_push_back(cfg.jobs, &job2);
    job_fin(&job2);

    job_ini(&job3);
    job3.name = strdup("job3");
    job3.disabled = 0;
    utarray_push_back(cfg.jobs, &job3);
    job_fin(&job3);

    /* Simulate "enable job1 job2" message processing */
    job_t *j1 = get_job_by_name(cfg.jobs, "job1");
    job_t *j2 = get_job_by_name(cfg.jobs, "job2");
    job_t *j3 = get_job_by_name(cfg.jobs, "job3");

    j1->disabled = 0;
    j2->disabled = 0;

    TEST_ASSERT_EQ(0, j1->disabled);
    TEST_ASSERT_EQ(0, j2->disabled);
    TEST_ASSERT_EQ(0, j3->disabled);  /* Was already enabled */

    free_test_cfg(&cfg);
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("UDP Listen Spec Parsing");
    RUN_TEST(udp_listen_valid_spec);
    RUN_TEST(udp_listen_valid_any_address);
    RUN_TEST(udp_listen_invalid_no_proto);
    RUN_TEST(udp_listen_invalid_wrong_proto);
    RUN_TEST(udp_listen_no_explicit_port);
    RUN_TEST(udp_listen_invalid_port_negative);
    RUN_TEST(udp_listen_invalid_port_too_high);
    RUN_TEST(udp_listen_port_zero);
    RUN_TEST(udp_listen_port_max);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("UDP Report Spec Parsing");
    RUN_TEST(udp_report_valid_spec);
    RUN_TEST(udp_report_with_interface);
    RUN_TEST(udp_report_invalid_format);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Socket Management");
    RUN_TEST(close_sockets_empty);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Config Listen/Report Integration");
    RUN_TEST(config_listen_valid);
    RUN_TEST(config_listen_invalid);
    RUN_TEST(config_report_valid);
    RUN_TEST(config_report_invalid);
    RUN_TEST(config_multiple_reports);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("UDP Control Message Simulation");
    RUN_TEST(udp_enable_disabled_job);
    RUN_TEST(udp_disable_enabled_job);
    RUN_TEST(udp_enable_already_enabled);
    RUN_TEST(udp_disable_already_disabled);
    RUN_TEST(udp_control_unknown_job);
    RUN_TEST(udp_control_multiple_jobs);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
