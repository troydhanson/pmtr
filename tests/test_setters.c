/*
 * Unit Tests for pmtr Setter Functions (job.c)
 * Tests the set_* functions used during config parsing
 */

#define _GNU_SOURCE  /* Required for CPU_SET macros */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/resource.h>
#include "test_framework.h"
#include "test_helpers.h"

/*
 * set_name Tests
 */
TEST_CASE(set_name_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_name(&ps, "test_job");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.name);
    TEST_ASSERT_STR_EQ("test_job", job.name);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_name_duplicate) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_name(&ps, "first");
    TEST_ASSERT_EQ(0, ps.rc);

    /* Second set_name should fail */
    set_name(&ps, "second");
    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_name_with_special_chars) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_name(&ps, "my-job_123");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_STR_EQ("my-job_123", job.name);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_cmd Tests
 */
TEST_CASE(set_cmd_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cmd(&ps, "/usr/bin/test");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(1, utarray_len(&job.cmdv));
    char *cmd = get_cmd_arg_at(&job, 0);
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_STR_EQ("/usr/bin/test", cmd);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_dir Tests
 */
TEST_CASE(set_dir_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_dir(&ps, "/var/log");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.dir);
    TEST_ASSERT_STR_EQ("/var/log", job.dir);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_dir_duplicate) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_dir(&ps, "/first");
    TEST_ASSERT_EQ(0, ps.rc);

    set_dir(&ps, "/second");
    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_out Tests
 */
TEST_CASE(set_out_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_out(&ps, "/var/log/output.log");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.out);
    TEST_ASSERT_STR_EQ("/var/log/output.log", job.out);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_out_syslog) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_out(&ps, "syslog");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.out);
    TEST_ASSERT_STR_EQ("syslog", job.out);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_err Tests
 */
TEST_CASE(set_err_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_err(&ps, "/var/log/error.log");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.err);
    TEST_ASSERT_STR_EQ("/var/log/error.log", job.err);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_in Tests
 */
TEST_CASE(set_in_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_in(&ps, "/dev/null");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_NOT_NULL(job.in);
    TEST_ASSERT_STR_EQ("/dev/null", job.in);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_user Tests
 */
TEST_CASE(set_user_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_user(&ps, "nobody");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_STR_EQ("nobody", job.user);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_user_too_long) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    /* Create a very long username */
    char long_user[200];
    memset(long_user, 'a', sizeof(long_user) - 1);
    long_user[sizeof(long_user) - 1] = '\0';

    set_user(&ps, long_user);

    TEST_ASSERT_EQ(-1, ps.rc);  /* Should fail */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_ord Tests
 */
TEST_CASE(set_ord_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ord(&ps, "10");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(10, job.order);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ord_negative) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ord(&ps, "-5");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(-5, job.order);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ord_non_numeric) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ord(&ps, "abc");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_nice Tests
 */
TEST_CASE(set_nice_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "10");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(10, job.nice);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_negative) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "-10");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(-10, job.nice);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_min_valid) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "-20");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(-20, job.nice);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_max_valid) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "19");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(19, job.nice);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_too_low) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "-21");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_too_high) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "20");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_nice_non_numeric) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_nice(&ps, "high");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_dis Tests
 */
