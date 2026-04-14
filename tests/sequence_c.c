#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <omp.h>

#include "sequence.h"

static void test_sequence_ordered_ints(void) {
  skynet_sequence_t seq;
  int values[] = { 10, 20, 30, 40 };
  int extra = 50;
  int lane1_a = 60;
  int lane1_b = 70;
  size_t i;
  int nt;
  int per_thread;
  size_t expected_total;
  size_t lane0_count;
  int *v;

  printf("[sequence_c] init sequence\n");
  assert(skynet_sequence_init(&seq, sizeof(int)));
  assert(skynet_sequence_empty(&seq));
  assert(skynet_sequence_size(&seq) == 0);

  printf("[sequence_c] append baseline values (wrapper + helper)\n");
  assert(skynet_sequence_append_many(&seq, (const uint8_t *)values, 4));
  assert(skynet_sequence_append(&seq, &extra));
  assert(skynet_sequence_append__(&seq, 0, &extra));
  assert(!skynet_sequence_append__(&seq, 1, &extra));
  assert(skynet_sequence_size(&seq) == 6);

  for (i = 0; i < skynet_sequence_size(&seq); ++i) {
    int *v = (int *)skynet_sequence_at(&seq, i);
    assert(v != NULL);
    if (i < 5) {
      assert(*v == ((int)(i + 1) * 10));
    } else {
      assert(*v == 50);
    }
  }

  assert(skynet_sequence_at(&seq, 999) == NULL);

  printf("[sequence_c] expand to 2 lanes and append to lane 1\n");
  assert(seq.pad > 0);
  skynet_sequence_append_clause(2, &seq);
  assert(skynet_sequence_append__(&seq, 1, &lane1_a));
  assert(skynet_sequence_append__(&seq, 1, &lane1_b));
  assert(*(int *)skynet_sequence_at(&seq, 6) == 60);
  assert(*(int *)skynet_sequence_at(&seq, 7) == 70);

  skynet_sequence_clear(&seq);
  assert(skynet_sequence_empty(&seq));
  assert(skynet_sequence_size(&seq) == 0);

  assert(skynet_sequence_append(&seq, &values[0]));
  assert(skynet_sequence_append_many__(&seq, 0, (const uint8_t *)(values + 1), 2));
  assert(skynet_sequence_append_many__(&seq, 1, (const uint8_t *)(values + 1), 2));
  assert(skynet_sequence_size(&seq) == 5);
  assert(*(int *)skynet_sequence_at(&seq, 0) == 10);

  /*
   * More complete workload:
   * 1) append a larger sequential block into lane 0 through the wrapper API
   * 2) append in parallel with OpenMP, one lane per thread
   */
  printf("[sequence_c] append larger sequential block to lane 0\n");
  {
    int bulk[128];
    for (i = 0; i < 128; ++i) {
      bulk[i] = 1000 + (int)i;
    }
    assert(skynet_sequence_append_many(&seq, (const uint8_t *)bulk, 128));
  }

  lane0_count = skynet_sequence_size(&seq);
  assert(lane0_count == 133);

  /* Start a clean phase for deterministic parallel checks. */
  printf("[sequence_c] clear before parallel phase\n");
  skynet_sequence_clear(&seq);
  assert(skynet_sequence_size(&seq) == 0);

  nt = omp_get_max_threads();
  if (nt < 2) {
    nt = 2;
  }
  per_thread = 64;
  printf("[sequence_c] parallel append with OpenMP: threads=%d per_thread=%d\n",
         nt,
         per_thread);
  skynet_sequence_append_clause(nt, &seq);

#pragma omp parallel num_threads(nt)
  {
    int t = omp_get_thread_num();
    int j;
    for (j = 0; j < per_thread; ++j) {
      int value = (t + 1) * 100000 + j;
      assert(skynet_sequence_append__(&seq, t, &value));
    }
  }

  expected_total = (size_t)nt * (size_t)per_thread;
  printf("[sequence_c] validate total size and lane ordering\n");
  assert(skynet_sequence_size(&seq) == expected_total);

  /* Lane 0 chunk. */
  v = (int *)skynet_sequence_at(&seq, 0);
  assert(v != NULL && *v == 100000);
  v = (int *)skynet_sequence_at(&seq, (size_t)per_thread - 1u);
  assert(v != NULL && *v == 100000 + (per_thread - 1));

  /* Lane 1 starts after all lane 0 entries in global ordered indexing. */
  v = (int *)skynet_sequence_at(&seq, (size_t)per_thread);
  assert(v != NULL && *v == 200000);
  v = (int *)skynet_sequence_at(&seq, ((size_t)per_thread * 2u) - 1u);
  assert(v != NULL && *v == 200000 + (per_thread - 1));

  printf("[sequence_c] destroy sequence\n");
  skynet_sequence_destroy(&seq);
  assert(skynet_sequence_data(&seq) == NULL);
  assert(skynet_sequence_size(&seq) == 0);
  printf("[sequence_c] all checks passed\n");
}

int main(void) {
  test_sequence_ordered_ints();
  return 0;
}
