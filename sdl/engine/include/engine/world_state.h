#ifndef ENG_WORLD_STATE_H
#define ENG_WORLD_STATE_H

/* Persistance du monde (M7-12) : ce que le joueur a changé dans une cellule (camps vaincus, objets
 * ramassés ou détruits) survit au déchargement de la cellule. Une cellule jamais modifiée ne coûte
 * rien. Table à adressage ouvert de taille fixe ; sauvegarde texte (une ligne par cellule). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/world_cell.h"

#define ENG_WORLD_STATE_CAP 4096U

typedef struct EngCellState {
    EngCellCoord coord;
    uint64_t removed; /* bit i : objet i de la cellule retiré (ramassé, détruit) */
    uint32_t camps_defeated;
    uint32_t revision; /* +1 à chaque modification */
} EngCellState;

typedef struct EngWorldState {
    EngCellState cells[ENG_WORLD_STATE_CAP];
    bool used[ENG_WORLD_STATE_CAP];
    uint32_t count;
} EngWorldState;

void eng_wstate_init(EngWorldState *w);

/* État de la cellule (NULL si jamais modifiée). */
const EngCellState *eng_wstate_get(const EngWorldState *w, EngCellCoord c);

/* Crée l'entrée si besoin (NULL si la table est pleine). */
EngCellState *eng_wstate_edit(EngWorldState *w, EngCellCoord c);

bool eng_wstate_remove_object(EngWorldState *w, EngCellCoord c, uint32_t object_index);
bool eng_wstate_object_removed(const EngWorldState *w, EngCellCoord c, uint32_t object_index);
bool eng_wstate_defeat_camp(EngWorldState *w, EngCellCoord c);
uint32_t eng_wstate_camps_defeated(const EngWorldState *w, EngCellCoord c);

/* Texte : « cell X Z removed_hex camps revision » par ligne. Retourne les octets écrits (0 si la
 * capacité ne suffit pas). */
size_t eng_wstate_save(const EngWorldState *w, char *out, size_t capacity);
bool eng_wstate_load(EngWorldState *w, const char *text);

#endif
