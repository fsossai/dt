#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "arcana/noelle/core/Pragma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skynet_sequence_lane {
  void *data;
  size_t size;
  size_t capacity;
} skynet_sequence_lane_t;

typedef struct skynet_sequence {
  skynet_sequence_lane_t *lanes;
  size_t parts;
  uint32_t pad;
  size_t elem_size;
} skynet_sequence_t;

static inline int skynet_sequence_append_clause(int n, skynet_sequence_t *seq);

static inline uint32_t skynet_sequence_default_pad(void) {
  uint32_t pad = skynet_compute_padding_for_size(sizeof(skynet_sequence_lane_t));
  return pad == 0u ? 1u : pad;
}

static inline int skynet_sequence_init(skynet_sequence_t *seq, size_t elem_size) {
  if (seq == NULL || elem_size == 0u) {
    return 0;
  }

  seq->pad = skynet_sequence_default_pad();
  if (seq->pad == 0u) {
    return 0;
  }

  seq->elem_size = elem_size;
  seq->parts = 1u;
  seq->lanes = (skynet_sequence_lane_t *)calloc((size_t)seq->pad,
                                                sizeof(skynet_sequence_lane_t));
  if (seq->lanes == NULL) {
    seq->parts = 0u;
    return 0;
  }

  return 1;
}

static inline int skynet_sequence_reserve__(skynet_sequence_t *seq,
                                            int t,
                                            size_t capacity) {
  skynet_sequence_lane_t *lane;
  void *new_data;

  if (seq == NULL || seq->lanes == NULL || seq->elem_size == 0u || t < 0) {
    return 0;
  }
  if ((size_t)t >= seq->parts) {
    return 0;
  }

  lane = &seq->lanes[(size_t)t * (size_t)seq->pad];
  if (capacity <= lane->capacity) {
    return 1;
  }
  if (capacity > (SIZE_MAX / seq->elem_size)) {
    return 0;
  }

  new_data = realloc(lane->data, capacity * seq->elem_size);
  if (new_data == NULL) {
    return 0;
  }

  lane->data = new_data;
  lane->capacity = capacity;
  return 1;
}

static inline int skynet_sequence_reserve(skynet_sequence_t *seq, size_t capacity) {
  return skynet_sequence_reserve__(seq, 0, capacity);
}

static inline int skynet_sequence_append__(skynet_sequence_t *seq,
                                           int t,
                                           const void *elem) {
  skynet_sequence_lane_t *lane;
  size_t new_capacity;
  uint8_t *dst;

  if (seq == NULL || seq->lanes == NULL || elem == NULL || t < 0) {
    return 0;
  }
  if ((size_t)t >= seq->parts) {
    return 0;
  }

  lane = &seq->lanes[(size_t)t * (size_t)seq->pad];
  if (lane->size == lane->capacity) {
    new_capacity = (lane->capacity == 0u) ? 1u : (lane->capacity * 2u);
    if (new_capacity < lane->capacity) {
      return 0;
    }
    if (!skynet_sequence_reserve__(seq, t, new_capacity)) {
      return 0;
    }
  }

  dst = (uint8_t *)lane->data + (lane->size * seq->elem_size);
  memcpy(dst, elem, seq->elem_size);
  lane->size += 1u;
  return 1;
}

static inline int skynet_sequence_append(skynet_sequence_t *seq, const void *elem) {
  int k = 0;
  int ok = 0;
  int p = noelle_pragma_begin("ldtc",
                              &k,
                              0,
                              skynet_sequence_append_clause,
                              seq);
  if (seq != NULL && elem != NULL) {
    ok = skynet_sequence_append__(seq, k, elem);
  }
  noelle_pragma_end(p);
  return ok;
}

static inline int skynet_sequence_append_many__(skynet_sequence_t *seq,
                                                int t,
                                                const uint8_t *elems,
                                                size_t n) {
  size_t i;

  if (seq == NULL || (elems == NULL && n != 0u)) {
    return 0;
  }

  for (i = 0u; i < n; ++i) {
    if (!skynet_sequence_append__(seq, t, elems + (i * seq->elem_size))) {
      return 0;
    }
  }
  return 1;
}

