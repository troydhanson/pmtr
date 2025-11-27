/*
 * Integration Tests for pmtr Config File Parsing
 * Tests the complete parsing pipeline from config file to job structures
 */

#define _GNU_SOURCE  /* Required for CPU_SET macros */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "test_framework.h"
#include "test_helpers.h"

/*
 * Basic Config Parsing Tests
 */
TEST_CASE(parse_empty_config) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(""));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(0, job_count(&cfg));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_minimal_job) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(1, job_count(&cfg));

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_NOT_NULL(job);
    TEST_ASSERT_STR_EQ("test", job->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_job_with_cmd_args) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name echo_test\n"
        "  cmd /bin/echo hello world\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_NOT_NULL(job);

    /* cmdv should have: /bin/echo, hello, world, NULL */
    TEST_ASSERT_EQ(4, get_cmd_arg_count(job));
    TEST_ASSERT_STR_EQ("/bin/echo", get_cmd_arg_at(job, 0));
    TEST_ASSERT_STR_EQ("hello", get_cmd_arg_at(job, 1));
    TEST_ASSERT_STR_EQ("world", get_cmd_arg_at(job, 2));
    TEST_ASSERT_NULL(get_cmd_arg_at(job, 3));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_job_with_quoted_args) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name echo_test\n"
        "  cmd /bin/echo \"hello world\" another\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_NOT_NULL(job);

    /* Quoted string should be preserved as single argument */
    TEST_ASSERT_STR_EQ("/bin/echo", get_cmd_arg_at(job, 0));
    TEST_ASSERT_STR_EQ("hello world", get_cmd_arg_at(job, 1));
    TEST_ASSERT_STR_EQ("another", get_cmd_arg_at(job, 2));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_full_job) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name full_test\n"
        "  cmd /usr/bin/daemon -f\n"
        "  dir /var/run\n"
        "  out /var/log/daemon.out\n"
        "  err /var/log/daemon.err\n"
        "  in /dev/null\n"
        "  user nobody\n"
        "  nice 5\n"
        "  order 10\n"
        "  env DEBUG=1\n"
        "  env LOG_LEVEL=info\n"
        "  cpu 0-3\n"
        "  ulimit -n 1024\n"
        "  ulimit -c 0\n"
        "  bounce every 1h\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(1, job_count(&cfg));

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_NOT_NULL(job);
    TEST_ASSERT_STR_EQ("full_test", job->name);
    TEST_ASSERT_STR_EQ("/var/run", job->dir);
    TEST_ASSERT_STR_EQ("/var/log/daemon.out", job->out);
    TEST_ASSERT_STR_EQ("/var/log/daemon.err", job->err);
    TEST_ASSERT_STR_EQ("/dev/null", job->in);
    TEST_ASSERT_STR_EQ("nobody", job->user);
    TEST_ASSERT_EQ(5, job->nice);
    TEST_ASSERT_EQ(10, job->order);
    TEST_ASSERT_EQ(2, get_env_count(job));
    TEST_ASSERT_EQ(3600, job->bounce_interval);
    TEST_ASSERT_EQ(2, utarray_len(&job->rlim));

    /* Check CPU affinity */
    TEST_ASSERT(CPU_ISSET(0, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(1, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(3, &job->cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(4, &job->cpuset));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_multiple_jobs) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name job1\n"
        "  cmd /bin/true\n"
        "}\n"
        "job {\n"
        "  name job2\n"
        "  cmd /bin/false\n"
        "}\n"
        "job {\n"
        "  name job3\n"
        "  cmd /bin/sleep 10\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(3, job_count(&cfg));

    TEST_ASSERT_NOT_NULL(get_job_by_name(cfg.jobs, "job1"));
    TEST_ASSERT_NOT_NULL(get_job_by_name(cfg.jobs, "job2"));
    TEST_ASSERT_NOT_NULL(get_job_by_name(cfg.jobs, "job3"));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_jobs_ordered) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Jobs in reverse order in config */
    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name third\n"
        "  cmd /bin/true\n"
        "  order 30\n"
        "}\n"
        "job {\n"
        "  name first\n"
        "  cmd /bin/true\n"
        "  order 10\n"
        "}\n"
        "job {\n"
        "  name second\n"
        "  cmd /bin/true\n"
        "  order 20\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    /* Jobs should be sorted by order */
    job_t *job0 = get_job_at(&cfg, 0);
    job_t *job1 = get_job_at(&cfg, 1);
    job_t *job2 = get_job_at(&cfg, 2);

    TEST_ASSERT_STR_EQ("first", job0->name);
    TEST_ASSERT_STR_EQ("second", job1->name);
    TEST_ASSERT_STR_EQ("third", job2->name);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_disabled_job) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name disabled_job\n"
        "  cmd /bin/true\n"
        "  disable\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, job->disabled);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_wait_job) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name setup\n"
        "  cmd /bin/mkdir /tmp/test\n"
        "  wait\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, job->wait);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_once_job) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name onetime\n"
        "  cmd /bin/true\n"
        "  once\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, job->once);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_depends_block) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Create the dependency files first */
    create_temp_file("config1.conf", "content1");
    create_temp_file("config2.conf", "content2");

    char config[1024];
    snprintf(config, sizeof(config),
        "job {\n"
        "  name watcher\n"
        "  cmd /bin/true\n"
        "  depends {\n"
        "    %s/config1.conf\n"
        "    %s/config2.conf\n"
        "  }\n"
        "}\n", g_test_tmpdir, g_test_tmpdir);

    cfg.file = strdup(create_temp_config(config));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(2, get_dep_count(job));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_ulimit_block) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name limited\n"
        "  cmd /bin/true\n"
        "  ulimit {\n"
        "    -n 1024\n"
        "    -c 0\n"
        "  }\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(2, utarray_len(&job->rlim));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Comment Handling Tests
 */
