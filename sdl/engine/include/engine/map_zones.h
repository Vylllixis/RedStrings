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
#include "engine/boss_special.h"

typedef enum EngMapZoneKind {
    ENG_MAPZONE_FARM_EASY = 0,
    ENG_MAPZONE_FARM_HARD = 1,
    ENG_MAPZONE_BOSS = 2,
    ENG_MAPZONE_KINDS,                   /* genres des zones fixes (trois par biome) */
    ENG_MAPZONE_PACK = ENG_MAPZONE_KINDS /* meute de petits monstres (eng_mapzones_pack) */
} EngMapZoneKind;

#define ENG_MAPZONE_MAX_ENEMIES 5U
#define ENG_MAPZONES_PER_BIOME 3U
#define ENG_MAPZONE_COUNT (ENG_BIOME_COUNT * ENG_MAPZONES_PER_BIOME)

/* Meutes (demande de Théo, 2026-11-22 : « plus de petits mobs, pour les farmer et monter de
 * niveau ») : ENG_MAPZONE_PACK_SIZE petits squelettes (archétype « imp ») aux PV du biome, bien
 * plus fragiles que l'ennemi facile ; la meute vaincue revient vite. La carte en pose des
 * dizaines (lignes « meute x z », engine/world_map.h). */
#define ENG_MAPZONE_PACK_SIZE 5U

/* Distance (m) sous laquelle une zone s'active : une zone ordinaire, une meute (plus courte : elles
 * sont rapprochées), l'antre du dragon (il est dix fois plus grand et vole : on le voit de loin).
 */
#define ENG_MAPZONE_REACH 110.0
#define ENG_MAPZONE_PACK_REACH 85.0
#define ENG_MAPZONE_DRAGON_REACH 420.0

typedef struct EngMapZone {
    const char *boss_title; /* zone de boss : nom du boss (barre de vie) ; NULL sinon */
    const char *name;       /* affiché à l'entrée de la zone */
    EngBiome biome;
    EngMapZoneKind kind;
    double x; /* centre, mètres monde */
    double z;
    double reach; /* la zone s'active quand le joueur est à moins de `reach` m du centre */
    const char *arch[ENG_MAPZONE_MAX_ENEMIES]; /* archétypes des ennemis (enemies.txt) */
    uint32_t enemy_count;
    EngBossKind boss_kind; /* zone de boss : boss spécial (cavalier, dragon) ou ordinaire */
    float hp;              /* PV de chaque ennemi de la zone */
    float damage_scale;    /* dégâts infligés x */
    float size_scale;      /* taille du personnage x */
    uint32_t reward;       /* boss : palier de butin donné (1 à 4) ; 0 : rien */
} EngMapZone;

/* Les ENG_MAPZONE_COUNT zones, dans l'ordre du chemin (x croissant). `map` place chaque zone dans
 * sa bande (carte par défaut si NULL). false si `out` est NULL ou la carte invalide. */
bool eng_mapzones_build(const EngBiomeMap *map, EngMapZone out[ENG_MAPZONE_COUNT]);

/* Indice de la zone dont le centre est le plus proche de (x, z), à `max_distance` au plus ;
 * -1 si aucune. */
int32_t eng_mapzones_nearest(const EngMapZone zones[ENG_MAPZONE_COUNT], double x, double z,
                             double max_distance);

/* Meute du biome `biome` posée en (x, z) : genre ENG_MAPZONE_PACK, ENG_MAPZONE_PACK_SIZE ennemis
 * « imp ». false si `out` est NULL, le biome inconnu ou la position non finie. */
bool eng_mapzones_pack(EngBiome biome, double x, double z, EngMapZone *out);

/* Parmi `count` zones (les zones fixes puis les meutes), indice de celle à activer pour un joueur
 * en (x, z) : la plus proche de celles dont il est à portée (`reach` de chacune) ; -1 si aucune. */
int32_t eng_mapzones_active(const EngMapZone *zones, uint32_t count, double x, double z);

const char *eng_mapzone_kind_name(EngMapZoneKind kind);

#endif
