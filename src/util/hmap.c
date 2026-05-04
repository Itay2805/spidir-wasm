#include "hmap.h"

#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "except.h"

#define GOLDEN_RATIO_64 0x61C8864680B583EBull

#define MIN_CAP_ORDER 3
// Safety, should still allow 16M entries at 50% load factor.
#define MAX_CAP_ORDER 25

#define MAX_LOAD_FACTOR_NUM 4
#define MAX_LOAD_FACTOR_DENOM 5

#define MIN_LOAD_FACTOR_NUM 1
#define MIN_LOAD_FACTOR_DENOM 8

typedef enum hmap_slot_state {
    SH_SLOT_FREE = 0,
    SH_SLOT_USED,
    SH_SLOT_TOMBSTONE,
} hmap_slot_state_t;

struct hmap_slot {
    hmap_slot_state_t state;
    uint64_t key;
    uint64_t value;
};

static size_t hash_value(const hmap_t* table, uint64_t key) {
    return (key * GOLDEN_RATIO_64) >> (64 - table->cap_order);
}

static size_t hmap_capacity(const hmap_t* table) {
    return 1 << table->cap_order;
}

static size_t hmap_get_probe_slot(const hmap_t* table, size_t hash, size_t i) {
    size_t slot = hash + (uint64_t) i * (i + 1) / 2;
    return slot & (hmap_capacity(table) - 1);
}

static hmap_slot_t* hmap_find_slot(hmap_t* table, uint64_t key, bool find_free) {
    if (!table->slots) {
        return nullptr;
    }

    size_t cap = hmap_capacity(table);
    size_t hash = hash_value(table, key);

    for (size_t i = 0; i < cap; i++) {
        hmap_slot_t* slot = &table->slots[hmap_get_probe_slot(table, hash, i)];
        switch (slot->state) {
            case SH_SLOT_USED:
                if (slot->key == key) {
                    return slot;
                }
                break;

            case SH_SLOT_FREE:
                return find_free ? slot : nullptr;

            case SH_SLOT_TOMBSTONE:
                if (find_free) {
                    return slot;
                }
                continue;
        }
    }

    return nullptr;
}

static hmap_slot_t* hmap_find_existing_slot(hmap_t* table, uint64_t key) {
    return hmap_find_slot(table, key, false);
}

static bool hmap_should_grow(const hmap_t* table) {
    return !table->slots || table->size * MAX_LOAD_FACTOR_DENOM >=
                                hmap_capacity(table) * MAX_LOAD_FACTOR_NUM;
}

static bool hmap_should_shrink(const hmap_t* table) {
    return table->cap_order > MIN_CAP_ORDER &&
           table->size * MIN_LOAD_FACTOR_DENOM <=
               hmap_capacity(table) * MIN_LOAD_FACTOR_NUM;
}

static wasm_err_t hmap_insert_new(hmap_t* table, uint64_t key, uint64_t value) {
    wasm_err_t err = WASM_NO_ERROR;

    hmap_slot_t* slot = hmap_find_slot(table, key, true);
    CHECK(slot != nullptr);
    CHECK(slot->state != SH_SLOT_USED);

    slot->key = key;
    slot->value = value;
    slot->state = SH_SLOT_USED;
    table->size++;

cleanup:
    return err;
}

static wasm_err_t hmap_rehash(hmap_t* table, size_t new_cap_order) {
    wasm_err_t err = WASM_NO_ERROR;
    hmap_t new_table = HMAP_INIT;

    new_cap_order = MAX(new_cap_order, MIN_CAP_ORDER);
    CHECK(new_cap_order <= MAX_CAP_ORDER);
    new_table.cap_order = new_cap_order;

    new_table.slots = CALLOC(hmap_slot_t, hmap_capacity(&new_table));
    CHECK(new_table.slots != nullptr);

    size_t old_cap = hmap_capacity(table);
    if (table->slots) {
        for (size_t i = 0; i < old_cap; i++) {
            hmap_slot_t* slot = &table->slots[i];

            // Copy any used slots into the new table.
            if (slot->state == SH_SLOT_USED) {
                RETHROW(hmap_insert_new(&new_table, slot->key, slot->value));
            }
        }
    }

    // Steal the new table now that it's ready.
    hmap_free(table);
    *table = new_table;
    new_table = HMAP_INIT;

cleanup:
    hmap_free(&new_table);
    return err;
}

void hmap_free(hmap_t* table) {
    wasm_host_free(table->slots);
    *table = HMAP_INIT;
}

wasm_err_t hmap_insert(hmap_t* table, uint64_t key, uint64_t value) {
    wasm_err_t err = WASM_NO_ERROR;

    hmap_slot_t* slot = hmap_find_slot(table, key, false);

    if (slot) {
        slot->value = value;
        goto cleanup;
    }

    if (hmap_should_grow(table)) {
        RETHROW(hmap_rehash(table, table->cap_order + 1));
    }

    RETHROW(hmap_insert_new(table, key, value));

cleanup:
    return err;
}

bool hmap_lookup(const hmap_t* table, uint64_t key, uint64_t* out_value) {
    hmap_slot_t* slot = hmap_find_existing_slot((hmap_t*) table, key);
    if (slot == nullptr) {
        return false;
    }

    *out_value = slot->value;
    return true;
}

void hmap_delete(hmap_t* table, uint64_t key) {
    hmap_slot_t* slot = hmap_find_existing_slot(table, key);
    if (slot) {
        slot->state = SH_SLOT_TOMBSTONE;
        table->size--;
        if (hmap_should_shrink(table)) {
            // Best-effort shrink: if this fails, we'll be stuck with an
            // oversized table/too many tombstones, but everything will still
            // work.
            (void) hmap_rehash(table, table->cap_order - 1);
        }
    }
}

void hmap_iter(const hmap_t* table, hmap_iter_t* iter) {
    *iter = (hmap_iter_t){.table = table, .i = 0};
}

bool hmap_iter_next(hmap_iter_t* iter, uint64_t* key, uint64_t* value) {
    if (iter->table->slots == nullptr) {
        return false;
    }

    size_t capacity = hmap_capacity(iter->table);
    while (iter->i < capacity) {
        hmap_slot_t* slot = &iter->table->slots[iter->i++];
        if (slot->state == SH_SLOT_USED) {
            *key = slot->key;
            *value = slot->value;
            return true;
        }
    }

    return false;
}
