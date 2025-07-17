set(src
  ce.c
  ce_memory.c
)

FLB_PLUGIN(in_cgroup_exporter_metrics "${src}" "")
