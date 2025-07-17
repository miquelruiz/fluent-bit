#include "flb_tests_runtime.h"
#include <fluent-bit.h>

#define DPATH_MOUNTPOINT FLB_TESTS_DATA_PATH "/data/in_cgroup_exporter_metrics/sys/fs/cgroup"

char* expected_metrics[] = { "\"psi_total_seconds\"" };

struct str_list {
    size_t size;
    char** lists;
};

void do_create(flb_ctx_t* ctx, struct flb_lib_out_cb* cb_data, char* system, ...)
{
    int ret;
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

    out_ffd = flb_output(ctx, (char*)"lib", (void*)cb_data);
    TEST_CHECK(out_ffd >= 0);

    ret = flb_output_set(ctx, out_ffd, "format", "json", NULL);
    TEST_CHECK(ret == 0);

    TEST_CHECK(flb_service_set(ctx,
                   "Flush", "0.5",
                   "Grace", "1",
                   "log_level", "debug",
                   NULL)
        == 0);
}

static int cb_check_metrics(void* record, size_t size, void* data)
{
    size_t i;
    char* found;
    char* json = (char*)record;
    struct str_list* metrics = (struct str_list*)data;

    TEST_CHECK(json != NULL);
    TEST_CHECK(size > 0);

    // TODO maybe decode the json and inspect it more thoroughly
    puts(json);
    for (i = 0; i < metrics->size; i++) {
        found = strstr(json, metrics->lists[i]);
        if (!TEST_CHECK(found != NULL)) {
            TEST_MSG("Expected to find: '%s' in result '%s'",
                metrics->lists[i], json);
        }
    }

    return 0;
}

void flb_test_system_psi()
{
    struct flb_lib_out_cb cb_data;

    flb_ctx_t* ctx = flb_create();

    struct str_list expected = {
        .size = sizeof(expected_metrics) / sizeof(char*),
        .lists = &expected_metrics[0],

    };
    cb_data.cb = cb_check_metrics;
    cb_data.data = &expected;

    do_create(ctx,
        &cb_data,
        "cgroup_exporter_metrics",
        "mountpoint", DPATH_MOUNTPOINT,
        "scrape_interval", "1",
        "psi_controllers", "memory",
        NULL);
    TEST_CHECK(flb_start(ctx) == 0);

    sleep(1);

    flb_stop(ctx);
    flb_destroy(ctx);
}

TEST_LIST = {
    { "system_psi", flb_test_system_psi },
    { NULL, NULL }
};