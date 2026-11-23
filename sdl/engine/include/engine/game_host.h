#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/game_api.h"
#include "engine/memory.h"
#include "engine/profile.h"

/* Hôte du jeu : charge la bibliothèque du jeu, surveille sa modification et la recharge à
 * chaud en conservant l'état (voir les règles dans game_api.h).
 *
 * Déroulement d'un rechargement :
 *  - le fichier de la bibliothèque change (mtime ou taille) ;
 *  - on attend qu'il soit stable sur deux vérifications consécutives (l'éditeur de liens peut
 *    encore être en train de l'écrire) ;
 *  - on le COPIE sous un nom neuf dans <dossier>/hot/ et on charge la copie : l'original reste
 *    libre, le compilateur peut le réécrire (sous Windows, une DLL chargée est verrouillée) ;
 *  - on vérifie la version de l'API et la taille de l'état ; sinon le rechargement est refusé et
 *    l'ancien code continue ;
 *  - ancien.unload(état), nouveau.reload(état, services), puis l'ancienne copie est déchargée
 *    et supprimée. */

/* Vrai quand dlclose() ne décharge PAS réellement la bibliothèque. Tracy le neutralise sous
 * Linux (TracyOverride.cpp, documenté) : une bibliothèque rechargée n'est alors jamais libérée.
 * Deux conséquences traitées par l'hôte :
 *  - un nom de copie n'est jamais réutilisé dans le processus (dlopen renverrait l'ancienne) ;
 *  - une copie n'est pas supprimée tant que le processus tourne : glibc reconnaît une
 *    bibliothèque déjà chargée par son numéro d'inode, et un fichier neuf pourrait récupérer le
 *    numéro d'un fichier supprimé encore mappé (l'ancien code serait rechargé en silence).
 * Les copies sont supprimées à l'arrêt. Coût : quelques dizaines de Ko par rechargement,
 * uniquement dans un build de profilage. */
#if ENG_PROF_ENABLED && defined(__linux__)
#define ENG_HOST_DLCLOSE_IS_NOOP 1
#else
#define ENG_HOST_DLCLOSE_IS_NOOP 0
#endif

typedef struct EngGameHost {
    char source_path[512];
    char hot_dir[512];
    const EngServices *services;
    EngArena *state_arena;

    void *library; /* SDL_SharedObject* (masqué pour ne pas exposer SDL) */
    const EngGameApi *api;
    char loaded_copy[512];
    void *state;
    size_t state_size;

    int64_t loaded_mtime; /* version du fichier actuellement chargée */
    int64_t loaded_size;
    int64_t seen_mtime; /* version vue au dernier contrôle, en attente de stabilité */
    int64_t seen_size;
    bool pending;
    int64_t rejected_mtime; /* version refusée : on ne la réessaie pas */
    int64_t rejected_size;

    double poll_interval; /* secondes entre deux contrôles du fichier */
    double next_poll;
    uint32_t generation;    /* nombre de copies créées par cet hôte */
    uint32_t reload_count;  /* rechargements réussis */
    uint32_t refused_count; /* rechargements refusés (taille d'état, bibliothèque invalide...) */
} EngGameHost;

/* Remplit la table de services à partir des arènes du moteur. */
void eng_game_services_init(EngServices *services, EngMemory *memory);

/* Chemin par défaut : « game » + extension des bibliothèques, à côté de l'exécutable. */
bool eng_game_default_path(char *out, size_t out_size);

/* Charge le jeu, alloue son état dans `state_arena` et appelle api->init. */
bool eng_game_host_init(EngGameHost *host, const char *library_path, const EngServices *services,
                        EngArena *state_arena, double poll_interval_seconds);

/* À appeler une fois par image (le coût réel n'est payé qu'à chaque intervalle). `now` est un
 * temps croissant en secondes. Retourne true si le jeu vient d'être rechargé. */
bool eng_game_host_poll(EngGameHost *host, double now_seconds);

void eng_game_host_shutdown(EngGameHost *host);
