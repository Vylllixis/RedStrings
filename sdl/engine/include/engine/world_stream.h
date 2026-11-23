#ifndef ENG_WORLD_STREAM_H
#define ENG_WORLD_STREAM_H

/* Streaming asynchrone des cellules (M7-03).
 *
 * Le monde autour du joueur = les cellules à au plus `load_radius` cellules (carré de Chebyshev).
 * Chaque appel à `eng_stream_update` (thread principal, une fois par image) :
 *   1. récolte les cellules dont le job est fini (état Ready) ;
 *   2. libère celles qui sont au-delà de `load_radius + 1` (hystérésis d'une cellule : un aller-
 *      retour sur une frontière ne recharge rien) ;
 *   3. lance au plus `max_requests` nouveaux chargements, les plus proches d'abord.
 * Le chargement (lecture de `dir/cell_X_Z.rscl` si le dossier est donné et le fichier existe,
 * sinon génération procédurale) tourne dans un job : le thread principal ne fait que de la
 * comptabilité. Mémoire BORNÉE : ENG_STREAM_SLOTS emplacements préalloués, aucun malloc. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/jobs.h"
#include "engine/world_cell.h"

#define ENG_STREAM_SLOTS 64U

typedef enum EngSlotState {
    ENG_SLOT_FREE = 0,
    ENG_SLOT_LOADING,
    ENG_SLOT_READY,
    ENG_SLOT_RELEASING /* hors de portée pendant son chargement : libéré à la fin du job */
} EngSlotState;

struct EngWorldStream;

typedef struct EngStreamSlot {
    EngSlotState state;
    EngCellCoord coord;
    EngCellData data;
    EngJobCounter done;
    bool from_file;
    bool failed;
    const struct EngWorldStream *owner;
} EngStreamSlot;

/* Appelées sur le thread principal pendant `eng_stream_update`. */
typedef void (*EngStreamCellFn)(void *user, const EngCellData *cell);

typedef struct EngWorldStream {
    EngWorldGen gen;
    EngJobs *jobs;
    const char *dir; /* NULL : génération seulement */
    int32_t load_radius;
    uint32_t max_requests; /* nouveaux chargements par mise à jour */
    EngStreamSlot slots[ENG_STREAM_SLOTS];
    EngStreamCellFn on_ready;
    EngStreamCellFn on_unload;
    void *user;
    EngCellCoord center;
    /* mesures */
    uint64_t loads_started;
    uint64_t loads_done;
    uint64_t files_read;
    uint64_t unloads;
    uint32_t loaded; /* cellules prêtes */
    uint32_t in_flight;
    uint32_t peak_slots; /* plus grand nombre d'emplacements occupés */
    uint64_t refused;    /* demandes refusées faute d'emplacement */
} EngWorldStream;

/* `load_radius` : 1 à 3 ((2r+1)² cellules gardées, au plus 49 + l'hystérésis). */
bool eng_stream_init(EngWorldStream *s, const EngWorldGen *gen, EngJobs *jobs, int32_t load_radius,
                     const char *dir);

/* Attend la fin de tous les jobs en cours puis libère tout (appelle `on_unload`). */
void eng_stream_shutdown(EngWorldStream *s);

/* Une image : voir l'en-tête. Retourne le nombre de cellules devenues prêtes. */
uint32_t eng_stream_update(EngWorldStream *s, double x, double z);

/* Cellule prête ou NULL. */
const EngCellData *eng_stream_cell(const EngWorldStream *s, EngCellCoord c);

/* Toutes les cellules à au plus `radius` de (x, z) sont prêtes ? */
bool eng_stream_ready_around(const EngWorldStream *s, double x, double z, int32_t radius);

/* Hauteur du sol au point monde (x, z) si sa cellule est prête ; false sinon. */
bool eng_stream_height(const EngWorldStream *s, double x, double z, float *out);

/* Octets réservés par le streaming (budget mémoire, fixe). */
size_t eng_stream_memory(void);

#endif
