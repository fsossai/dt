#pragma once

#include <omp.h>

inline int tc_chain_id() {
  return omp_get_thread_num();
}

inline int tc_chain_length() {
  return omp_get_team_size(omp_get_level());
}
