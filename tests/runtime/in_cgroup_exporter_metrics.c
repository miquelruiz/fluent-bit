#include "flb_tests_runtime.h"
#include <fluent-bit.h>

#include <stdio.h>

#define DPATH_MOUNTPOINT FLB_TESTS_DATA_PATH "/data/in_cgroup_exporter_metrics/sys/fs/cgroup"

char* expected_psi_lines[] = {
    "cgroups_psi_total_seconds{controller=\"memory\",cgroup=\"/\",kind=\"some\"} = 999999999.99999905",
    "cgroups_psi_total_seconds{controller=\"memory\",cgroup=\"/\",kind=\"full\"} = 888888888.888888",
    "cgroups_psi_avg10_ratio{controller=\"memory\",cgroup=\"/\",kind=\"some\"} = 10",
    "cgroups_psi_avg10_ratio{controller=\"memory\",cgroup=\"/\",kind=\"full\"} = 11",
    "cgroups_psi_avg60_ratio{controller=\"memory\",cgroup=\"/\",kind=\"some\"} = 60",
    "cgroups_psi_avg60_ratio{controller=\"memory\",cgroup=\"/\",kind=\"full\"} = 61",
    "cgroups_psi_avg300_ratio{controller=\"memory\",cgroup=\"/\",kind=\"some\"} = 300",
    "cgroups_psi_avg300_ratio{controller=\"memory\",cgroup=\"/\",kind=\"full\"} = 301",
};

void check_output(FILE* f)
{
    char* line = NULL;
    size_t len = 0;
    ssize_t n;
    bool found[8] = { false };

    rewind(f);
    while ((n = getline(&line, &len, f)) != -1) {
        for (int i = 0; i < 8; i++) {
            if (strstr(line, expected_psi_lines[i]) != NULL) {
                found[i] = true;
                goto outer;
            }
        }
    outer:
    }

    for (int i = 0; i < 8; i++) {
        TEST_CHECK(found[i] == true);
    }

    free(line);
}

void do_create(flb_ctx_t* ctx, char* system, ...)
{
    int in_ffd;
    int out_ffd;
    va_list va;
    char* key;
    char* value;

    in_ffd = flb_input(ctx, (char*)system, NULL);

    va_start(va, system);
    while ((key = va_arg(va, char*))) {
        value = va_arg(va, char*);
        TEST_CHECK(value != NULL);
        TEST_CHECK(flb_input_set(ctx, in_ffd, key, value, NULL) == 0);
    }
    va_end(va);

    out_ffd = flb_output(ctx, (char*)"stdout", NULL);
    TEST_CHECK(out_ffd >= 0);

    TEST_CHECK(flb_service_set(ctx,
                   "Flush", "0.5",
                   "Grace", "1",
                   "log_level", "debug",
                   NULL)
        == 0);
}

void flb_test_system_psi()
{
    flb_ctx_t* ctx = flb_create();

    int tmpfd, orig_stdout, stdout_fd;
    FILE* tmp;

    // Create a temp file and get its file descriptor
    tmp = tmpfile();
    tmpfd = fileno(tmp);

    // Store original stdout's file descriptor
    stdout_fd = fileno(stdout);
    orig_stdout = dup(stdout_fd);

    // Make stdout point to the temp file
    dup2(tmpfd, stdout_fd);

    do_create(ctx,
        "cgroup_exporter_metrics",
        "mountpoint", DPATH_MOUNTPOINT,
        "scrape_interval", "1",
        "psi_controllers", "memory",
        NULL);
    TEST_CHECK(flb_start(ctx) == 0);

    sleep(1);

    flb_stop(ctx);
    flb_destroy(ctx);

    // Restore stdout
    dup2(orig_stdout, stdout_fd);

    check_output(tmp);

    // Close temp file
    fclose(tmp);
}

void flb_test_memory_metrics(void)
{
}

TEST_LIST = {
    { "system_psi", flb_test_system_psi },
    { "memory_metrics", flb_test_memory_metrics },
    { NULL, NULL }
};
