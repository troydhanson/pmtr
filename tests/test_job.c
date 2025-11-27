/*
 * Unit Tests for pmtr Job Management (job.c)
 * Tests job lifecycle, comparison, and lookup functions
 */

#define _GNU_SOURCE  /* Required for CPU_SET macros */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "test_framework.h"
#include "test_helpers.h"

/*
 * job_ini Tests
 */
TEST_CASE(job_ini_zeroed) {
    job_t job;
    job_ini(&job);

    TEST_ASSERT_NULL(job.name);
    TEST_ASSERT_NULL(job.dir);
    TEST_ASSERT_NULL(job.out);
    TEST_ASSERT_NULL(job.err);
    TEST_ASSERT_NULL(job.in);
    TEST_ASSERT_EQ(0, job.pid);
    TEST_ASSERT_EQ(0, job.order);
    TEST_ASSERT_EQ(0, job.nice);
    TEST_ASSERT_EQ(0, job.disabled);
    TEST_ASSERT_EQ(0, job.wait);
    TEST_ASSERT_EQ(0, job.once);
    TEST_ASSERT_EQ(0, job.bounce_interval);

    job_fin(&job);
}

TEST_CASE(job_ini_respawn_default) {
    job_t job;
    job_ini(&job);

    /* respawn should default to 1 (true) */
    TEST_ASSERT_EQ(1, job.respawn);

    job_fin(&job);
}

TEST_CASE(job_ini_arrays_initialized) {
    job_t job;
    job_ini(&job);

    TEST_ASSERT_EQ(0, utarray_len(&job.cmdv));
    TEST_ASSERT_EQ(0, utarray_len(&job.envv));
    TEST_ASSERT_EQ(0, utarray_len(&job.depv));
    TEST_ASSERT_EQ(0, utarray_len(&job.rlim));

    job_fin(&job);
}

TEST_CASE(job_ini_cpuset_empty) {
    job_t job;
    job_ini(&job);

    TEST_ASSERT_EQ(0, CPU_COUNT(&job.cpuset));

    job_fin(&job);
}

/*
 * job_fin Tests
 */
TEST_CASE(job_fin_frees_strings) {
    job_t job;
    job_ini(&job);

    job.name = strdup("test");
    job.dir = strdup("/tmp");
    job.out = strdup("/var/log/out");
    job.err = strdup("/var/log/err");
    job.in = strdup("/dev/null");

    /* Should not crash when freeing */
    job_fin(&job);

    /* After fin, pointers should still be set (not cleared by fin)
     * but the memory is freed. We just verify it doesn't crash. */
}

TEST_CASE(job_fin_handles_null) {
    job_t job;
    job_ini(&job);

    /* All string fields are NULL, should not crash */
    job_fin(&job);
}

/*
 * job_cpy Tests
 */
