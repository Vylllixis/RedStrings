#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Table de hachage à adressage ouvert (sondage linéaire, suppression par décalage arrière : pas
 * de « pierres tombales »). Clé : entier 64 bits (pour une chaîne, passer par eng_hash_string).
 * Valeur : `value_size` octets, copiés dans la table.
 *
 * La table grandit toute seule (facteur de charge max 70 %). Mêmes règles mémoire que
 * ENG_ARRAY : arena == NULL -> tas ; sinon chaque agrandissement prend un nouveau bloc dans
 * l'arène (l'ancien reste perdu jusqu'au reset). Si le nombre d'entrées est connu, passer
 * `initial_count` à eng_map_init pour éviter tout agrandissement.
 *
 * Un pointeur rendu par eng_map_get est invalidé par la prochaine insertion ou suppression. */

typedef struct EngMap {
    uint64_t *keys;
    uint8_t *values;
    uint8_t *used;
    size_t value_size;
    size_t capacity; /* puissance de deux */
    size_t count;
    EngArena *arena;
} EngMap;

bool eng_map_init(EngMap *map, EngArena *arena, size_t value_size, size_t initial_count);
void eng_map_shutdown(EngMap *map);

/* Insère ou remplace. false si la mémoire manque (la table reste intacte). */
bool eng_map_put(EngMap *map, uint64_t key, const void *value);
/* NULL si la clé est absente. */
void *eng_map_get(const EngMap *map, uint64_t key);
bool eng_map_contains(const EngMap *map, uint64_t key);
/* false si la clé est absente. */
bool eng_map_remove(EngMap *map, uint64_t key);
void eng_map_clear(EngMap *map);
size_t eng_map_count(const EngMap *map);

/* Itération dans un ordre quelconque, sans insérer ni supprimer pendant le parcours :
 *     size_t cursor = 0; uint64_t key; void *value;
 *     while (eng_map_next(&map, &cursor, &key, &value)) {...} */
bool eng_map_next(const EngMap *map, size_t *cursor, uint64_t *out_key, void **out_value);

/* FNV-1a 64 bits d'une chaîne terminée par un zéro. */
uint64_t eng_hash_string(const char *text);
