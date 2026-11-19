#ifndef ENG_UPLOAD_QUEUE_H
#define ENG_UPLOAD_QUEUE_H

/* Uploads GPU étalés (M7-04) : les cellules prêtes ne sont pas toutes envoyées au GPU dans la même
 * image. Chaque image prend, des plus proches aux plus lointaines, des demandes jusqu'à
 * `budget_bytes` ; une demande plus grosse que le budget passe seule (sinon elle attendrait
 * toujours). Le module ne fait que l'ordonnancement : l'appelant fait l'upload réel des
 * identifiants retournés. */

#include <stdbool.h>
#include <stdint.h>

#define ENG_UPLOAD_MAX 256U

typedef struct EngUploadReq {
    uint64_t id;
    uint32_t bytes;
    float distance;
    uint32_t age; /* images d'attente */
} EngUploadReq;

typedef struct EngUploadQueue {
    EngUploadReq req[ENG_UPLOAD_MAX];
    uint32_t count;
    uint32_t budget_bytes;
    uint32_t max_frame_bytes; /* plus gros total d'une image (mesure) */
    uint64_t sent;
    uint64_t sent_bytes;
    uint32_t oldest_wait; /* plus longue attente observée, en images */
} EngUploadQueue;

void eng_upq_init(EngUploadQueue *q, uint32_t budget_bytes);
bool eng_upq_push(EngUploadQueue *q, uint64_t id, uint32_t bytes, float distance);
bool eng_upq_cancel(EngUploadQueue *q, uint64_t id); /* cellule déchargée avant l'upload */
bool eng_upq_set_distance(EngUploadQueue *q, uint64_t id, float distance);

/* Une image : écrit au plus `max` identifiants à envoyer maintenant, retourne leur nombre. */
uint32_t eng_upq_take(EngUploadQueue *q, uint64_t *out_ids, uint32_t max);

#endif