TEST_CASE(job_cpy_basic_fields) {
    job_t src, dst;
    job_ini(&src);

    src.name = strdup("source_job");
    src.dir = strdup("/home/user");
    src.out = strdup("/var/log/out.log");
    src.err = strdup("/var/log/err.log");
    src.in = strdup("/dev/null");
    src.order = 5;
    src.nice = 10;
    src.disabled = 1;
    src.wait = 1;
    src.once = 1;
    src.bounce_interval = 3600;

    job_cpy(&dst, &src);

    TEST_ASSERT_NOT_NULL(dst.name);
    TEST_ASSERT_STR_EQ("source_job", dst.name);
    TEST_ASSERT_TRUE(dst.name != src.name);  /* Different pointers (deep copy) */

    TEST_ASSERT_STR_EQ("/home/user", dst.dir);
    TEST_ASSERT_STR_EQ("/var/log/out.log", dst.out);
    TEST_ASSERT_STR_EQ("/var/log/err.log", dst.err);
    TEST_ASSERT_STR_EQ("/dev/null", dst.in);

    TEST_ASSERT_EQ(5, dst.order);
    TEST_ASSERT_EQ(10, dst.nice);
    TEST_ASSERT_EQ(1, dst.disabled);
    TEST_ASSERT_EQ(1, dst.wait);
    TEST_ASSERT_EQ(1, dst.once);
    TEST_ASSERT_EQ(3600, dst.bounce_interval);

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_user) {
    job_t src, dst;
    job_ini(&src);

    strcpy(src.user, "testuser");

    job_cpy(&dst, &src);

    TEST_ASSERT_STR_EQ("testuser", dst.user);

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_cmdv) {
    job_t src, dst;
    job_ini(&src);

    char *cmd1 = "/bin/echo";
    char *cmd2 = "hello";
    char *cmd3 = "world";
    utarray_push_back(&src.cmdv, &cmd1);
    utarray_push_back(&src.cmdv, &cmd2);
    utarray_push_back(&src.cmdv, &cmd3);

    job_cpy(&dst, &src);

    TEST_ASSERT_EQ(3, utarray_len(&dst.cmdv));

    char **arg0 = (char**)utarray_eltptr(&dst.cmdv, 0);
    char **arg1 = (char**)utarray_eltptr(&dst.cmdv, 1);
    char **arg2 = (char**)utarray_eltptr(&dst.cmdv, 2);

    TEST_ASSERT_STR_EQ("/bin/echo", *arg0);
    TEST_ASSERT_STR_EQ("hello", *arg1);
    TEST_ASSERT_STR_EQ("world", *arg2);

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_envv) {
    job_t src, dst;
    job_ini(&src);

    char *env1 = "VAR1=value1";
    char *env2 = "VAR2=value2";
    utarray_push_back(&src.envv, &env1);
    utarray_push_back(&src.envv, &env2);

    job_cpy(&dst, &src);

    TEST_ASSERT_EQ(2, utarray_len(&dst.envv));

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_depv) {
    job_t src, dst;
    job_ini(&src);

    char *dep1 = "/etc/config1";
    char *dep2 = "/etc/config2";
    utarray_push_back(&src.depv, &dep1);
    utarray_push_back(&src.depv, &dep2);
    src.deps_hash = 12345;

    job_cpy(&dst, &src);

    TEST_ASSERT_EQ(2, utarray_len(&dst.depv));
    TEST_ASSERT_EQ(12345, dst.deps_hash);

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_cpuset) {
    job_t src, dst;
    job_ini(&src);

    CPU_SET(0, &src.cpuset);
    CPU_SET(2, &src.cpuset);
    CPU_SET(4, &src.cpuset);

    job_cpy(&dst, &src);

    TEST_ASSERT(CPU_ISSET(0, &dst.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(1, &dst.cpuset));
    TEST_ASSERT(CPU_ISSET(2, &dst.cpuset));
    TEST_ASSERT_FALSE(CPU_ISSET(3, &dst.cpuset));
    TEST_ASSERT(CPU_ISSET(4, &dst.cpuset));

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_runtime_state) {
    job_t src, dst;
    job_ini(&src);

    src.name = strdup("test");
    src.pid = 12345;
    src.start_ts = 1000000;
    src.start_at = 1000010;
    src.terminate = 1;
    src.delete_when_collected = 1;
    src.respawn = 0;

    job_cpy(&dst, &src);

    TEST_ASSERT_EQ(12345, dst.pid);
    TEST_ASSERT_EQ(1000000, dst.start_ts);
    TEST_ASSERT_EQ(1000010, dst.start_at);
    TEST_ASSERT_EQ(1, dst.terminate);
    TEST_ASSERT_EQ(1, dst.delete_when_collected);
    TEST_ASSERT_EQ(0, dst.respawn);

    job_fin(&src);
    job_fin(&dst);
}

TEST_CASE(job_cpy_null_strings) {
    job_t src, dst;
    job_ini(&src);

    src.name = strdup("test");
    /* dir, out, err, in are all NULL */

    job_cpy(&dst, &src);

    TEST_ASSERT_NULL(dst.dir);
    TEST_ASSERT_NULL(dst.out);
    TEST_ASSERT_NULL(dst.err);
    TEST_ASSERT_NULL(dst.in);

    job_fin(&src);
    job_fin(&dst);
}

/*
 * job_cmp Tests
 */
TEST_CASE(job_cmp_identical) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    char *cmd = "/bin/test";
    utarray_push_back(&a.cmdv, &cmd);
    utarray_push_back(&b.cmdv, &cmd);

    int result = job_cmp(&a, &b);
    TEST_ASSERT_EQ(0, result);  /* Should be equal */

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_name) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("alpha");
    b.name = strdup("beta");

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_cmd) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    char *cmd1 = "/bin/cmd1";
    char *cmd2 = "/bin/cmd2";
    utarray_push_back(&a.cmdv, &cmd1);
    utarray_push_back(&b.cmdv, &cmd2);

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_cmd_args) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    char *cmd = "/bin/cmd";
    char *arg1 = "arg1";
    char *arg2 = "arg2";

    utarray_push_back(&a.cmdv, &cmd);
    utarray_push_back(&a.cmdv, &arg1);

    utarray_push_back(&b.cmdv, &cmd);
    utarray_push_back(&b.cmdv, &arg2);

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_env_count) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    char *env1 = "VAR1=1";
    char *env2 = "VAR2=2";

    utarray_push_back(&a.envv, &env1);
    utarray_push_back(&b.envv, &env1);
    utarray_push_back(&b.envv, &env2);

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_dir) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.dir = strdup("/dir1");
    b.dir = strdup("/dir2");

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_one_dir_null) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.dir = strdup("/dir");
    /* b.dir is NULL */

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_user) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    strcpy(a.user, "user1");
    strcpy(b.user, "user2");

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_order) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.order = 1;
    b.order = 2;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_disabled) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.disabled = 0;
    b.disabled = 1;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_wait) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.wait = 0;
    b.wait = 1;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_once) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.once = 0;
    b.once = 1;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_bounce) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.bounce_interval = 3600;
    b.bounce_interval = 7200;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_cpuset) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    CPU_SET(0, &a.cpuset);
    CPU_SET(1, &b.cpuset);

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

