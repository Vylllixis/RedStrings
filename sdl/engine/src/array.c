#include "engine/array.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>

#include "block.h"
#include "engine/log.h"

#define ARRAY_MIN_CAPACITY 8

bool eng_array_reserve_raw(EngArrayRaw *raw, size_t elem_size, size_t min_capacity)
{
    if (elem_size == 0) {
        return false;
    }
    if (min_capacity <= raw->capacity) {
        return true;
    }

    size_t new_capacity = raw->capacity != 0 ? raw->capacity : ARRAY_MIN_CAPACITY;
    while (new_capacity < min_capacity) {
        if (new_capacity > SIZE_MAX / 2) {
            return false;
        }
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / elem_size) {
        ENG_LOG_ERROR("tableau : %zu éléments de %zu octets débordent de size_t", new_capacity,
                      elem_size);
        return false;
    }
    size_t bytes = new_capacity * elem_size;

    void *block;
    if (raw->arena != NULL) {
        block = eng_arena_alloc(raw->arena, bytes, ENG_BLOCK_ALIGN);
        if (block == NULL) {
            return false;
        }
        if (raw->count != 0) {
            memcpy(block, raw->data, raw->count * elem_size);
        }
    } else {
        block = SDL_realloc(raw->data, bytes);
        if (block == NULL) {
            ENG_LOG_ERROR("tableau : impossible de réserver %zu octets", bytes);
            return false; /* l'ancien bloc reste valide */
        }
    }
    raw->data = block;
    raw->capacity = new_capacity;
    return true;
}

void eng_array_free_raw(EngArrayRaw *raw)
{
    eng_block_free(raw->arena, raw->data);
    raw->data = NULL;
    raw->count = 0;
    raw->capacity = 0;
}
