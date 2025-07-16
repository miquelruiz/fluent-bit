/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "ce.h"

#include <fluent-bit/flb_config_map.h>
#include <fluent-bit/flb_input.h>
#include <stddef.h>

static struct flb_config_map config_map[] = {
    {FLB_CONFIG_MAP_TIME, "scrape_interval", "5",
     0, FLB_TRUE, offsetof(struct flb_ce, scrape_interval),
     "scrape interval to collect metrics from the node."},

    {FLB_CONFIG_MAP_STR, "mountpoint", "/sys/fs/cgroup",
     0, FLB_TRUE, offsetof(struct flb_ce, mountpoint),
     "where the cgroup hierarchy is mounted"},

    {FLB_CONFIG_MAP_CLIST, "cgroups",
     CE_DEFAULT_CGROUPS,
     0, FLB_TRUE, offsetof(struct flb_ce, cgroups),
     "Comma separated list of cgroups to monitor."},
};

struct flb_input_plugin in_cgroup_exporter_metrics_plugin = {
    .name = "cgroup_exporter_metrics",
    .description = "CGroup Exporter Metrics (Prometheus Compatible)",
    .cb_init = NULL,
    .cb_pre_run = NULL,
    .cb_collect = NULL,
    .cb_flush_buf = NULL,
    .config_map = config_map,
    .cb_pause = NULL,
    .cb_resume = NULL,
    .cb_exit = NULL,
    .flags = 0};