TEST_CASE(job_cmp_different_deps_hash) {
    job_t a, b;
    job_ini(&a);
    job_ini(&b);

    a.name = strdup("test");
    b.name = strdup("test");

    a.deps_hash = 111;
    b.deps_hash = 222;

    int result = job_cmp(&a, &b);
    TEST_ASSERT(result != 0);

    job_fin(&a);
    job_fin(&b);
}

/*
 * get_job_by_pid Tests
 */
TEST_CASE(get_job_by_pid_found) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    /* Create and add a job manually */
    job_t job;
    job_ini(&job);
    job.name = strdup("testjob");
    job.pid = 12345;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);  /* The array made a copy */

    job_t *found = get_job_by_pid(cfg.jobs, 12345);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("testjob", found->name);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_pid_not_found) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t job;
    job_ini(&job);
    job.name = strdup("testjob");
    job.pid = 12345;
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *found = get_job_by_pid(cfg.jobs, 99999);
    TEST_ASSERT_NULL(found);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_pid_empty_list) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t *found = get_job_by_pid(cfg.jobs, 12345);
    TEST_ASSERT_NULL(found);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_pid_multiple_jobs) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t job1, job2, job3;

    job_ini(&job1);
    job1.name = strdup("job1");
    job1.pid = 100;
    utarray_push_back(cfg.jobs, &job1);
    job_fin(&job1);

    job_ini(&job2);
    job2.name = strdup("job2");
    job2.pid = 200;
    utarray_push_back(cfg.jobs, &job2);
    job_fin(&job2);

    job_ini(&job3);
    job3.name = strdup("job3");
    job3.pid = 300;
    utarray_push_back(cfg.jobs, &job3);
    job_fin(&job3);

    job_t *found = get_job_by_pid(cfg.jobs, 200);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("job2", found->name);

    free_test_cfg(&cfg);
}

/*
 * get_job_by_name Tests
 */
TEST_CASE(get_job_by_name_found) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *found = get_job_by_name(cfg.jobs, "myjob");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_STR_EQ("myjob", found->name);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_name_not_found) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t job;
    job_ini(&job);
    job.name = strdup("myjob");
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    job_t *found = get_job_by_name(cfg.jobs, "otherjob");
    TEST_ASSERT_NULL(found);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_name_empty_list) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t *found = get_job_by_name(cfg.jobs, "anyjob");
    TEST_ASSERT_NULL(found);

    free_test_cfg(&cfg);
}

TEST_CASE(get_job_by_name_case_sensitive) {
    pmtr_t cfg;
    init_test_cfg(&cfg);

    job_t job;
    job_ini(&job);
    job.name = strdup("MyJob");
    utarray_push_back(cfg.jobs, &job);
    job_fin(&job);

    /* Exact match required */
    job_t *found = get_job_by_name(cfg.jobs, "myjob");
    TEST_ASSERT_NULL(found);

    found = get_job_by_name(cfg.jobs, "MyJob");
    TEST_ASSERT_NOT_NULL(found);

    free_test_cfg(&cfg);
}

/*
 * push_job Tests (via integration with parse_t)
 */
