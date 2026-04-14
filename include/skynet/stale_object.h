#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arcana/noelle/core/Pragma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skynet_stale_object {
  uint8_t *container;
  size_t parts;
  uint32_t pad;
  size_t elem_size;
} skynet_stale_object_t;

static inline void skynet_stale_object_set_clause(int n,
                                                  skynet_stale_object_t *obj);

static inline uint32_t skynet_stale_object_default_pad(size_t elem_size) {
  uint32_t raw;
  if (elem_size == 0u) {
    return 0u;
  }
  if (elem_size >= SKYNET_L1D_CACHE_LINE_SIZE) {
    return 1u;
  }
  raw = SKYNET_L1D_CACHE_LINE_SIZE / (uint32_t)elem_size;
  return raw == 0u ? 1u : raw;
}

static inline int skynet_stale_object_init(skynet_stale_object_t *obj,
                                           size_t elem_size) {
  size_t bytes;

  if (obj == NULL || elem_size == 0u) {
    return 0;
  }

  obj->pad = skynet_stale_object_default_pad(elem_size);
  if (obj->pad == 0u) {
    return 0;
  }

  obj->parts = 1u;
  obj->elem_size = elem_size;
  bytes = (size_t)obj->pad * obj->elem_size;
  obj->container = (uint8_t *)calloc(bytes, 1u);
  if (obj->container == NULL) {
    obj->parts = 0u;
    obj->elem_size = 0u;
    obj->pad = 0u;
    return 0;
  }

  return 1;
}

static inline void skynet_stale_object_destroy(skynet_stale_object_t *obj) {
  if (obj == NULL) {
    return;
  }

  free(obj->container);
  obj->container = NULL;
  obj->parts = 0u;
  obj->pad = 0u;
  obj->elem_size = 0u;
}

static inline void *skynet_stale_object_get__(skynet_stale_object_t *obj,
                                              int k) {
  size_t slot;

  if (obj == NULL || obj->container == NULL || k < 0) {
    return NULL;
  }
  if ((size_t)k >= obj->parts) {
    return NULL;
  }

  slot = (size_t)k * (size_t)obj->pad;
  return obj->container + (slot * obj->elem_size);
}

static inline void *skynet_stale_object_get(skynet_stale_object_t *obj) {
  int k = 0;
  void *result = NULL;
  int p = noelle_pragma_begin("ldtc", &k, 0);

  result = skynet_stale_object_get__(obj, k);

  noelle_pragma_end(p);
  return result;
}

static inline int skynet_stale_object_set__(skynet_stale_object_t *obj,
                                            int k,
                                            const void *value) {
  void *dst;

  if (obj == NULL || value == NULL) {
    return 0;
  }

  dst = skynet_stale_object_get__(obj, k);
  if (dst == NULL) {
    return 0;
  }

  memcpy(dst, value, obj->elem_size);
  return 1;
}

static inline int skynet_stale_object_set(skynet_stale_object_t *obj,
                                          const void *value) {
  int k = 0;
  int ok = 0;
  int p =
      noelle_pragma_begin("ldtc", &k, 0, skynet_stale_object_set_clause, obj);

  if (obj != NULL && value != NULL) {
    ok = skynet_stale_object_set__(obj, k, value);
  }

  noelle_pragma_end(p);
  return ok;
}

static inline void skynet_stale_object_set_clause(int n,
                                                  skynet_stale_object_t *obj) {
  size_t parts;
  size_t old_slots;
  size_t new_slots;
  size_t new_bytes;
  uint8_t *new_container;
  size_t i;
  uint8_t *head;

  if (obj == NULL || obj->elem_size == 0u || obj->pad == 0u || n <= 0) {
    return;
  }

  parts = (size_t)n;
  if (obj->parts == parts) {
    return;
  }

  old_slots = obj->parts * (size_t)obj->pad;
  new_slots = parts * (size_t)obj->pad;
  if (new_slots > SIZE_MAX / obj->elem_size) {
    return;
  }
  new_bytes = new_slots * obj->elem_size;

  new_container = (uint8_t *)realloc(obj->container, new_bytes);
  if (new_container == NULL) {
    return;
  }

  if (new_slots > old_slots) {
    memset(new_container + (old_slots * obj->elem_size),
           0,
           (new_slots - old_slots) * obj->elem_size);
  }

  obj->container = new_container;
  obj->parts = parts;

  head = obj->container;
  for (i = 1u; i < obj->parts; ++i) {
    uint8_t *dst = obj->container + (i * (size_t)obj->pad * obj->elem_size);
    memcpy(dst, head, obj->elem_size);
  }
}

#ifdef __cplusplus
}
#endif
