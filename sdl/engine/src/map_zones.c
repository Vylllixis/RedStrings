#include "engine/map_zones.h"

#include <math.h>
#include <stddef.h>

/* Par biome : PV de l'ennemi facile, du dur et du boss ; dégâts x ; noms. La plaine suit les
 * repères de la demande (3, 6 et 20 coups de 10 dégâts) ; chaque biome suivant monte d'un cran. */
typedef struct BiomeTier {
    const char *easy_name;
    const char *hard_name;
    const char *boss_name;
    const char *boss_title;
    const char *pack_name;
    float hp_easy;
    float hp_hard;
    float hp_boss;
    float hp_pack; /* petit monstre d'une meute : 0,4 fois l'ennemi facile (2 coups en plaine) */
    float damage;
} BiomeTier;

static const BiomeTier TIERS[ENG_BIOME_COUNT] = {
    {"Plaine : avant-poste des soldats", "Plaine : camp des brutes",
     "Plaine : château du chevalier", "Chevalier noir", "Plaine : meute d'osselets", 30.0F, 60.0F,
     200.0F, 12.0F, 1.0F},
    {"Montagne : col des éclaireurs", "Montagne : camp des géants des neiges",
     "Montagne : sommet du seigneur du givre", "Seigneur du givre",
     "Montagne : meute d'osselets du givre", 55.0F, 110.0F, 360.0F, 22.0F, 1.5F},
    {"Désert : oasis des pillards", "Désert : ruines des colosses",
     "Désert : arène du roi des sables", "Roi des sables", "Désert : meute d'osselets des sables",
     95.0F, 190.0F, 620.0F, 38.0F, 2.2F},
    {"Volcan : pentes de cendre", "Volcan : forge des titans", "Volcan : antre du dragon",
     "Dragon de lave", "Volcan : meute d'osselets de cendre", 150.0F, 300.0F, 1000.0F, 60.0F, 3.0F},
};

/* place de chaque genre de zone dans la bande de son biome (fraction de la longueur) */
static const float ALONG[ENG_MAPZONE_KINDS] = {0.30F, 0.58F, 0.84F};
static const float SIDE_M[ENG_MAPZONE_KINDS] = {40.0F, -60.0F, 20.0F}; /* décalage en z, mètres */

bool eng_mapzones_build(const EngBiomeMap *map, EngMapZone out[ENG_MAPZONE_COUNT])
{
    const EngBiomeMap fallback = eng_biome_map_default();
    const EngBiomeMap *m = map != NULL ? map : &fallback;
    if (out == NULL || !eng_biome_map_valid(m)) {
        return false;
    }
    for (int b = 0; b < ENG_BIOME_COUNT; b++) {
        const BiomeTier *tier = &TIERS[b];
        for (int k = 0; k < ENG_MAPZONE_KINDS; k++) {
            EngMapZone *z = &out[(b * (int)ENG_MAPZONES_PER_BIOME) + k];
            *z = (EngMapZone){.biome = (EngBiome)b,
                              .kind = (EngMapZoneKind)k,
                              .x = (double)m->start_x +
                                   (((double)b + (double)ALONG[k]) * (double)m->band_m),
                              .z = 128.0 + (double)SIDE_M[k],
                              .reach = ENG_MAPZONE_REACH,
                              .damage_scale = tier->damage,
                              .size_scale = 1.0F};
            if (k == ENG_MAPZONE_FARM_EASY) {
                z->name = tier->easy_name;
                z->arch[0] = "grunt";
                z->arch[1] = "grunt";
                z->arch[2] = "archer";
                z->enemy_count = 3;
                z->hp = tier->hp_easy;
            } else if (k == ENG_MAPZONE_FARM_HARD) {
                z->name = tier->hard_name;
                z->arch[0] = "brute";
                z->arch[1] = "brute";
                z->enemy_count = 2;
                z->hp = tier->hp_hard;
                z->size_scale =
                    1.67F; /* la brute (x 1,2) devient deux fois plus grande qu'un soldat */
            } else {
                z->name = tier->boss_name;
                z->arch[0] = "boss";
                z->enemy_count = 1;
                z->hp = tier->hp_boss;
                z->size_scale = 1.0F;
                z->reward = (uint32_t)b + 1U;
                z->boss_title = tier->boss_title;
                if (b == ENG_BIOME_PLAIN) { /* le Chevalier noir arrive à cheval */
                    z->boss_kind = ENG_BOSSKIND_RIDER;
                } else if (b == ENG_BIOME_VOLCANO) { /* boss final : le dragon */
                    z->boss_kind = ENG_BOSSKIND_DRAGON;
                    z->arch[0] = "dragon";
                    z->reach = ENG_MAPZONE_DRAGON_REACH;
                }
            }
        }
    }
    return true;
}

int32_t eng_mapzones_nearest(const EngMapZone zones[ENG_MAPZONE_COUNT], double x, double z,
                             double max_distance)
{
    if (zones == NULL || !isfinite(x) || !isfinite(z) || !(max_distance >= 0.0)) {
        return -1;
    }
    int32_t best = -1;
    double best_d = max_distance;
    for (int32_t i = 0; i < (int32_t)ENG_MAPZONE_COUNT; i++) {
        const double d = hypot(zones[i].x - x, zones[i].z - z);
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

bool eng_mapzones_pack(EngBiome biome, double x, double z, EngMapZone *out)
{
    if (out == NULL || (unsigned)biome >= (unsigned)ENG_BIOME_COUNT || !isfinite(x) ||
        !isfinite(z)) {
        return false;
    }
    const BiomeTier *tier = &TIERS[biome];
    *out = (EngMapZone){.name = tier->pack_name,
                        .biome = biome,
                        .kind = ENG_MAPZONE_PACK,
                        .x = x,
                        .z = z,
                        .reach = ENG_MAPZONE_PACK_REACH,
                        .enemy_count = ENG_MAPZONE_PACK_SIZE,
                        .hp = tier->hp_pack,
                        .damage_scale = tier->damage,
                        .size_scale = 1.0F};
    for (uint32_t i = 0; i < ENG_MAPZONE_PACK_SIZE; i++) {
        out->arch[i] = "imp";
    }
    return true;
}

int32_t eng_mapzones_active(const EngMapZone *zones, uint32_t count, double x, double z)
{
    if (zones == NULL || !isfinite(x) || !isfinite(z)) {
        return -1;
    }
    int32_t best = -1;
    double best_d = 0.0;
    for (uint32_t i = 0; i < count && i <= (uint32_t)INT32_MAX; i++) {
        const double d = hypot(zones[i].x - x, zones[i].z - z);
        if (!(d <= zones[i].reach)) {
            continue; /* hors de portée, ou portée non finie */
        }
        if (best < 0 || d < best_d) {
            best_d = d;
            best = (int32_t)i;
        }
    }
    return best;
}

const char *eng_mapzone_kind_name(EngMapZoneKind kind)
{
    switch (kind) {
    case ENG_MAPZONE_FARM_EASY:
        return "farm facile";
    case ENG_MAPZONE_FARM_HARD:
        return "farm dur";
    case ENG_MAPZONE_BOSS:
        return "boss";
    case ENG_MAPZONE_PACK:
        return "meute";
    default:
        return "?";
    }
}
