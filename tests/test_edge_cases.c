/*
 * Edge Case Tests for pmtr Configuration
 * Tests boundary conditions, unusual inputs, and corner cases
 */

#define _GNU_SOURCE  /* Required for CPU_SET macros */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include "test_framework.h"
#include "test_helpers.h"

/*
 * Name Edge Cases
 */
TEST_CASE(edge_name_single_char) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name a\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("a", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_name_with_dots) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name my.job.name\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("my.job.name", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_name_numbers_only) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name 12345\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("12345", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Path Edge Cases
 */
TEST_CASE(edge_cmd_long_path) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Create a long but valid path */
    char long_path[256];
    snprintf(long_path, sizeof(long_path),
        "/usr/local/very/long/path/to/some/deeply/nested/directory/bin/program");

    char config[512];
    snprintf(config, sizeof(config),
        "job {\n"
        "  name longpath\n"
        "  cmd %s\n"
        "}\n", long_path);

    cfg.file = strdup(create_temp_config(config));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ(long_path, get_cmd_arg_at(job, 0));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_cmd_special_chars_in_path) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name special\n"
        "  cmd /opt/my-app_v2.1/bin/run.sh\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("/opt/my-app_v2.1/bin/run.sh", get_cmd_arg_at(job, 0));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Argument Edge Cases
 */
TEST_CASE(edge_many_cmd_args) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name manyargs\n"
        "  cmd /bin/echo a1 a2 a3 a4 a5 a6 a7 a8 a9 a10\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    /* 11 args + NULL = 12 */
    TEST_ASSERT_EQ(12, get_cmd_arg_count(job));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_empty_quoted_arg) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name emptyarg\n"
        "  cmd /bin/echo \"\" after\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("", get_cmd_arg_at(job, 1));
    TEST_ASSERT_STR_EQ("after", get_cmd_arg_at(job, 2));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Environment Variable Edge Cases
 */
TEST_CASE(edge_env_empty_value) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name emptyenv\n"
        "  cmd /bin/true\n"
        "  env EMPTY=\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("EMPTY=", get_env_at(job, 0));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_env_value_with_equals) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name eqenv\n"
        "  cmd /bin/true\n"
        "  env URL=http://example.com?a=b\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("URL=http://example.com?a=b", get_env_at(job, 0));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_many_env_vars) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name manyenv\n"
        "  cmd /bin/true\n"
        "  env VAR1=1\n"
        "  env VAR2=2\n"
        "  env VAR3=3\n"
        "  env VAR4=4\n"
        "  env VAR5=5\n"
        "  env VAR6=6\n"
        "  env VAR7=7\n"
        "  env VAR8=8\n"
        "  env VAR9=9\n"
        "  env VAR10=10\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(10, get_env_count(job));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Nice Edge Cases
 */
TEST_CASE(edge_nice_boundary_low) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name nicelow\n"
        "  cmd /bin/true\n"
        "  nice -20\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(-20, job->nice);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_nice_boundary_high) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name nicehigh\n"
        "  cmd /bin/true\n"
        "  nice 19\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(19, job->nice);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_nice_zero) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name nicezero\n"
        "  cmd /bin/true\n"
        "  nice 0\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(0, job->nice);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Order Edge Cases
 */
TEST_CASE(edge_order_negative) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name negorder\n"
        "  cmd /bin/true\n"
        "  order -100\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(-100, job->order);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_order_large) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name largeorder\n"
        "  cmd /bin/true\n"
        "  order 999999\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(999999, job->order);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Bounce Interval Edge Cases
 */
TEST_CASE(edge_bounce_one_second) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name bounce1s\n"
        "  cmd /bin/true\n"
        "  bounce every 1s\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, job->bounce_interval);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_bounce_large) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name bounce30d\n"
        "  cmd /bin/true\n"
        "  bounce every 30d\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    /* 30 days * 24 hours * 60 minutes * 60 seconds = 2592000 */
    TEST_ASSERT_EQ(2592000, job->bounce_interval);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * CPU Affinity Edge Cases
 */
TEST_CASE(edge_cpu_single_high) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name cpuhigh\n"
        "  cmd /bin/true\n"
        "  cpu 63\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT(CPU_ISSET(63, &job->cpuset));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_cpu_hex_large) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name cpuhex\n"
        "  cmd /bin/true\n"
        "  cpu 0xff\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    /* 0xff = CPUs 0-7 */
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT(CPU_ISSET(i, &job->cpuset));
    }

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Ulimit Edge Cases
 */
TEST_CASE(edge_ulimit_zero) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name ulimitzero\n"
        "  cmd /bin/true\n"
        "  ulimit -c 0\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    resource_rlimit_t *rt = (resource_rlimit_t*)utarray_front(&job->rlim);
    TEST_ASSERT_EQ(0, rt->rlim.rlim_cur);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_ulimit_all_resources) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name ulimitall\n"
        "  cmd /bin/true\n"
        "  ulimit -c 0\n"
        "  ulimit -d 1000000\n"
        "  ulimit -f 1000000\n"
        "  ulimit -l 64\n"
        "  ulimit -m 1000000\n"
        "  ulimit -n 1024\n"
        "  ulimit -s 8192\n"
        "  ulimit -t 3600\n"
        "  ulimit -u 100\n"
        "  ulimit -v 1000000\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(10, utarray_len(&job->rlim));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Whitespace and Formatting Edge Cases
 */
