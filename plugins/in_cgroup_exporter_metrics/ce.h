/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef FLB_CGROUP_EXPORTER_H
#define FLB_CGROUP_EXPORTER_H

#include "ce_psi.h"

#include <cmetrics/cmetrics.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_sds.h>

#define CE_DEFAULT_CGROUPS "/"
#define CE_DEFAULT_CONTROLLERS "cpu,memory,io"
#define CE_DEFAULT_SCRAPE_INTERVAL "5"
#define CE_DEFAULT_CGROUPS_MOUNTPOINT "/sys/fs/cgroup"

struct flb_ce {
    /* configuration */
    int scrape_interval;
    flb_sds_t mountpoint;
    struct mk_list* cgroups;
    struct mk_list* psi_controllers;

    struct cmt* cmt;
    struct flb_input_instance* ins;

    int coll_fd;
    struct mk_list collectors;

    /* PSI metrics */
    struct cmt_counter* psi_total_seconds;
};

struct flb_ce_collector {
    const char* name;
    int coll_fd;
    int interval;
    int activated;

    int (*cb_init)(struct flb_ce* ctx);
    int (*cb_update)(struct flb_input_instance* ins, struct flb_config* conf, void* in_context);
    int (*cb_exit)(struct flb_ce* ctx);

    struct mk_list _head;
};

#endif
