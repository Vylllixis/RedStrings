#ifndef ENG_PROFILE_H
#define ENG_PROFILE_H

/* Profilage (Tracy, API C). Compilé seulement avec -DOW_PROFILE=ON ; sinon toutes les macros
 * se réduisent à rien : aucun coût, aucune dépendance, aucun téléchargement.
 *
 * Usage :
 *     ENG_PROF_ZONE(z, "simulation");   // ouvre une zone ; `z` est un nom de variable local
 *     ...
 *     ENG_PROF_ZONE_END(z);             // la ferme (une fois, dans la même fonction)
 *     ENG_PROF_FRAME();                 // fin d'image : une fois par tour de boucle
 *
 * Règles :
 *  - le nom est un LITTÉRAL de chaîne (Tracy garde le pointeur, il ne copie pas) ;
 *  - chaque zone ouverte est fermée sur TOUS les chemins de sortie (return, goto, break) ;
 *  - deux zones ouvertes dans la même portée ont deux noms de variable différents ;
 *  - le code du jeu (game/) ne lie pas le moteur : ses zones passeront par la table de
 *    fonctions d'EngServices, pas encore prévue (voir docs/architecture.md). */

#if defined(OW_PROFILE)
#include <tracy/TracyC.h>

#define ENG_PROF_ENABLED 1
#define ENG_PROF_ZONE(var, name) TracyCZoneN(var, name, 1)
#define ENG_PROF_ZONE_END(var) TracyCZoneEnd(var)
#define ENG_PROF_FRAME() TracyCFrameMark
#define ENG_PROF_THREAD_NAME(name) TracyCSetThreadName(name)
#define ENG_PROF_PLOT(name, value) TracyCPlot(name, (double)(value))
#else
#define ENG_PROF_ENABLED 0
#define ENG_PROF_ZONE(var, name) ((void)0)
#define ENG_PROF_ZONE_END(var) ((void)0)
#define ENG_PROF_FRAME() ((void)0)
#define ENG_PROF_THREAD_NAME(name) ((void)0)
#define ENG_PROF_PLOT(name, value) ((void)0)
#endif

#endif
