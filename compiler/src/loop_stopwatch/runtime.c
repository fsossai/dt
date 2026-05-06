#include "timers/stopwatch.h"

#include <stdint.h>
#include <stdio.h>

#define DT_SW_MAX_TAG 1024

static stopwatch_t stopwatches[DT_SW_MAX_TAG];
static int registered[DT_SW_MAX_TAG];

void __dt_sw_init(int64_t tag, const char *name) {
  if (tag < 0 || tag >= DT_SW_MAX_TAG)
    return;
  stopwatch_init(&stopwatches[tag], name);
  registered[tag] = 1;
}

void __dt_sw_start(int64_t tag) {
  stopwatch_start(&stopwatches[tag]);
}

void __dt_sw_stop(int64_t tag) {
  stopwatch_stop(&stopwatches[tag]);
}

__attribute__((destructor))
static void __dt_sw_print_all(void) {
  for (int i = 0; i < DT_SW_MAX_TAG; i++) {
    if (registered[i]) {
      stopwatch_print_stats(&stopwatches[i]);
    }
  }
}
