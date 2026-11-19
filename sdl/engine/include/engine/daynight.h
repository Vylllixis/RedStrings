#ifndef ENG_DAYNIGHT_H
#define ENG_DAYNIGHT_H

/* Cycle jour / nuit (M8-07) : une horloge de jeu qui pilote le soleil.
 *
 * L'heure (0 à 24) avance de 24 h en `day_seconds` secondes réelles (20 min par défaut, critère
 * de M8-07 : 1 h de jeu = 50 s). Le soleil se lève à l'est (+X) à 6 h, culmine au sud (-Z) à midi à
 * `max_elevation_deg`, se couche à l'ouest (-X) à 18 h et passe sous l'horizon la nuit (élévation
 * négative, symétrique). Le ciel atmosphérique et la couleur de la lumière suivent déjà la hauteur
 * du soleil (engine/atmosphere.h) ; ce module ajoute ce qu'il faut pour que la nuit reste jouable :
 *   - un « clair de lune » : lumière directionnelle faible et bleutée, opposée au soleil, dont le
 *     poids `night` (0 le jour, 1 la nuit) suit la hauteur du soleil entre +2° et -10° ;
 *   - un biais d'exposition (0 EV le jour, `night_exposure_ev` la nuit), lissé de même.
 * Tout est en fonctions pures (testées sans GPU). Ce n'est pas un modèle astronomique : pas de
 * latitude, pas de saisons, pas de phases de lune. */

#include <stdbool.h>
#include <stddef.h>

#define ENG_DAYNIGHT_DAY_SECONDS 1200.0F /* une journée complète en 20 min réelles */

typedef struct EngDayNight {
    double hour;             /* [0, 24[ */
    float day_seconds;       /* durée réelle d'une journée, > 0 */
    float speed;             /* multiplicateur (0 : arrêt) */
    float max_elevation_deg; /* hauteur du soleil à midi, ]0, 90] */
    float night_exposure_ev; /* biais d'exposition en pleine nuit, >= 0 */
} EngDayNight;

/* Valeurs par défaut : `start_hour` (ramenée dans [0, 24[), journée de 1200 s, vitesse 1, soleil
 * à 62° à midi, +1,5 EV la nuit. */
void eng_daynight_init(EngDayNight *d, double start_hour);

/* Avance l'horloge de `dt` secondes réelles (dt négatif, non fini ou énorme : ignoré au-delà
 * d'une journée). */
void eng_daynight_tick(EngDayNight *d, double dt);

/* Place l'heure (ramenée dans [0, 24[). Une valeur non finie est refusée (false). */
bool eng_daynight_set_hour(EngDayNight *d, double hour);

/* Angles du soleil pour l'heure courante, au format de eng_lighting_set_sun_angles. */
void eng_daynight_sun(const EngDayNight *d, float *azimuth_deg, float *elevation_deg);

/* Poids de la nuit selon la hauteur du soleil : 0 au-dessus de +2°, 1 sous -10°, lissé
 * (smoothstep) entre les deux. */
float eng_daynight_night_weight(float sun_elevation_deg);

/* Biais d'exposition (EV) pour cette hauteur de soleil. */
float eng_daynight_exposure_bias(const EngDayNight *d, float sun_elevation_deg);

/* « HH:MM » ; retourne la longueur écrite (0 si `capacity` < 6). */
size_t eng_daynight_format(const EngDayNight *d, char *out, size_t capacity);

#endif
