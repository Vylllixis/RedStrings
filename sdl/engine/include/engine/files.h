#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Accès aux fichiers : chemins relatifs à l'EXÉCUTABLE (et non au dossier où on lance le
 * programme), lecture complète dans une arène, surveillance de modification. */

/* ── Chemins ── */

/* Dossier de l'exécutable, séparateur final inclus. false si SDL ne le connaît pas ou si
 * `out` est trop petit. */
bool eng_path_base(char *out, size_t out_size);

/* Chemin relatif à l'exécutable : « config.ini » -> « <dossier de l'exécutable>/config.ini ».
 * Un chemin déjà absolu est recopié tel quel. false si le résultat ne tient pas dans `out`. */
bool eng_path_resolve(char *out, size_t out_size, const char *relative);

bool eng_path_is_absolute(const char *path);

/* ── Lecture ── */

typedef struct EngFileData {
    const uint8_t *data; /* terminé par un octet nul (data[size] == 0) : utilisable comme texte */
    size_t size;         /* taille réelle, sans l'octet nul */
} EngFileData;

#define ENG_FILE_MAX_SIZE ((size_t)256 * 1024 * 1024)

/* Vrai si `path` désigne un fichier ordinaire existant. */
bool eng_file_exists(const char *path);

/* Lit tout le fichier dans `arena`. Les octets restent valides jusqu'au reset de l'arène.
 * Échec (false, `out` mis à zéro, arène inchangée) : fichier absent, pas un fichier, plus grand
 * que ENG_FILE_MAX_SIZE, arène trop petite, erreur de lecture. Un fichier ABSENT n'est pas
 * journalisé (c'est un cas normal : configuration facultative) ; les autres échecs le sont. */
bool eng_file_read_all(EngArena *arena, const char *path, EngFileData *out);

/* ── Surveillance ── */

#define ENG_FILE_WATCH_DEFAULT_INTERVAL 0.25

/* Détecte qu'un fichier a changé (date de modification ou taille). Une modification n'est
 * signalée qu'une fois le fichier STABLE (inchangé entre deux contrôles consécutifs), pour ne
 * pas lire un fichier qu'un éditeur ou un compilateur est encore en train d'écrire. Délai de
 * détection : au plus 2 intervalles de contrôle (0,5 s par défaut).
 *
 * Limite : une réécriture qui conserve exactement la même taille ET la même date de
 * modification n'est pas détectable (dates à la seconde sur certains systèmes de fichiers). */
typedef struct EngFileWatch {
    char path[512];
    double interval;
    double next_poll;
    int64_t base_mtime; /* dernière version signalée (ou présente au démarrage) */
    int64_t base_size;  /* -1 : le fichier n'existait pas */
    int64_t seen_mtime; /* version vue au contrôle précédent, en attente de stabilité */
    int64_t seen_size;
    bool pending;
} EngFileWatch;

/* La version présente à ce moment est la référence : elle ne sera pas signalée. Un fichier
 * absent est une référence valide : son apparition sera signalée. */
bool eng_file_watch_init(EngFileWatch *watch, const char *path, double interval_seconds);

/* À appeler souvent (une fois par image convient) avec un temps croissant en secondes ; le
 * fichier n'est réellement consulté qu'à chaque intervalle. Vrai UNE fois par modification. */
bool eng_file_watch_poll(EngFileWatch *watch, double now_seconds);