TEST_CASE(set_dis_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    TEST_ASSERT_EQ(0, job.disabled);  /* Initially not disabled */

    set_dis(&ps);

    TEST_ASSERT_EQ(1, job.disabled);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_wait Tests
 */
TEST_CASE(set_wait_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    TEST_ASSERT_EQ(0, job.wait);

    set_wait(&ps);

    TEST_ASSERT_EQ(1, job.wait);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_once Tests
 */
TEST_CASE(set_once_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    TEST_ASSERT_EQ(0, job.once);

    set_once(&ps);

    TEST_ASSERT_EQ(1, job.once);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_env Tests
 */
TEST_CASE(set_env_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_env(&ps, "DEBUG=1");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(1, utarray_len(&job.envv));
    char *env = get_env_at(&job, 0);
    TEST_ASSERT_NOT_NULL(env);
    TEST_ASSERT_STR_EQ("DEBUG=1", env);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_env_multiple) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_env(&ps, "VAR1=value1");
    set_env(&ps, "VAR2=value2");
    set_env(&ps, "VAR3=value3");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(3, utarray_len(&job.envv));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_env_no_equals) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_env(&ps, "INVALID_NO_EQUALS");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_env_empty_value) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_env(&ps, "EMPTY=");

    TEST_ASSERT_EQ(0, ps.rc);  /* Empty value is valid */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_bounce Tests
 * Note: set_bounce modifies the string in place, so we use char arrays, not string literals
 */
TEST_CASE(set_bounce_seconds) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "30s";  /* Mutable - set_bounce modifies in place */

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(30, job.bounce_interval);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_bounce_minutes) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "5m";

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(300, job.bounce_interval);  /* 5 * 60 = 300 */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_bounce_hours) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "2h";

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(7200, job.bounce_interval);  /* 2 * 60 * 60 = 7200 */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_bounce_days) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "1d";

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(86400, job.bounce_interval);  /* 24 * 60 * 60 = 86400 */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_bounce_invalid_unit) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "10x";

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_bounce_non_numeric) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;
    char timespec[] = "abcs";

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_bounce(&ps, timespec);

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_cpu Tests
 */
TEST_CASE(set_cpu_single) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(1, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_range) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0-3");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(1, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(3, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(4, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_list) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0,2,4");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(1, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(3, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(4, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_mixed) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0,2-4,8");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(1, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(3, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(4, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(5, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(8, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_hex_simple) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0x1");  /* CPU 0 */

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(1, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_hex_multiple) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0xf");  /* CPUs 0-3 */

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT(CPU_ISSET(0, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(1, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job.cpuset));
    TEST_ASSERT(CPU_ISSET(3, &job.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(4, &job.cpuset));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_hex_uppercase) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0xAB");

    TEST_ASSERT_EQ(0, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_hex_empty) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0x");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_hex_invalid_char) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0xGH");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_invalid_range) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "5-3");  /* Invalid: end < start */

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_double_dash) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0--3");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_cpu_trailing_comma) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_cpu(&ps, "0,");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * set_ulimit Tests
 */
TEST_CASE(set_ulimit_nofile_flag) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-n", "1024");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(1, utarray_len(&job.rlim));

    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job.rlim);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(RLIMIT_NOFILE, rt->id);
    TEST_ASSERT_EQ(1024, rt->rlim.rlim_cur);
    TEST_ASSERT_EQ(1024, rt->rlim.rlim_max);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_nofile_name) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "RLIMIT_NOFILE", "2048");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(1, utarray_len(&job.rlim));

    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job.rlim);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(RLIMIT_NOFILE, rt->id);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_core) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-c", "0");

    TEST_ASSERT_EQ(0, ps.rc);

    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job.rlim);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(RLIMIT_CORE, rt->id);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_infinity) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-c", "infinity");

    TEST_ASSERT_EQ(0, ps.rc);

    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job.rlim);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(RLIM_INFINITY, rt->rlim.rlim_cur);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_unlimited) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-c", "unlimited");

    TEST_ASSERT_EQ(0, ps.rc);

    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job.rlim);
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(RLIM_INFINITY, rt->rlim.rlim_cur);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_nofile_infinity_rejected) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-n", "infinity");

    TEST_ASSERT_EQ(-1, ps.rc);  /* -n infinity is not allowed */

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_unknown_resource) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-x", "100");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_non_numeric_value) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-n", "abc");

    TEST_ASSERT_EQ(-1, ps.rc);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(set_ulimit_multiple) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    set_ulimit(&ps, "-n", "1024");
    set_ulimit(&ps, "-c", "0");
    set_ulimit(&ps, "-m", "1000000");

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(3, utarray_len(&job.rlim));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * unquote Tests
 */
TEST_CASE(unquote_basic) {
    char str[] = "\"hello\"";
    char *result = unquote(str);
    TEST_ASSERT_STR_EQ("hello", result);
}

TEST_CASE(unquote_with_spaces) {
    char str[] = "\"hello world\"";
    char *result = unquote(str);
    TEST_ASSERT_STR_EQ("hello world", result);
}

TEST_CASE(unquote_empty) {
    char str[] = "\"\"";
    char *result = unquote(str);
    TEST_ASSERT_STR_EQ("", result);
}