TEST_CASE(push_job_basic) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    job.name = strdup("testjob");
    char *cmd = "/bin/test";
    utarray_push_back(&job.cmdv, &cmd);

    push_job(&ps);

    TEST_ASSERT_EQ(0, ps.rc);
    TEST_ASSERT_EQ(1, utarray_len(cfg.jobs));

    job_t *added = get_job_at(&cfg, 0);
    TEST_ASSERT_NOT_NULL(added);
    TEST_ASSERT_STR_EQ("testjob", added->name);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(push_job_no_name) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    /* No name set */
    push_job(&ps);

    TEST_ASSERT_EQ(-1, ps.rc);
    TEST_ASSERT_EQ(0, utarray_len(cfg.jobs));

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

TEST_CASE(push_job_adds_null_terminator) {
    pmtr_t cfg;
    job_t job;
    UT_string *em;
    parse_t ps;

    init_test_cfg(&cfg);
    job_ini(&job);
    utstring_new(em);
    init_test_parse(&ps, &cfg, &job, em);

    job.name = strdup("testjob");
    char *cmd = "/bin/test";
    char *arg = "arg1";
    utarray_push_back(&job.cmdv, &cmd);
    utarray_push_back(&job.cmdv, &arg);

    /* Before push, cmdv has 2 elements */
    TEST_ASSERT_EQ(2, utarray_len(&job.cmdv));

    push_job(&ps);

    /* After push, the job in the array should have NULL terminator */
    job_t *added = get_job_at(&cfg, 0);
    TEST_ASSERT_EQ(3, utarray_len(&added->cmdv));  /* cmd, arg, NULL */

    char **last = (char**)utarray_eltptr(&added->cmdv, 2);
    TEST_ASSERT_NULL(*last);

    job_fin(&job);
    utstring_free(em);
    free_test_cfg(&cfg);
}

/*
 * Test Runner
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    TEST_SUITE_BEGIN("job_ini");
    RUN_TEST(job_ini_zeroed);
    RUN_TEST(job_ini_respawn_default);
    RUN_TEST(job_ini_arrays_initialized);
    RUN_TEST(job_ini_cpuset_empty);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("job_fin");
    RUN_TEST(job_fin_frees_strings);
    RUN_TEST(job_fin_handles_null);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("job_cpy");
    RUN_TEST(job_cpy_basic_fields);
    RUN_TEST(job_cpy_user);
    RUN_TEST(job_cpy_cmdv);
    RUN_TEST(job_cpy_envv);
    RUN_TEST(job_cpy_depv);
    RUN_TEST(job_cpy_cpuset);
    RUN_TEST(job_cpy_runtime_state);
    RUN_TEST(job_cpy_null_strings);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("job_cmp");
    RUN_TEST(job_cmp_identical);
    RUN_TEST(job_cmp_different_name);
    RUN_TEST(job_cmp_different_cmd);
    RUN_TEST(job_cmp_different_cmd_args);
    RUN_TEST(job_cmp_different_env_count);
    RUN_TEST(job_cmp_different_dir);
    RUN_TEST(job_cmp_one_dir_null);
    RUN_TEST(job_cmp_different_user);
    RUN_TEST(job_cmp_different_order);
    RUN_TEST(job_cmp_different_disabled);
    RUN_TEST(job_cmp_different_wait);
    RUN_TEST(job_cmp_different_once);
    RUN_TEST(job_cmp_different_bounce);
    RUN_TEST(job_cmp_different_cpuset);
    RUN_TEST(job_cmp_different_deps_hash);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("get_job_by_pid");
    RUN_TEST(get_job_by_pid_found);
    RUN_TEST(get_job_by_pid_not_found);
    RUN_TEST(get_job_by_pid_empty_list);
    RUN_TEST(get_job_by_pid_multiple_jobs);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("get_job_by_name");
    RUN_TEST(get_job_by_name_found);
    RUN_TEST(get_job_by_name_not_found);
    RUN_TEST(get_job_by_name_empty_list);
    RUN_TEST(get_job_by_name_case_sensitive);
    TEST_SUITE_END();

    TEST_SUITE_BEGIN("push_job");
    RUN_TEST(push_job_basic);
    RUN_TEST(push_job_no_name);
    RUN_TEST(push_job_adds_null_terminator);
    TEST_SUITE_END();

    print_test_results();
    return get_test_exit_code();
}
