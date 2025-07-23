set(src
  ce_memory.c
  ce_psi.c
  ce.c
)

FLB_PLUGIN(in_cgroup_exporter_metrics "${src}" "")
