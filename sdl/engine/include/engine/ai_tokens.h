#ifndef ENG_AI_TOKENS_H
#define ENG_AI_TOKENS_H

/* Coordination de groupe (M6-07) : jetons d'attaque (au plus `max` attaquants en même temps) et
 * encerclement (positions réparties autour du joueur pour les ennemis sans jeton). */

#include <stdbool.h>
#include <stdint.h>

#define ENG_AI_TOKEN_SLOTS 8U
#define ENG_AI_NO_HOLDER UINT32_MAX

typedef struct EngAiTokens {
    uint32_t max;                        /* jetons en circulation (2) */
    uint32_t holder[ENG_AI_TOKEN_SLOTS]; /* identifiant du porteur ou ENG_AI_NO_HOLDER */
} EngAiTokens;

void eng_tokens_init(EngAiTokens *t, uint32_t max);
bool eng_tokens_has(const EngAiTokens *t, uint32_t id);
uint32_t eng_tokens_in_use(const EngAiTokens *t);
void eng_tokens_release(EngAiTokens *t, uint32_t id);

/* Attribution : les porteurs actuels gardent leur jeton ; les jetons libres vont aux demandeurs
 * les plus proches (égalité : plus petit identifiant). `ids[k]` demande, `dist[k]` = distance au
 * joueur. Ne dépasse jamais `max`. */
void eng_tokens_arbitrate(EngAiTokens *t, const uint32_t *ids, const float *dist, uint32_t n);

/* Positions d'encerclement : `n` ennemis autour de `center` à `radius`, répartis également ;
 * chaque ennemi reçoit le créneau le plus proche de son angle actuel (sans doublon). */
void eng_ai_ring(const float center[2], const float (*pos)[2], uint32_t n, float radius,
                 float (*out)[2]);

#endif
