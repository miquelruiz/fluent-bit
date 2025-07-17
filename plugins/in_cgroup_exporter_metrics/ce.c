/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "ce.h"

#include <cmetrics/cmetrics.h>
#include <fluent-bit/flb_config_map.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_input_plugin.h>
#include <fluent-bit/flb_mem.h>
#include <stddef.h>

struct flb_ce*
flb_ce_config_create(struct flb_input_instance* ins, struct flb_config* config)
{
    int ret;
    struct flb_ce* ctx;

    ctx = flb_calloc(1, sizeof(struct flb_ce));
    if (!ctx) {
        flb_errno();
        return NULL;
    }
    ctx->ins = ins;

    /* Load the config map */
    ret = flb_input_config_map_set(ins, (void*)ctx);
    if (ret == -1) {
        flb_free(ctx);
        return NULL;
    }

    ctx->cmt = cmt_create();
    if (!ctx->cmt) {
        flb_plg_error(ins, "could not initialize CMetrics");
        flb_free(ctx);
        return NULL;
    }

    return ctx;
}

static int
in_ce_init(struct flb_input_instance* in, struct flb_config* config, void* data)
{
    struct flb_ce* ctx;

    ctx = flb_ce_config_create(in, config);
    if (!ctx) {
        flb_errno();
        return -1;
    }

    flb_input_set_context(in, ctx);

    return 0;
}

static struct flb_config_map config_map[] = {
    { FLB_CONFIG_MAP_TIME,
        "scrape_interval",
        "5",
        0,
        FLB_TRUE,
        offsetof(struct flb_ce, scrape_interval),
        "scrape interval to collect metrics from the node." },

    { FLB_CONFIG_MAP_STR,
        "mountpoint",
        "/sys/fs/cgroup",
        0,
        FLB_TRUE,
        offsetof(struct flb_ce, mountpoint),
        "where the cgroup hierarchy is mounted" },

    { FLB_CONFIG_MAP_CLIST,
        "cgroups",
        CE_DEFAULT_CGROUPS,
        0,
        FLB_TRUE,
        offsetof(struct flb_ce, cgroups),
        "Comma separated list of cgroups to monitor." },
};

struct flb_input_plugin in_cgroup_exporter_metrics_plugin = {
    .name = "cgroup_exporter_metrics",
    .description = "CGroup Exporter Metrics (Prometheus Compatible)",
    .cb_init = in_ce_init,
    .cb_pre_run = NULL,
    .cb_collect = NULL,
    .cb_flush_buf = NULL,
    .config_map = config_map,
    .cb_pause = NULL,
    .cb_resume = NULL,
    .cb_exit = NULL,
    .flags = 0
};
