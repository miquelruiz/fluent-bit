/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "ce.h"

#include <fluent-bit/flb_input_plugin.h>
#include <fluent-bit/flb_sds.h>

#include <cmetrics/cmt_counter.h>
#include <cmetrics/cmt_gauge.h>

#include <stdio.h>

#define CE_PSI_SOME "some"
#define CE_PSI_FULL "full"
#define CE_PSI_KIND_SOME 0
#define CE_PSI_KIND_FULL 1

#define CE_PSI_PRESSURE_SUFFIX ".pressure"
#define CE_PSI_PRESSURE_SUFFIX_LEN 9

#define CE_PSI_MAX_LINE_SIZE 256

struct ce_psi {
    int kind;
    float avg10;
    float avg60;
    float avg300;
    uint64_t total;
};

static int ce_psi_init(struct flb_ce* ctx)
{
    struct cmt_counter* c;

    c = cmt_counter_create(ctx->cmt,
        "ce", "psi", "psi_total_seconds",
        "Absolute stall time",
        3, (char*[]) { "controller", "cgroup", "kind" });
    if (!c) {
        return -1;
    }
    ctx->psi_total_seconds = c;

    return 0;
}

static int ce_parse_psi_line(char* line, struct ce_psi* psi)
{
    int ret;

    char kind[5];
    ret = sscanf(
        line,
        "%4s avg10=%f avg60=%f avg300=%f total=%lu",
        kind,
        &psi->avg10,
        &psi->avg60,
        &psi->avg300,
        &psi->total);

    if (ret < 5) {
        return -1;
    }

    if (strncmp(kind, CE_PSI_SOME, 4) == 0) {
        psi->kind = CE_PSI_KIND_SOME;
    } else if (strncmp(kind, CE_PSI_FULL, 4) == 0) {
        psi->kind = CE_PSI_KIND_FULL;
    } else {
        return -1;
    }

    return 0;
}

static int ce_update_psi_metrics(struct flb_ce* ctx, char* controller, char* cgroup)
{
    int ret;
    char line[CE_PSI_MAX_LINE_SIZE];
    flb_sds_t path;
    flb_sds_t tmp;
    struct ce_psi psi;

    FILE* fp = NULL;

    path = flb_sds_create_size(
        flb_sds_len(ctx->mountpoint) + strlen(cgroup) + strlen(controller) + CE_PSI_PRESSURE_SUFFIX_LEN + 2);
    if (!path) {
        flb_errno();
        return -1;
    }

    tmp = flb_sds_printf(&path, strlen(cgroup) == 1 ? "%s%s%s%s" : "%s%s/%s%s",
        ctx->mountpoint,
        cgroup,
        controller,
        CE_PSI_PRESSURE_SUFFIX);
    if (!tmp) {
        flb_errno();
        flb_sds_destroy(path);
        return -1;
    }
    path = tmp;

    fp = fopen(path, "r");
    if (fp == NULL) {
        flb_plg_error(ctx->ins, "couldn't open %s", path);
        flb_errno();
        flb_sds_destroy(path);
        return -1;
    }

    while (fgets(line, CE_PSI_MAX_LINE_SIZE - 1, fp) != NULL) {
        ret = ce_parse_psi_line(line, &psi);
        if (ret != 0) {
            flb_plg_warn(ctx->ins, "malformed psi data in %s", path);
            flb_sds_destroy(path);
            return -1;
        }

        ret = cmt_counter_set(
            ctx->psi_total_seconds,
            cfl_time_now(),
            psi.total,
            3,
            (char*[]) { controller, cgroup, psi.kind == CE_PSI_KIND_FULL ? CE_PSI_FULL : CE_PSI_SOME });
        if (ret != 0) {
            flb_plg_warn(ctx->ins, "failed to set psi counter");
        }
    }

    flb_sds_destroy(path);
    return 0;
}

static int ce_psi_update(struct flb_input_instance* ins, struct flb_config* config, void* in_context)
{
    struct mk_list *cg_head, *pc_head;
    struct flb_slist_entry *cgroup, *pc;

    struct flb_ce* ctx = (struct flb_ce*)in_context;

    mk_list_foreach(cg_head, ctx->cgroups)
    {
        cgroup = mk_list_entry(cg_head, struct flb_slist_entry, _head);
        mk_list_foreach(pc_head, ctx->psi_controllers)
        {
            pc = mk_list_entry(pc_head, struct flb_slist_entry, _head);
            ce_update_psi_metrics(ctx, pc->str, cgroup->str);
        }
    }
    return 0;
}

struct flb_ce_collector psi_collector = {
    .name = "psi",
    .cb_init = ce_psi_init,
    .cb_update = ce_psi_update,
    .cb_exit = NULL,
};