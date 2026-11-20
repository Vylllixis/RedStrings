#ifndef ENG_MAP_ZONES_H
#define ENG_MAP_ZONES_H

/* Zones de la grande carte (demande de Théo, 2026-11-20) : dans chaque biome, le long de +X, une
 * zone de farm facile, une zone de farm dure, puis une zone de boss. Les ennemis d'une zone sont
 * des archétypes du jeu (assets/combat/enemies.txt) mis à l'échelle : PV et dégâts montent d'un
 * biome à l'autre, pour donner envie d'avancer dans l'ordre sans l'imposer.
 *
 * Repères de difficulté au niveau 1 (une attaque légère : 10 dégâts) : ennemi facile de la plaine
 * 3 coups (30 PV), ennemi dur 6 coups (60 PV, deux fois plus grand), boss 20 coups (200 PV). */

#include <stdbool.h>
#include <stdint.h>

#include "engine/biome.h"

typedef enum EngMapZoneKind {
    ENG_MAPZONE_FARM_EASY = 0,
    ENG_MAPZONE_FARM_HARD = 1,
    ENG_MAPZONE_BOSS = 2,
    ENG_MAPZONE_KINDS
} EngMapZoneKind;

#define ENG_MAPZONE_MAX_ENEMIES 3U
#define ENG_MAPZONES_PER_BIOME 3U
#define ENG_MAPZONE_COUNT (ENG_BIOME_COUNT * ENG_MAPZONES_PER_BIOME)

typedef struct EngMapZone {
    const char *name; /* affiché à l'entrée de la zone */
    EngBiome biome;
    EngMapZoneKind kind;
    double x; /* centre, mètres monde */
    double z;
    const char *arch[ENG_MAPZONE_MAX_ENEMIES]; /* archétypes des ennemis (enemies.txt) */
    uint32_t enemy_count;
    float hp;           /* PV de chaque ennemi de la zone */
    float damage_scale; /* dégâts infligés x */
    float size_scale;   /* taille du personnage x */
    uint32_t reward;    /* boss : palier de butin donné (1 à 4) ; 0 : rien */
} EngMapZone;

/* Les ENG_MAPZONE_COUNT zones, dans l'ordre du chemin (x croissant). `map` place chaque zone dans
 * sa bande (carte par défaut si NULL). false si `out` est NULL ou la carte invalide. */
bool eng_mapzones_build(const EngBiomeMap *map, EngMapZone out[ENG_MAPZONE_COUNT]);

/* Indice de la zone dont le centre est le plus proche de (x, z), à `max_distance` au plus ;
 * -1 si aucune. */
int32_t eng_mapzones_nearest(const EngMapZone zones[ENG_MAPZONE_COUNT], double x, double z,
                             double max_distance);

const char *eng_mapzone_kind_name(EngMapZoneKind kind);

#endif
