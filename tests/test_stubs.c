/*
 * Test Stubs for pmtr
 * Provides stub implementations for functions defined in pmtr.c
 * that are referenced by job.c but not needed for testing
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>

/* Stub for dep_monitor - the real version is in pmtr.c which we don't link
 * This is called from collect_jobs() when dependency monitor exits.
 * For testing purposes, we just return 0 to indicate no monitoring. */
pid_t dep_monitor(char *file) {
    (void)file;
    return 0;
}