TEST_CASE(edge_tabs_instead_of_spaces) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "\tname\ttabbed\n"
        "\tcmd\t/bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("tabbed", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_no_newline_at_end) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name nonewline\n"
        "  cmd /bin/true\n"
        "}"  /* No trailing newline */
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_windows_line_endings) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* CRLF line endings */
    cfg.file = strdup(create_temp_config(
        "job {\r\n"
        "  name windows\r\n"
        "  cmd /bin/true\r\n"
        "}\r\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_compact_formatting) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Minimal whitespace */
    cfg.file = strdup(create_temp_config(
        "job {\nname compact\ncmd /bin/true\n}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("compact", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Multiple Jobs Edge Cases
 */
TEST_CASE(edge_many_jobs) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Generate config with 50 jobs */
    char config[8192];
    int offset = 0;
    for (int i = 0; i < 50; i++) {
        offset += snprintf(config + offset, sizeof(config) - offset,
            "job {\n  name job%d\n  cmd /bin/true\n}\n", i);
    }

    cfg.file = strdup(create_temp_config(config));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(50, job_count(&cfg));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(edge_jobs_different_orders) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name z_last\n"
        "  cmd /bin/true\n"
        "  order 100\n"
        "}\n"
        "job {\n"
        "  name a_first\n"
        "  cmd /bin/true\n"
        "  order -50\n"
        "}\n"
        "job {\n"
        "  name m_middle\n"
        "  cmd /bin/true\n"
        "  order 0\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    /* Jobs should be sorted by order */
    job_t *first = get_job_at(&cfg, 0);
    job_t *middle = get_job_at(&cfg, 1);
    job_t *last = get_job_at(&cfg, 2);

    TEST_ASSERT_STR_EQ("a_first", first->name);
    TEST_ASSERT_STR_EQ("m_middle", middle->name);
    TEST_ASSERT_STR_EQ("z_last", last->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Combined Options Edge Cases
 */
TEST_CASE(edge_all_flags) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name allflags\n"
        "  cmd /bin/true\n"
        "  disable\n"
        "  wait\n"
        "  once\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, job->disabled);
    TEST_ASSERT_EQ(1, job->wait);
    TEST_ASSERT_EQ(1, job->once);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Special Value Tests
 */
TEST_CASE(edge_out_syslog) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name syslogout\n"
        "  cmd /bin/true\n"
        "  out syslog\n"
        "  err syslog\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("syslog", job->out);
    TEST_ASSERT_STR_EQ("syslog", job->err);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Global Options Mixed with Jobs
 */
TEST_CASE(edge_global_options_with_jobs) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;

    cfg.file = strdup(create_temp_config(
        "listen on udp://0.0.0.0:9999\n"
        "job {\n"
        "  name job1\n"
        "  cmd /bin/true\n"
        "}\n"
        "report to udp://127.0.0.1:8888\n"
        "job {\n"
        "  name job2\n"
        "  cmd /bin/false\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(2, job_count(&cfg));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("Name Edge Cases");
    RUN_TEST(edge_name_single_char);
    RUN_TEST(edge_name_with_dots);
    RUN_TEST(edge_name_numbers_only);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Path Edge Cases");
    RUN_TEST(edge_cmd_long_path);
    RUN_TEST(edge_cmd_special_chars_in_path);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Argument Edge Cases");
    RUN_TEST(edge_many_cmd_args);
    RUN_TEST(edge_empty_quoted_arg);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Environment Variable Edge Cases");
    RUN_TEST(edge_env_empty_value);
    RUN_TEST(edge_env_value_with_equals);
    RUN_TEST(edge_many_env_vars);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Nice Edge Cases");
    RUN_TEST(edge_nice_boundary_low);
    RUN_TEST(edge_nice_boundary_high);
    RUN_TEST(edge_nice_zero);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Order Edge Cases");
    RUN_TEST(edge_order_negative);
    RUN_TEST(edge_order_large);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Bounce Interval Edge Cases");
    RUN_TEST(edge_bounce_one_second);
    RUN_TEST(edge_bounce_large);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("CPU Affinity Edge Cases");
    RUN_TEST(edge_cpu_single_high);
    RUN_TEST(edge_cpu_hex_large);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Ulimit Edge Cases");
    RUN_TEST(edge_ulimit_zero);
    RUN_TEST(edge_ulimit_all_resources);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Whitespace and Formatting Edge Cases");
    RUN_TEST(edge_tabs_instead_of_spaces);
    RUN_TEST(edge_no_newline_at_end);
    RUN_TEST(edge_windows_line_endings);
    RUN_TEST(edge_compact_formatting);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Multiple Jobs Edge Cases");
    RUN_TEST(edge_many_jobs);
    RUN_TEST(edge_jobs_different_orders);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Combined Options Edge Cases");
    RUN_TEST(edge_all_flags);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Special Values");
    RUN_TEST(edge_out_syslog);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Global Options with Jobs");
    RUN_TEST(edge_global_options_with_jobs);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