TEST_CASE(parse_comments) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "# This is a comment\n"
        "job {\n"
        "  # Comment inside job\n"
        "  name test\n"
        "  cmd /bin/true # inline comment doesn't work like this\n"
        "}\n"
        "# Final comment\n"
    ));

    int rc = parse_jobs(&cfg, em);
    /* Note: inline comments after tokens may not work, but leading comments should */
    /* The test checks that comments don't break parsing */
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(1, job_count(&cfg));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_blank_lines) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "\n"
        "\n"
        "job {\n"
        "\n"
        "  name test\n"
        "\n"
        "  cmd /bin/true\n"
        "\n"
        "}\n"
        "\n"
        "\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(1, job_count(&cfg));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Global Options Tests
 */
TEST_CASE(parse_listen_option) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;  /* Don't actually create sockets */

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

TEST_CASE(parse_report_option) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);
    cfg.test_only = 1;  /* Don't actually create sockets */

    cfg.file = strdup(create_temp_config(
        "report to udp://127.0.0.1:8888\n"
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
 * Error Handling Tests
 */
TEST_CASE(parse_missing_name) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_syntax_error_missing_brace) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        /* Missing closing brace */
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_invalid_nice) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "  nice 100\n"  /* Invalid: out of range */
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_invalid_env) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "  env INVALID\n"  /* Missing = */
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_invalid_bounce) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "  bounce every 10x\n"  /* Invalid unit */
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_invalid_cpu) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  cmd /bin/true\n"
        "  cpu 5-3\n"  /* Invalid range */
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_duplicate_name_option) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name test\n"
        "  name again\n"  /* Duplicate name option */
        "  cmd /bin/true\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_duplicate_job_names) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Two jobs with the same name */
    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name duplicate\n"
        "  cmd /bin/true\n"
        "}\n"
        "job {\n"
        "  name duplicate\n"
        "  cmd /bin/false\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    /* pmtr currently allows duplicate job names - both jobs are added */
    TEST_ASSERT_EQ(0, rc);
    TEST_ASSERT_EQ(2, (int)utarray_len(cfg.jobs));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_missing_cmd) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name nocmd\n"
        "  /* no cmd specified */\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(-1, rc);

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_depends_relative_path) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Create a dependency file */
    create_temp_file("config.json", "{\"key\": \"value\"}");

    char config[1024];
    snprintf(config, sizeof(config),
        "job {\n"
        "  name watcher\n"
        "  cmd /bin/true\n"
        "  dir %s\n"
        "  depends {\n"
        "    config.json\n"  /* Relative path - resolved with dir */
        "  }\n"
        "}\n", g_test_tmpdir);

    cfg.file = strdup(create_temp_config(config));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(1, get_dep_count(job));
    TEST_ASSERT_STR_EQ("config.json", get_dep_at(job, 0));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_depends_multiple_files) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Create multiple dependency files */
    create_temp_file("file1.conf", "config1");
    create_temp_file("file2.conf", "config2");
    create_temp_file("file3.conf", "config3");

    char config[2048];
    snprintf(config, sizeof(config),
        "job {\n"
        "  name multideps\n"
        "  cmd /bin/true\n"
        "  depends {\n"
        "    %s/file1.conf\n"
        "    %s/file2.conf\n"
        "    %s/file3.conf\n"
        "  }\n"
        "}\n", g_test_tmpdir, g_test_tmpdir, g_test_tmpdir);

    cfg.file = strdup(create_temp_config(config));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(3, get_dep_count(job));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_cmd_quoted_with_special_chars) {
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
        "  cmd /bin/sh -c \"echo hello; echo world\"\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_STR_EQ("/bin/sh", get_cmd_arg_at(job, 0));
    TEST_ASSERT_STR_EQ("-c", get_cmd_arg_at(job, 1));
    TEST_ASSERT_STR_EQ("echo hello; echo world", get_cmd_arg_at(job, 2));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_env_special_values) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name envtest\n"
        "  cmd /bin/true\n"
        "  env PATH=/usr/bin:/bin\n"
        "  env URL=http://example.com?foo=bar&baz=1\n"
        "  env EMPTY=\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(3, get_env_count(job));
    TEST_ASSERT_STR_EQ("PATH=/usr/bin:/bin", get_env_at(job, 0));
    TEST_ASSERT_STR_EQ("URL=http://example.com?foo=bar&baz=1", get_env_at(job, 1));
    TEST_ASSERT_STR_EQ("EMPTY=", get_env_at(job, 2));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_ulimit_inline_vs_block) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Test that both inline and block ulimit syntax work */
    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name ulimittest\n"
        "  cmd /bin/true\n"
        "  ulimit -n 1024\n"  /* Inline syntax */
        "  ulimit {\n"        /* Block syntax */
        "    -c 0\n"
        "    -m 1000000\n"
        "  }\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(3, utarray_len(&job->rlim));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_cpu_various_formats) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Test hex format */
    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name hexcpu\n"
        "  cmd /bin/true\n"
        "  cpu 0xF\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT(CPU_ISSET(0, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(1, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(2, &job->cpuset));
    TEST_ASSERT(CPU_ISSET(3, &job->cpuset));

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

