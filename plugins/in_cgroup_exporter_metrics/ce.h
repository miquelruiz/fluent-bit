/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef FLB_CGROUP_EXPORTER_H
#define FLB_CGROUP_EXPORTER_H

#define CE_DEFAULT_CGROUPS "system.slice"

#include <fluent-bit/flb_sds.h>

struct flb_ce
{
    /* configuration */
    int scrape_interval;
    flb_sds_t mountpoint;
    struct mk_list *cgroups;
};

#endif
