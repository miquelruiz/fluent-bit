/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "ce.h"

#include <cmetrics/cmt_counter.h>
#include <cmetrics/cmt_gauge.h>

#include <fluent-bit/flb_input_plugin.h>
#include <fluent-bit/flb_slist.h>

#define CE_MEMORY_MAX_LINE_SIZE 64
#define CE_MEMORY_MAX_FILE_NAME_SIZE 64

#define CE_MEMORY_PEAK "memory.peak"
#define CE_MEMORY_CURRENT "memory.current"
#define CE_MEMORY_SWAP_PEAK "memory.swap.peak"
#define CE_MEMORY_SWAP_CURRENT "memory.swap.current"
#define CE_MEMORY_EVENTS "memory.events"
#define CE_MEMORY_MIN "memory.min"
#define CE_MEMORY_LOW "memory.low"
#define CE_MEMORY_HIGH "memory.high"
#define CE_MEMORY_MAX "memory.max"

#define CE_DEFINE_COUNTER(var, name, description, attr) \
    var = cmt_counter_create(ctx->cmt,                  \
        "cgroups", "memory", name,                      \
        description,                                    \
        1, (char*[]) { "cgroup" });                     \
    if (!var) {                                         \
        return -1;                                      \
    }                                                   \
    attr = var;

#define CE_DEFINE_GAUGE(var, name, description, attr) \
    var = cmt_gauge_create(ctx->cmt,                  \
        "cgroups", "memory", name,                    \
        description,                                  \
        1, (char*[]) { "cgroup" });                   \
    if (!var) {                                       \
        return -1;                                    \
    }                                                 \
    attr = var;

static int ce_memory_init(struct flb_ce* ctx)
{
    struct cmt_counter* c;
    struct cmt_gauge* g;

    CE_DEFINE_GAUGE(g, "peak",
        "Max memory usage recorded", ctx->memory_peak);
    CE_DEFINE_GAUGE(g, "current",
        "Total amount of memory currently being used", ctx->memory_current);
    CE_DEFINE_GAUGE(g, "swap_peak",
        "Max swap usage recorded", ctx->memory_swap_peak);
    CE_DEFINE_GAUGE(g, "swap_current",
        "Total amount of swap currently being used", ctx->memory_swap_current);

    CE_DEFINE_GAUGE(g, "min",
        "Hard memory protection", ctx->memory_min);
    CE_DEFINE_GAUGE(g, "low",
        "Best-effort memory protection", ctx->memory_low);
    CE_DEFINE_GAUGE(g, "high",
        "Memory usage throttle limit", ctx->memory_high);
    CE_DEFINE_GAUGE(g, "max",
        "Memory usage hard limit", ctx->memory_max);

    CE_DEFINE_COUNTER(c, "events_low",
        "Number of times the cgroup is reclaimed due to high memory pressure",
        ctx->memory_events_low);
    CE_DEFINE_COUNTER(c, "events_high",
        "Number of times processes of the cgroup are throttled",
        ctx->memory_events_high);
    CE_DEFINE_COUNTER(c, "events_max",
        "Number of times the memory usage was about to "
        "go over the max boundary",
        ctx->memory_events_max);
    CE_DEFINE_COUNTER(c, "events_oom",
        "Number of times the memory usage reached the limit and "
        "allocation was about to fail",
        ctx->memory_events_oom);
    CE_DEFINE_COUNTER(c, "events_oom_kill",
        "Number of processes belonging to this cgroup killed",
        ctx->memory_events_oom_kill);
    CE_DEFINE_COUNTER(c, "events_oom_group_kill",
        "Number of times a group OOM has occurred",
        ctx->memory_events_oom_group_kill);

    return 0;
}

static int ce_build_file_path(flb_sds_t* target, flb_sds_t mountpoint, flb_sds_t cgroup, char* fname)
{
    flb_sds_t tmp;

    tmp = flb_sds_printf(target, "%s%s/%s", mountpoint, cgroup, fname);
    if (!tmp) {
        flb_errno();
        return -1;
    }
    *target = tmp;

    return 0;
}

static int ce_read_single_value_from_file(flb_sds_t path, double* value, bool falible)
{
    FILE* fp;
    char line[CE_MEMORY_MAX_LINE_SIZE];
    char* ret;

    fp = fopen(path, "r");
    if (fp == NULL) {
        // The root cgroup may lack some files, so this is fine
        if (falible) {
            return 0;
        }
        flb_errno();
        return -1;
    }

    ret = fgets(line, CE_MEMORY_MAX_LINE_SIZE - 1, fp);
    if (ret == NULL) {
        fclose(fp);
        return -1;
    }

    *value = strtod(line, NULL);

    fclose(fp);
    return 0;
}

static int ce_set_gauge_from_file(
    struct flb_ce* ctx,
    flb_sds_t cgroup,
    char* fname,
    struct cmt_gauge* g)
{
    int ret;
    double value;
    flb_sds_t path;

    path = flb_sds_create_size(
        flb_sds_len(ctx->mountpoint) + flb_sds_len(cgroup) + CE_MEMORY_MAX_FILE_NAME_SIZE);
    if (!path) {
        flb_errno();
        return -1;
    }

    ret = ce_build_file_path(&path, ctx->mountpoint, cgroup, fname);
    if (ret != 0) {
        flb_sds_destroy(path);
        return -1;
    }

    ret = ce_read_single_value_from_file(path, &value, FLB_TRUE);
    if (ret != 0) {
        flb_plg_error(ctx->ins, "couldn't read %s", path);
        flb_errno();
        flb_sds_destroy(path);
        return -1;
    }

    ret = cmt_gauge_set(g, cfl_time_now(), value, 1, (char*[]) { cgroup });
    if (ret != 0) {
        flb_plg_warn(ctx->ins, "failed to set memory gauge");
    }

    flb_sds_destroy(path);
    return 0;
}

static int ce_update_memory_metrics(struct flb_ce* ctx, flb_sds_t cgroup)
{
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_PEAK, ctx->memory_peak);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_CURRENT, ctx->memory_current);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_SWAP_PEAK, ctx->memory_swap_peak);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_SWAP_CURRENT, ctx->memory_swap_current);

    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_MIN, ctx->memory_min);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_LOW, ctx->memory_low);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_HIGH, ctx->memory_high);
    ce_set_gauge_from_file(ctx, cgroup, CE_MEMORY_MAX, ctx->memory_max);

    return 0;
}

static int ce_memory_update(struct flb_input_instance* ins, struct flb_config* config, void* in_context)
{
    struct mk_list* cg_head;
    struct flb_slist_entry* cgroup;
    struct flb_ce* ctx = (struct flb_ce*)in_context;

    mk_list_foreach(cg_head, ctx->cgroups)
    {
        cgroup = mk_list_entry(cg_head, struct flb_slist_entry, _head);
        ce_update_memory_metrics(ctx, cgroup->str);
    }

    return 0;
}

struct flb_ce_collector memory_collector = {
    .name = "memory",
    .cb_init = ce_memory_init,
    .cb_update = ce_memory_update,
    .cb_exit = NULL,
};