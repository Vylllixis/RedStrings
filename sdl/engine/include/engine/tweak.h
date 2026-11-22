#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Variables réglables en direct (M1-12) : le code enregistre l'ADRESSE d'une de ses variables avec
 * un nom et des bornes ; l'interface de debug (ou un test, ou plus tard une console) la modifie
 * pendant l'exécution. Le registre ne copie rien : il écrit directement dans la variable, qui
 * reste donc la seule source de vérité. Toute modification passe par eng_tweaks_set_* : valeur
 * bornée, NaN refusé.
 *
 * Pas d'allocation : table de taille fixe, noms non copiés (littéraux, valides toute l'exécution).
 * Une variable enregistrée doit vivre au moins aussi longtemps que le registre (ou être retirée
 * avec eng_tweaks_clear avant sa destruction). */

#define ENG_TWEAK_MAX 64U

typedef enum EngTweakType {
    ENG_TWEAK_FLOAT,
    ENG_TWEAK_INT,
    ENG_TWEAK_BOOL,
} EngTweakType;

typedef struct EngTweak {
    const char *name;
    EngTweakType type;
    void *value; /* float*, int32_t* ou bool* selon `type` */
    float min;   /* bornes (FLOAT, INT) ; ignorées pour BOOL */
    float max;
    float initial; /* valeur à l'enregistrement : cible de « réinitialiser » (bool : 0 ou 1) */
} EngTweak;

typedef struct EngTweaks {
    EngTweak items[ENG_TWEAK_MAX];
    uint32_t count;
} EngTweaks;

void eng_tweaks_init(EngTweaks *t);
/* Retire tout (à faire avant de détruire les variables enregistrées). */
void eng_tweaks_clear(EngTweaks *t);

/* Enregistrent une variable. false (rien d'enregistré) si : adresse ou nom nul, nom déjà pris,
 * table pleine, bornes invalides (min > max, NaN, infini) ou valeur actuelle hors bornes. */
bool eng_tweaks_add_float(EngTweaks *t, const char *name, float *value, float min, float max);
bool eng_tweaks_add_int(EngTweaks *t, const char *name, int32_t *value, int32_t min, int32_t max);
bool eng_tweaks_add_bool(EngTweaks *t, const char *name, bool *value);

/* Recherche par nom ; NULL si absent. */
const EngTweak *eng_tweaks_find(const EngTweaks *t, const char *name);

/* Modifient la variable nommée, bornée à [min, max]. false si le nom est inconnu, le type ne
 * correspond pas, ou la valeur est NaN (la variable n'est alors pas modifiée). */
bool eng_tweaks_set_float(EngTweaks *t, const char *name, float v);
bool eng_tweaks_set_int(EngTweaks *t, const char *name, int32_t v);
bool eng_tweaks_set_bool(EngTweaks *t, const char *name, bool v);

/* Lecture. Retournent false (et laissent `out` intact) si le nom est inconnu ou le type diffère. */
bool eng_tweaks_get_float(const EngTweaks *t, const char *name, float *out);
bool eng_tweaks_get_int(const EngTweaks *t, const char *name, int32_t *out);
bool eng_tweaks_get_bool(const EngTweaks *t, const char *name, bool *out);

/* Remet la variable (ou toutes) à sa valeur d'enregistrement. false si l'indice est invalide. */
bool eng_tweaks_reset(EngTweaks *t, uint32_t index);
void eng_tweaks_reset_all(EngTweaks *t);
