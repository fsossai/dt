#pragma once

#include <omp.h>

int tc_chain_id() {
  return omp_get_thread_num();
}

int tc_chain_length() {
  return omp_get_team_size(omp_get_level());
}
