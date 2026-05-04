#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wasm/error.h"

typedef struct hmap_slot hmap_slot_t;

typedef struct hmap {
    size_t cap_order;
    size_t size;
    hmap_slot_t* slots;
} hmap_t;

#define HMAP_INIT \
    ((hmap_t){.cap_order = 0, .size = 0, .slots = NULL})

typedef struct hmap_iter {
    size_t i;
    const hmap_t* table;
} hmap_iter_t;

void hmap_free(hmap_t* table);
wasm_err_t hmap_insert(hmap_t* table, uint64_t key, uint64_t value);
bool hmap_lookup(const hmap_t* table, uint64_t key, uint64_t* out_value);
void hmap_delete(hmap_t* table, uint64_t key);

void hmap_iter(const hmap_t* table, hmap_iter_t* iter);
bool hmap_iter_next(hmap_iter_t* iter, uint64_t* key, uint64_t* value);
