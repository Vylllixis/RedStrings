#pragma once

#include <stdbool.h>

#include "engine/tweak.h"

/* Sprint et endurance (M4-09). Module pur : « l'envie de courir » et « on bouge » entrent, « on
 * court réellement » sort, et l'endurance baisse en courant, puis se régénère après un délai.
 *
 *     EngStamina st;  eng_stamina_init(&st);
 *     par pas :  bool sprinting;
 *                eng_stamina_update(&st, eng_action_held(&in, ENG_ACTION_SPRINT), moving, dt,
 * &sprinting); eng_locomotion_update(..., sprinting ? ENG_STAMINA_SPRINT_SCALE : 1.0F, ...);
 *
 * Règles :
 *  - on ne court que si l'on veut courir, que l'on se déplace et que l'on n'est pas épuisé ;
 *  - courir consomme `drain` points par seconde ; à 0 on est ÉPUISÉ : la course s'arrête et ne
 *    reprend (touche relâchée ou non) que lorsque l'endurance a regagné `resume_at` points ;
 *  - la régénération (`regen` points par seconde) démarre `regen_delay` secondes après le dernier
 *    pas couru, et pas pendant la course.
 * Les points vont de 0 à ENG_STAMINA_MAX (100), pour s'afficher tels quels dans le panneau de
 * debug (eng_stamina_register_tweaks). */

#define ENG_STAMINA_MAX 100.0F
#define ENG_STAMINA_SPRINT_SCALE 1.6F

typedef struct EngStaminaParams {
    float drain;       /* points/s en courant, > 0 */
    float regen;       /* points/s au repos, > 0 */
    float regen_delay; /* s sans courir avant la régénération, >= 0 */
    float resume_at;   /* points à regagner après épuisement, dans [0 ; MAX] */
} EngStaminaParams;

typedef struct EngStamina {
    EngStaminaParams params;
    float value; /* [0 ; ENG_STAMINA_MAX] */
    float delay_left;
    bool exhausted;
} EngStamina;

/* Pleine ; défauts : 20 points/s (5 s de course), régénération 25 points/s après 1 s, reprise à 25.
 */
void eng_stamina_init(EngStamina *stamina);
/* Refusé (inchangé) si une valeur est non finie ou hors bornes. */
bool eng_stamina_set_params(EngStamina *stamina, const EngStaminaParams *params);

/* Un pas de `dt` secondes (0 < dt <= 0,1). `sprinting` (peut être NULL) : on court CE pas. false,
 * rien de modifié, si un argument est invalide. */
bool eng_stamina_update(EngStamina *stamina, bool want_sprint, bool moving, float dt,
                        bool *sprinting);

/* Endurance dans [0 ; 1]. */
float eng_stamina_fraction(const EngStamina *stamina);

/* Remplit l'endurance (potion, repos). Borné à ENG_STAMINA_MAX ; lève l'épuisement s'il y a lieu.
 */
void eng_stamina_restore(EngStamina *stamina);

/* Expose « endurance » (0 à 100, modifiable) dans le panneau de debug. false si le registre refuse
 * (nom pris, table pleine). La variable enregistrée est stamina->value : elle doit vivre aussi
 * longtemps que le registre. */
bool eng_stamina_register_tweaks(EngStamina *stamina, EngTweaks *tweaks);
