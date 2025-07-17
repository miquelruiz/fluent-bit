/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef FLB_CGROUP_EXPORTER_H
#define FLB_CGROUP_EXPORTER_H

#include <cmetrics/cmetrics.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_sds.h>

#define CE_DEFAULT_CGROUPS "system.slice"

struct flb_ce {
    /* configuration */
    int scrape_interval;
    flb_sds_t mountpoint;
    struct mk_list* cgroups;

    struct cmt* cmt;
    struct flb_input_instance* ins;
};

struct flb_ce_collector {
    const char* name;
    int interval;
    int activated;

    int (*cb_init)(struct flb_ce* ctx);
    int (*cb_update)(struct flb_input_instance* ins, struct flb_config* conf, void* in_context);
    int (*cb_exit)(struct flb_ce* ctx);
};

#endif
