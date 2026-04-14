#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <omp.h>

#include "stale_object.h"

static void test_stale_object_int(void) {
  skynet_stale_object_t obj;
  int v;
  int *ptr;
  int nt;

  printf("[stale_object_c] init object\n");
  assert(skynet_stale_object_init(&obj, sizeof(int)));
  assert(obj.pad > 0u);
  assert(obj.parts == 1u);

  v = 42;
  printf("[stale_object_c] set/get through wrappers\n");
  assert(skynet_stale_object_set(&obj, &v));
  ptr = (int *)skynet_stale_object_get(&obj);
  assert(ptr != NULL && *ptr == 42);

  printf("[stale_object_c] expand to 4 parts and copy lane0 into all lanes\n");
  skynet_stale_object_set_clause(4, &obj);
  assert(obj.parts == 4u);
  assert(*(int *)skynet_stale_object_get__(&obj, 1) == 42);
  assert(*(int *)skynet_stale_object_get__(&obj, 2) == 42);
  assert(*(int *)skynet_stale_object_get__(&obj, 3) == 42);

  v = 7;
  printf("[stale_object_c] set lane 2 via helper\n");
  assert(skynet_stale_object_set__(&obj, 2, &v));
  assert(*(int *)skynet_stale_object_get__(&obj, 2) == 7);
  assert(*(int *)skynet_stale_object_get__(&obj, 0) == 42);

  nt = omp_get_max_threads();
  if (nt < 2) {
    nt = 2;
  }
  printf("[stale_object_c] parallel set/get across lanes with OpenMP: threads=%d\n",
         nt);
  skynet_stale_object_set_clause(nt, &obj);

#pragma omp parallel num_threads(nt)
  {
    int t = omp_get_thread_num();
    int x = 1000 + t;
    int *lane;

    assert(skynet_stale_object_set__(&obj, t, &x));
    lane = (int *)skynet_stale_object_get__(&obj, t);
    assert(lane != NULL && *lane == x);
  }

  printf("[stale_object_c] validate lane values after parallel phase\n");
  for (int t = 0; t < nt; ++t) {
    assert(*(int *)skynet_stale_object_get__(&obj, t) == 1000 + t);
  }

  printf("[stale_object_c] destroy object\n");
  skynet_stale_object_destroy(&obj);
  assert(obj.container == NULL);
  assert(obj.parts == 0u);

  printf("[stale_object_c] all checks passed\n");
}

int main(void) {
  test_stale_object_int();
  return 0;
}