/*
 * fpath Tests
 */
TEST_CASE(fpath_absolute) {
    job_t job;
    job_ini(&job);

    char *result = fpath(&job, "/absolute/path");
    TEST_ASSERT_STR_EQ("/absolute/path", result);

    job_fin(&job);
}

TEST_CASE(fpath_relative_no_dir) {
    job_t job;
    job_ini(&job);
    /* job.dir is NULL */

    char *result = fpath(&job, "relative/path");
    TEST_ASSERT_STR_EQ("relative/path", result);

    job_fin(&job);
}

TEST_CASE(fpath_relative_with_dir) {
    job_t job;
    job_ini(&job);
    job.dir = strdup("/home/user");

    char *result = fpath(&job, "relative/path");
    TEST_ASSERT_STR_EQ("/home/user/relative/path", result);

    job_fin(&job);
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("set_name");
    RUN_TEST(set_name_basic);
    RUN_TEST(set_name_duplicate);
    RUN_TEST(set_name_with_special_chars);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_cmd");
    RUN_TEST(set_cmd_basic);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_dir");
    RUN_TEST(set_dir_basic);
    RUN_TEST(set_dir_duplicate);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_out");
    RUN_TEST(set_out_basic);
    RUN_TEST(set_out_syslog);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_err");
    RUN_TEST(set_err_basic);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_in");
    RUN_TEST(set_in_basic);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_user");
    RUN_TEST(set_user_basic);
    RUN_TEST(set_user_too_long);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_ord");
    RUN_TEST(set_ord_basic);
    RUN_TEST(set_ord_negative);
    RUN_TEST(set_ord_non_numeric);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_nice");
    RUN_TEST(set_nice_basic);
    RUN_TEST(set_nice_negative);
    RUN_TEST(set_nice_min_valid);
    RUN_TEST(set_nice_max_valid);
    RUN_TEST(set_nice_too_low);
    RUN_TEST(set_nice_too_high);
    RUN_TEST(set_nice_non_numeric);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_dis/set_wait/set_once");
    RUN_TEST(set_dis_basic);
    RUN_TEST(set_wait_basic);
    RUN_TEST(set_once_basic);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_env");
    RUN_TEST(set_env_basic);
    RUN_TEST(set_env_multiple);
    RUN_TEST(set_env_no_equals);
    RUN_TEST(set_env_empty_value);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_bounce");
    RUN_TEST(set_bounce_seconds);
    RUN_TEST(set_bounce_minutes);
    RUN_TEST(set_bounce_hours);
    RUN_TEST(set_bounce_days);
    RUN_TEST(set_bounce_invalid_unit);
    RUN_TEST(set_bounce_non_numeric);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_cpu");
    RUN_TEST(set_cpu_single);
    RUN_TEST(set_cpu_range);
    RUN_TEST(set_cpu_list);
    RUN_TEST(set_cpu_mixed);
    RUN_TEST(set_cpu_hex_simple);
    RUN_TEST(set_cpu_hex_multiple);
    RUN_TEST(set_cpu_hex_uppercase);
    RUN_TEST(set_cpu_hex_empty);
    RUN_TEST(set_cpu_hex_invalid_char);
    RUN_TEST(set_cpu_invalid_range);
    RUN_TEST(set_cpu_double_dash);
    RUN_TEST(set_cpu_trailing_comma);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("set_ulimit");
    RUN_TEST(set_ulimit_nofile_flag);
    RUN_TEST(set_ulimit_nofile_name);
    RUN_TEST(set_ulimit_core);
    RUN_TEST(set_ulimit_infinity);
    RUN_TEST(set_ulimit_unlimited);
    RUN_TEST(set_ulimit_nofile_infinity_rejected);
    RUN_TEST(set_ulimit_unknown_resource);
    RUN_TEST(set_ulimit_non_numeric_value);
    RUN_TEST(set_ulimit_multiple);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("unquote");
    RUN_TEST(unquote_basic);
    RUN_TEST(unquote_with_spaces);
    RUN_TEST(unquote_empty);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("fpath");
    RUN_TEST(fpath_absolute);
    RUN_TEST(fpath_relative_no_dir);
    RUN_TEST(fpath_relative_with_dir);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
