#include "engine/map.h"

#include <string.h>

#include "block.h"
#include "engine/assert.h"
#include "engine/log.h"

#define MAP_MIN_CAPACITY 16

/* Finaliseur de splitmix64 : répartit bien même des clés consécutives (0, 1, 2...). */
static uint64_t mix(uint64_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

uint64_t eng_hash_string(const char *text)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        hash ^= *p;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

/* Remplit keys/values/used à partir d'un bloc neuf pour la capacité donnée. */
static bool alloc_tables(EngMap *map, size_t capacity)
{
    if (map->value_size > (SIZE_MAX / 4) / capacity) {
        ENG_LOG_ERROR("table : %zu entrées de %zu octets débordent de size_t", capacity,
                      map->value_size);
        return false;
    }
    size_t values_off = eng_align_up(capacity * sizeof(uint64_t), ENG_BLOCK_ALIGN);
    size_t used_off = values_off + capacity * map->value_size;
    size_t total = used_off + capacity;

    uint8_t *block = eng_block_alloc(map->arena, total);
    if (block == NULL) {
        ENG_LOG_ERROR("table : impossible de réserver %zu octets", total);
        return false;
    }
    map->keys = (uint64_t *)block;
    map->values = block + values_off;
    map->used = block + used_off;
    map->capacity = capacity;
    return true;
}

static size_t capacity_for(size_t count)
{
    size_t capacity = MAP_MIN_CAPACITY;
    /* charge max 70 % : count <= capacity * 7 / 10 */
    while (capacity / 10 * 7 < count) {
        capacity *= 2;
    }
    return capacity;
}

bool eng_map_init(EngMap *map, EngArena *arena, size_t value_size, size_t initial_count)
{
    *map = (EngMap){0};
    if (value_size == 0 || initial_count > SIZE_MAX / 64) {
        ENG_LOG_ERROR("table : paramètres invalides (taille de valeur %zu)", value_size);
        return false;
    }
    map->value_size = value_size;
    map->arena = arena;
    if (!alloc_tables(map, capacity_for(initial_count))) {
        *map = (EngMap){0};
        return false;
    }
    return true;
}

void eng_map_shutdown(EngMap *map)
{
    eng_block_free(map->arena, map->keys); /* le bloc commence à `keys` */
    *map = (EngMap){0};
}

/* Indice de la clé, ou de la première case libre rencontrée si elle est absente. */
static size_t probe(const EngMap *map, uint64_t key, bool *found)
{
    size_t mask = map->capacity - 1;
    size_t i = (size_t)mix(key) & mask;
    while (map->used[i] != 0) {
        if (map->keys[i] == key) {
            *found = true;
            return i;
        }
        i = (i + 1) & mask;
    }
    *found = false;
    return i;
}

static bool grow(EngMap *map)
{
    EngMap bigger = *map;
    bigger.count = 0;
    if (!alloc_tables(&bigger, map->capacity * 2)) {
        return false;
    }
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->used[i] != 0) {
            bool found;
            size_t at = probe(&bigger, map->keys[i], &found);
            bigger.keys[at] = map->keys[i];
            bigger.used[at] = 1;
            memcpy(bigger.values + at * map->value_size, map->values + i * map->value_size,
                   map->value_size);
        }
    }
    bigger.count = map->count;
    eng_block_free(map->arena, map->keys);
    *map = bigger;
    return true;
}

bool eng_map_put(EngMap *map, uint64_t key, const void *value)
{
    bool found;
    size_t at = probe(map, key, &found);
    if (!found) {
        if ((map->count + 1) * 10 > map->capacity * 7) {
            if (!grow(map)) {
                return false;
            }
            at = probe(map, key, &found);
        }
        map->keys[at] = key;
        map->used[at] = 1;
        map->count++;
    }
    memcpy(map->values + at * map->value_size, value, map->value_size);
    return true;
}

void *eng_map_get(const EngMap *map, uint64_t key)
{
    bool found;
    size_t at = probe(map, key, &found);
    return found ? map->values + at * map->value_size : NULL;
}

bool eng_map_contains(const EngMap *map, uint64_t key)
{
    return eng_map_get(map, key) != NULL;
}

bool eng_map_remove(EngMap *map, uint64_t key)
{
    bool found;
    size_t hole = probe(map, key, &found);
    if (!found) {
        return false;
    }

    /* Suppression par décalage arrière : on comble le trou avec le premier élément suivant
     * qui n'est pas à sa place idéale « après » le trou, jusqu'à la première case libre. */
    size_t mask = map->capacity - 1;
    size_t j = hole;
    for (;;) {
        j = (j + 1) & mask;
        if (map->used[j] == 0) {
            break;
        }
        size_t ideal = (size_t)mix(map->keys[j]) & mask;
        bool movable = (hole <= j) ? (ideal <= hole || ideal > j) : (ideal <= hole && ideal > j);
        if (movable) {
            map->keys[hole] = map->keys[j];
            memcpy(map->values + hole * map->value_size, map->values + j * map->value_size,
                   map->value_size);
            hole = j;
        }
    }
    map->used[hole] = 0;
    map->count--;
    return true;
}

void eng_map_clear(EngMap *map)
{
    memset(map->used, 0, map->capacity);
    map->count = 0;
}

size_t eng_map_count(const EngMap *map)
{
    return map->count;
}

bool eng_map_next(const EngMap *map, size_t *cursor, uint64_t *out_key, void **out_value)
{
    for (size_t i = *cursor; i < map->capacity; i++) {
        if (map->used[i] != 0) {
            *cursor = i + 1;
            *out_key = map->keys[i];
            *out_value = map->values + i * map->value_size;
            return true;
        }
    }
    *cursor = map->capacity;
    return false;
}
