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

void flb_ce_config_destroy(struct flb_ce* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->cmt) {
        cmt_destroy(ctx->cmt);
    }

    flb_free(ctx);
}

static int in_ce_collect(struct flb_input_instance* ins,
    struct flb_config* config, void* in_context)
{
    int ret;
    struct flb_ce* ctx = in_context;

    /* Append the updated metrics */
    ret = flb_input_metrics_append(ins, NULL, 0, ctx->cmt);
    if (ret != 0) {
        flb_plg_error(ins, "could not append cgroup metrics");
    }

    return 0;
}
static int collectors_common_init(
    struct flb_ce* ctx,
    struct flb_config* config,
    struct flb_ce_collector* coll)
{
    int ret;

    if (coll == NULL) {
        return -1;
    }

    if (coll->cb_init == NULL) {
        flb_plg_warn(ctx->ins, "%s collector is not supported", coll->name);
        return 0;
    }

    coll->interval = ctx->scrape_interval;
    if (coll->cb_update) {
        ret = flb_input_set_collector_time(ctx->ins,
            coll->cb_update, coll->interval, 0, config);
        if (ret < 0) {
            flb_plg_error(ctx->ins, "flb_input_set_collector_time failed");
            return -1;
        }
        coll->coll_fd = ret;
    }

    ret = coll->cb_init(ctx);
    if (ret != 0) {
        flb_plg_error(ctx->ins, "%s collector init failed", coll->name);
        return -1;
    }
    coll->activated = FLB_TRUE;

    if (coll->cb_update) {
        coll->cb_update(ctx->ins, config, ctx);
    }

    return 0;
}

static int
in_ce_init(struct flb_input_instance* in, struct flb_config* config, void* data)
{
    int ret;
    struct flb_ce* ctx;
    struct mk_list* head;
    struct flb_ce_collector* coll;

    ctx = flb_ce_config_create(in, config);
    if (!ctx) {
        flb_errno();
        return -1;
    }

    ctx->coll_fd = -1;

    mk_list_init(&ctx->collectors);
    mk_list_add(&psi_collector._head, &ctx->collectors);

    mk_list_foreach(head, &ctx->collectors)
    {
        coll = mk_list_entry(head, struct flb_ce_collector, _head);
        collectors_common_init(ctx, config, coll);
    }

    flb_input_set_context(in, ctx);

    ret = flb_input_set_collector_time(in,
        in_ce_collect, ctx->scrape_interval, 0, config);
    if (ret == -1) {
        flb_plg_error(ctx->ins, "Could not set collector");
        flb_free(ctx);
        return -1;
    }
    ctx->coll_fd = ret;

    return 0;
}

static int in_ce_exit(void* data, struct flb_config* config)
{
    struct flb_ce* ctx = data;

    if (!ctx) {
        return 0;
    }

    // TODO call cb_exit on every collector

    flb_ce_config_destroy(ctx);
    return 0;
}

static struct flb_config_map config_map[] = {
    { FLB_CONFIG_MAP_TIME,
        "scrape_interval",
        CE_DEFAULT_SCRAPE_INTERVAL,
        0,
        FLB_TRUE,
        offsetof(struct flb_ce, scrape_interval),
        "scrape interval to collect metrics from the node." },

    { FLB_CONFIG_MAP_STR,
        "mountpoint",
        CE_DEFAULT_CGROUPS_MOUNTPOINT,
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

    { FLB_CONFIG_MAP_CLIST,
        "psi_controllers",
        CE_DEFAULT_CONTROLLERS,
        0,
        FLB_TRUE,
        offsetof(struct flb_ce, psi_controllers),
        "Comma separated list of controllers to monitor PSI" },
};

struct flb_input_plugin in_cgroup_exporter_metrics_plugin = {
    .name = "cgroup_exporter_metrics",
    .description = "CGroup Exporter Metrics (Prometheus Compatible)",
    .cb_init = in_ce_init,
    .cb_pre_run = NULL,
    .cb_collect = in_ce_collect,
    .cb_flush_buf = NULL,
    .config_map = config_map,
    .cb_pause = NULL,
    .cb_resume = NULL,
    .cb_exit = in_ce_exit,
    .flags = 0
};