TEST_CASE(parse_bounce_units) {
    pmtr_t cfg;
    UT_string *em;

    if (test_init() != 0) {
        TEST_ASSERT_MSG(0, "Failed to init test environment");
    }

    init_test_cfg(&cfg);
    utstring_new(em);

    /* Test different time units */
    cfg.file = strdup(create_temp_config(
        "job {\n"
        "  name bounce_m\n"
        "  cmd /bin/true\n"
        "  bounce every 5m\n"
        "}\n"
    ));

    int rc = parse_jobs(&cfg, em);
    TEST_ASSERT_EQ(0, rc);

    job_t *job = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(300, job->bounce_interval);  /* 5 minutes = 300 seconds */

    utstring_free(em);
    free_test_cfg(&cfg);
    test_cleanup();
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("Basic Config Parsing");
    RUN_TEST(parse_empty_config);
    RUN_TEST(parse_minimal_job);
    RUN_TEST(parse_job_with_cmd_args);
    RUN_TEST(parse_job_with_quoted_args);
    RUN_TEST(parse_full_job);
    RUN_TEST(parse_multiple_jobs);
    RUN_TEST(parse_jobs_ordered);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Job Flags");
    RUN_TEST(parse_disabled_job);
    RUN_TEST(parse_wait_job);
    RUN_TEST(parse_once_job);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Block Options");
    RUN_TEST(parse_depends_block);
    RUN_TEST(parse_ulimit_block);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Comments and Whitespace");
    RUN_TEST(parse_comments);
    RUN_TEST(parse_blank_lines);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Global Options");
    RUN_TEST(parse_listen_option);
    RUN_TEST(parse_report_option);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Error Handling");
    RUN_TEST(parse_missing_name);
    RUN_TEST(parse_syntax_error_missing_brace);
    RUN_TEST(parse_invalid_nice);
    RUN_TEST(parse_invalid_env);
    RUN_TEST(parse_invalid_bounce);
    RUN_TEST(parse_invalid_cpu);
    RUN_TEST(parse_duplicate_name_option);
    RUN_TEST(parse_duplicate_job_names);
    RUN_TEST(parse_missing_cmd);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Depends Block");
    RUN_TEST(parse_depends_relative_path);
    RUN_TEST(parse_depends_multiple_files);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Command and Environment");
    RUN_TEST(parse_cmd_quoted_with_special_chars);
    RUN_TEST(parse_env_special_values);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Ulimit and CPU");
    RUN_TEST(parse_ulimit_inline_vs_block);
    RUN_TEST(parse_cpu_various_formats);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Bounce");
    RUN_TEST(parse_bounce_units);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
