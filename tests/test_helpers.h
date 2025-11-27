/*
 * Test Helpers for pmtr
 * Common utilities and setup/teardown functions for testing
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

/* Required for CPU_SET macros and other GNU extensions */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>

/* Include pmtr headers */
#include "../src/pmtr.h"
#include "../src/job.h"
#include "../src/net.h"
#include "../src/cfg.h"

/* External declaration for job_ini (defined in job.c but not in job.h) */
void job_ini(job_t *job);

/* Temporary directory for test files */
static char g_test_tmpdir[256] = "";

/* Initialize test environment */
static inline int test_init(void) {
    /* Create temp directory for test files */
    snprintf(g_test_tmpdir, sizeof(g_test_tmpdir), "/tmp/pmtr_test_%d", getpid());
    if (mkdir(g_test_tmpdir, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create test directory");
        return -1;
    }
    return 0;
}

/* Cleanup test environment */
static inline void test_cleanup(void) {
    char cmd[512];
    if (g_test_tmpdir[0]) {
        snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_tmpdir);
        (void)system(cmd);
    }
}

/* Create a temporary config file and return its path */
static inline char *create_temp_config(const char *content) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/pmtr.conf", g_test_tmpdir);

    FILE *f = fopen(path, "w");
    if (!f) return NULL;

    if (content) {
        fputs(content, f);
    }
    fclose(f);
    return path;
}

/* Create a temp file with specified name and content */
static inline char *create_temp_file(const char *name, const char *content) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_test_tmpdir, name);

    FILE *f = fopen(path, "w");
    if (!f) return NULL;

    if (content) {
        fputs(content, f);
    }
    fclose(f);
    return path;
}

/* Initialize a pmtr_t config structure for testing */
static inline void init_test_cfg(pmtr_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    utarray_new(cfg->jobs, &job_mm);
    utarray_new(cfg->listen, &ut_int_icd);
    utarray_new(cfg->report, &ut_int_icd);
    utstring_new(cfg->s);
}

/* Free a pmtr_t config structure */
static inline void free_test_cfg(pmtr_t *cfg) {
    if (cfg->jobs) utarray_free(cfg->jobs);
    if (cfg->listen) utarray_free(cfg->listen);
    if (cfg->report) utarray_free(cfg->report);
    if (cfg->s) utstring_free(cfg->s);
    if (cfg->file) free(cfg->file);
}

/* Initialize a parse_t structure for testing setters */
static inline void init_test_parse(parse_t *ps, pmtr_t *cfg, job_t *job, UT_string *em) {
    ps->line = 1;
    ps->rc = 0;
    ps->em = em;
    ps->job = job;
    ps->cfg = cfg;
}

/* Get job from config by index */
static inline job_t *get_job_at(pmtr_t *cfg, int idx) {
    return (job_t*)utarray_eltptr(cfg->jobs, idx);
}

/* Count jobs in config */
static inline int job_count(pmtr_t *cfg) {
    return utarray_len(cfg->jobs);
}

/* Get first command argument from job */
static inline char *get_job_cmd(job_t *job) {
    char **cmd = (char**)utarray_front(&job->cmdv);
    return cmd ? *cmd : NULL;
}

/* Get environment variable count */
static inline int get_env_count(job_t *job) {
    return utarray_len(&job->envv);
}

/* Get environment variable by index */
static inline char *get_env_at(job_t *job, int idx) {
    char **env = (char**)utarray_eltptr(&job->envv, idx);
    return env ? *env : NULL;
}

/* Get dependency count */
static inline int get_dep_count(job_t *job) {
    return utarray_len(&job->depv);
}

/* Get dependency by index */
static inline char *get_dep_at(job_t *job, int idx) {
    char **dep = (char**)utarray_eltptr(&job->depv, idx);
    return dep ? *dep : NULL;
}

/* Get command argument count (including NULL terminator) */
static inline int get_cmd_arg_count(job_t *job) {
    return utarray_len(&job->cmdv);
}

/* Get command argument by index */
static inline char *get_cmd_arg_at(job_t *job, int idx) {
    char **arg = (char**)utarray_eltptr(&job->cmdv, idx);
    return arg ? *arg : NULL;
}

/* Check if a specific CPU is set in cpuset */
static inline int cpu_is_set(job_t *job, int cpu) {
    return CPU_ISSET(cpu, &job->cpuset);
}

/* Count CPUs in cpuset */
static inline int cpu_count(job_t *job) {
    return CPU_COUNT(&job->cpuset);
}

#endif /* TEST_HELPERS_H */