static inline int skynet_sequence_append_many(skynet_sequence_t *seq,
                                              const uint8_t *elems,
                                              size_t n) {
  int k = 0;
  int ok = 0;
  int p = noelle_pragma_begin("ldtc",
                              &k,
                              0,
                              skynet_sequence_append_clause,
                              seq);
  if (seq != NULL && (elems != NULL || n == 0u)) {
    ok = skynet_sequence_append_many__(seq, k, elems, n);
  }
  noelle_pragma_end(p);
  return ok;
}

static inline int skynet_sequence_append_clause(int n, skynet_sequence_t *seq) {
  size_t parts;
  size_t old_count;
  size_t new_count;
  skynet_sequence_lane_t *new_lanes;

  if (seq == NULL || n <= 0) {
    return 0;
  }

  parts = (size_t)n;
  if (seq->pad == 0u || parts == 0u) {
    return 0;
  }
  if (parts <= seq->parts) {
    return 1;
  }

  old_count = seq->parts * (size_t)seq->pad;
  new_count = parts * (size_t)seq->pad;
  if (old_count > SIZE_MAX / sizeof(skynet_sequence_lane_t) ||
      new_count > SIZE_MAX / sizeof(skynet_sequence_lane_t)) {
    return 0;
  }

  new_lanes = (skynet_sequence_lane_t *)realloc(
      seq->lanes, new_count * sizeof(skynet_sequence_lane_t));
  if (new_lanes == NULL) {
    return 0;
  }

  memset(new_lanes + old_count,
         0,
         (new_count - old_count) * sizeof(skynet_sequence_lane_t));
  seq->lanes = new_lanes;
  seq->parts = parts;
  return 1;
}

static inline void *skynet_sequence_at(skynet_sequence_t *seq, size_t idx) {
  size_t i;
  size_t j;

  if (seq == NULL || seq->lanes == NULL) {
    return NULL;
  }

  j = idx;
  for (i = 0u; i < seq->parts; ++i) {
    skynet_sequence_lane_t *lane = &seq->lanes[i * (size_t)seq->pad];
    if (j < lane->size) {
      return (uint8_t *)lane->data + (j * seq->elem_size);
    }
    j -= lane->size;
  }
  return NULL;
}

static inline void skynet_sequence_clear(skynet_sequence_t *seq) {
  size_t i;

  if (seq == NULL || seq->lanes == NULL) {
    return;
  }
  for (i = 0u; i < seq->parts; ++i) {
    skynet_sequence_lane_t *lane = &seq->lanes[i * (size_t)seq->pad];
    lane->size = 0u;
  }
}

static inline void skynet_sequence_destroy(skynet_sequence_t *seq) {
  size_t i;

  if (seq == NULL) {
    return;
  }

  if (seq->lanes != NULL) {
    for (i = 0u; i < seq->parts; ++i) {
      skynet_sequence_lane_t *lane = &seq->lanes[i * (size_t)seq->pad];
      free(lane->data);
      lane->data = NULL;
      lane->size = 0u;
      lane->capacity = 0u;
    }
    free(seq->lanes);
  }

  seq->lanes = NULL;
  seq->parts = 0u;
  seq->pad = 0u;
  seq->elem_size = 0u;
}

static inline size_t skynet_sequence_size(const skynet_sequence_t *seq) {
  size_t i;
  size_t sum;

  if (seq == NULL || seq->lanes == NULL) {
    return 0u;
  }

  sum = 0u;
  for (i = 0u; i < seq->parts; ++i) {
    sum += seq->lanes[i * (size_t)seq->pad].size;
  }
  return sum;
}

static inline int skynet_sequence_empty(const skynet_sequence_t *seq) {
  return skynet_sequence_size(seq) == 0u ? 1 : 0;
}

static inline void *skynet_sequence_data(skynet_sequence_t *seq) {
  if (seq == NULL || seq->lanes == NULL) {
    return NULL;
  }
  return seq->lanes[0].data;
}

#ifdef __cplusplus
}
#endif
