#include "engine/pool.h"

#include <string.h>

#include "block.h"
#include "engine/log.h"

#define POOL_NO_FREE UINT32_MAX

bool eng_pool_init(EngPool *pool, EngArena *arena, size_t item_size, uint32_t capacity)
{
    *pool = (EngPool){0};
    if (item_size == 0 || capacity == 0 || capacity >= POOL_NO_FREE) {
        ENG_LOG_ERROR("pool : paramètres invalides (taille %zu, capacité %u)", item_size,
                      (unsigned)capacity);
        return false;
    }

    size_t stride = eng_align_up(item_size, 8);
    if (stride > (SIZE_MAX - 4096) / capacity) {
        ENG_LOG_ERROR("pool : %u objets de %zu octets débordent de size_t", (unsigned)capacity,
                      item_size);
        return false;
    }

    /* Un seul bloc : [generations][next_free][alive][remplissage][objets] */
    size_t gen_bytes = (size_t)capacity * sizeof(uint32_t);
    size_t free_off = gen_bytes;
    size_t alive_off = free_off + gen_bytes;
    size_t items_off = eng_align_up(alive_off + capacity, ENG_BLOCK_ALIGN);
    size_t total = items_off + stride * capacity;

    uint8_t *block = eng_block_alloc(arena, total);
    if (block == NULL) {
        ENG_LOG_ERROR("pool : impossible de réserver %zu octets", total);
        return false;
    }

    pool->generations = (uint32_t *)block;
    pool->next_free = (uint32_t *)(block + free_off);
    pool->alive = block + alive_off;
    pool->items = block + items_off;
    pool->item_size = item_size;
    pool->item_stride = stride;
    pool->capacity = capacity;
    pool->arena = arena;

    for (uint32_t i = 0; i < capacity; i++) {
        pool->generations[i] = 1;
        pool->next_free[i] = (i + 1 < capacity) ? i + 1 : POOL_NO_FREE;
    }
    pool->free_head = 0;
    return true;
}

void eng_pool_shutdown(EngPool *pool)
{
    /* Le bloc commence à `generations` (premier élément de la disposition). */
    eng_block_free(pool->arena, pool->generations);
    *pool = (EngPool){0};
}

static uint8_t *slot(const EngPool *pool, uint32_t index)
{
    return pool->items + (size_t)index * pool->item_stride;
}

void *eng_pool_alloc(EngPool *pool, EngHandle *out_handle)
{
    if (pool->free_head == POOL_NO_FREE) {
        return NULL;
    }
    uint32_t index = pool->free_head;
    pool->free_head = pool->next_free[index];
    pool->alive[index] = 1;
    pool->count++;

    uint8_t *item = slot(pool, index);
    memset(item, 0, pool->item_size);
    if (out_handle != NULL) {
        *out_handle = (EngHandle){index, pool->generations[index]};
    }
    return item;
}

bool eng_pool_is_valid(const EngPool *pool, EngHandle handle)
{
    return handle.generation != 0 && handle.index < pool->capacity &&
           pool->alive[handle.index] != 0 && pool->generations[handle.index] == handle.generation;
}

void *eng_pool_get(const EngPool *pool, EngHandle handle)
{
    return eng_pool_is_valid(pool, handle) ? slot(pool, handle.index) : NULL;
}

bool eng_pool_free(EngPool *pool, EngHandle handle)
{
    if (!eng_pool_is_valid(pool, handle)) {
        ENG_LOG_WARN("pool : libération d'un handle invalide ou périmé (index %u, génération %u)",
                     (unsigned)handle.index, (unsigned)handle.generation);
        return false;
    }
    pool->alive[handle.index] = 0;
    if (++pool->generations[handle.index] == 0) {
        pool->generations[handle.index] = 1; /* la génération 0 est réservée au handle nul */
    }
    pool->next_free[handle.index] = pool->free_head;
    pool->free_head = handle.index;
    pool->count--;
    return true;
}

uint32_t eng_pool_count(const EngPool *pool)
{
    return pool->count;
}

bool eng_pool_next(const EngPool *pool, uint32_t *cursor, EngHandle *out_handle)
{
    for (uint32_t i = *cursor; i < pool->capacity; i++) {
        if (pool->alive[i] != 0) {
            *cursor = i + 1;
            *out_handle = (EngHandle){i, pool->generations[i]};
            return true;
        }
    }
    *cursor = pool->capacity;
    return false;
}
