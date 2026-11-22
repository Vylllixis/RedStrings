#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/game_menu.h"

/* Fichiers des parties (2026-11-22) : ce que le menu « Parties » fait d'un emplacement, comme la
 * liste des mondes de Minecraft : décrire (nom, niveau, temps de jeu, lieu, date, boss vaincus),
 * renommer, dupliquer, supprimer. Le nom, le lieu et la date sont dans la section META de la
 * sauvegarde (engine/save_game.h) ; une sauvegarde plus ancienne, sans META, s'appelle
 * « Partie N » et porte la date de son fichier. */

/* Secondes depuis 1970, maintenant. */
int64_t game_slot_now(void);

/* « 22 nov. 2026 · 10:31 » (heure locale) ; chaîne vide si `seconds` <= 0. */
void game_slot_date(int64_t seconds, char *out, size_t capacity);

/* Décrit le fichier `path` de l'emplacement `index`. Absent : `out->used` faux. Illisible :
 * `unreadable`. `boss_count` : boss de la carte (0 : pas de grande carte) ; les boss vaincus sont
 * ceux des zones de boss (bits 2, 5, 8, 11 de la sauvegarde), ramenés à un bit par boss. `when`
 * reçoit la date de la sauvegarde (secondes), pour trouver la plus récente. */
void game_slot_describe(const char *path, uint32_t index, uint32_t boss_count, EngMenuSlot *out,
                        int64_t *when);

/* Donne le nom `name` à la partie de `path` : tout le reste du fichier est conservé, sa date
 * aussi (renommer ne fait pas d'une partie « la plus récente »). */
bool game_slot_rename(const char *path, const char *name);

/* Copie `from` dans `to` ; la copie s'appelle « <nom> (copie) » et porte la date de l'original
 * moins une seconde (« Continuer » reprend toujours l'original). */
bool game_slot_copy(const char *from, const char *to);

/* Retire la partie de l'emplacement. Le fichier n'est pas détruit : il est renommé en
 * « <path>.supprimee » (une seule partie supprimée est gardée par emplacement), pour qu'une
 * suppression par erreur reste rattrapable à la main. */
bool game_slot_delete(const char *path);
