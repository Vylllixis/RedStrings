/* Carte du monde : lecture du fichier texte (format : docs/world-map.md). */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/world_map.h"

#define MAX_TOKENS 96U
#define TOKEN_LEN 32U

typedef struct Line {
    char tok[MAX_TOKENS][TOKEN_LEN];
    uint32_t count;
    uint32_t number;
} Line;

static bool fail(char *err, size_t cap, uint32_t line, const char *fmt, ...)
{
    if (err != NULL && cap > 0U) {
        char msg[160];
        va_list ap;
        va_start(ap, fmt);
        /* faux positif : ap est initialisé par va_start juste au-dessus (comme anim_graph.c) */
        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        (void)vsnprintf(msg, sizeof msg, fmt, ap);
        va_end(ap);
        (void)snprintf(err, cap, "ligne %u : %s", line, msg);
    }
    return false;
}

/* découpe une ligne en mots ; un « # » commence un commentaire */
static bool split(const char *start, size_t len, Line *out)
{
    out->count = 0;
    size_t i = 0;
    while (i < len) {
        while (i < len && (start[i] == ' ' || start[i] == '\t' || start[i] == '\r')) {
            i++;
        }
        if (i >= len || start[i] == '#') {
            break;
        }
        if (out->count >= MAX_TOKENS) {
            return false;
        }
        size_t n = 0;
        while (i < len && start[i] != ' ' && start[i] != '\t' && start[i] != '\r' &&
               start[i] != '#') {
            if (n + 1U >= TOKEN_LEN) {
                return false;
            }
            out->tok[out->count][n++] = start[i++];
        }
        out->tok[out->count][n] = '\0';
        out->count++;
    }
    return true;
}

static bool number(const Line *l, uint32_t i, float *out)
{
    if (i >= l->count) {
        return false;
    }
    char *end = NULL;
    const float v = strtof(l->tok[i], &end);
    if (end == l->tok[i] || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

/* lit `n` nombres à partir du mot `first` */
static bool numbers(const Line *l, uint32_t first, uint32_t n, float *out)
{
    for (uint32_t i = 0; i < n; i++) {
        if (!number(l, first + i, &out[i])) {
            return false;
        }
    }
    return true;
}

static int biome_of(const char *word)
{
    static const char *const names[ENG_BIOME_COUNT] = {"plaine", "montagne", "desert", "volcan"};
    for (int b = 0; b < ENG_BIOME_COUNT; b++) {
        if (strcmp(word, names[b]) == 0) {
            return b;
        }
    }
    return -1;
}

static int zone_kind_of(const char *word)
{
    static const char *const names[ENG_MAPZONE_KINDS] = {"facile", "dur", "boss"};
    for (int k = 0; k < ENG_MAPZONE_KINDS; k++) {
        if (strcmp(word, names[k]) == 0) {
            return k;
        }
    }
    return -1;
}

/* nombres par forme ronde : mot-clé, genre, nombres obligatoires, nombres facultatifs */
typedef struct RoundShape {
    const char *word;
    EngMapFeatureKind kind;
    uint32_t required;
    uint32_t optional;
} RoundShape;

static const RoundShape ROUND_SHAPES[] = {
    {"colline", ENG_MAPF_HILL, 4U, 1U},      /* x z rayon hauteur [plateau] */
    {"montagne", ENG_MAPF_MOUNTAIN, 4U, 0U}, /* x z rayon hauteur */
    {"volcan", ENG_MAPF_VOLCANO, 6U, 0U},    /* x z rayon hauteur rayon_cratère profondeur */
    {"plateau", ENG_MAPF_MESA, 4U, 0U},      /* x z rayon hauteur */
    {"dunes", ENG_MAPF_DUNES, 6U, 0U},       /* x z rayon amplitude longueur_onde direction_deg */
    {"aplanir", ENG_MAPF_FLATTEN, 3U, 1U},   /* x z rayon [hauteur] */
    {"lac", ENG_MAPF_LAKE, 4U, 0U},          /* x z rayon profondeur */
};

static bool add_feature(EngWorldMap *m, EngMapFeature f, uint32_t line, char *err, size_t cap)
{
    if (m->feature_count >= ENG_WORLDMAP_MAX_FEATURES) {
        return fail(err, cap, line, "trop de formes (au plus %u)",
                    (unsigned)ENG_WORLDMAP_MAX_FEATURES);
    }
    m->features[m->feature_count++] = f;
    return true;
}

static bool parse_round(EngWorldMap *m, const Line *l, const RoundShape *s, char *err, size_t cap)
{
    const uint32_t given = l->count - 1U;
    if (given < s->required || given > s->required + s->optional) {
        return fail(err, cap, l->number, "« %s » attend %u nombre(s)%s", s->word,
                    (unsigned)s->required, s->optional > 0U ? " (+1 facultatif)" : "");
    }
    float v[7] = {0};
    if (!numbers(l, 1U, given, v)) {
        return fail(err, cap, l->number, "« %s » : nombre illisible", s->word);
    }
    EngMapFeature f = {
        .x = v[0], .z = v[1], .radius = v[2], .height = v[3], .kind = (uint8_t)s->kind};
    if (s->kind == ENG_MAPF_HILL) {
        f.a = given > 4U ? v[4] : 0.0F;
    } else if (s->kind == ENG_MAPF_VOLCANO) {
        f.a = v[4];
        f.b = v[5];
    } else if (s->kind == ENG_MAPF_DUNES) {
        f.height = 0.0F;
        f.a = v[3];
        f.b = v[4];
        f.c = v[5] * 0.017453293F;
    } else if (s->kind == ENG_MAPF_FLATTEN) {
        f.height = given > 3U ? v[3] : NAN;
    }
    const bool bad = !(f.radius > 0.0F) || (s->kind == ENG_MAPF_HILL && !(f.a < f.radius)) ||
                     (s->kind == ENG_MAPF_VOLCANO && !(f.a > 0.0F && f.a < f.radius)) ||
                     (s->kind == ENG_MAPF_DUNES && !(f.b > 0.0F));
    if (bad) {
        return fail(err, cap, l->number, "« %s » : rayon ou dimensions incohérents", s->word);
    }
    return add_feature(m, f, l->number, err, cap);
}

/* crete largeur hauteur x z x z ... | riviere largeur profondeur x z ... | chemin largeur x z ...
 */
static bool parse_line_shape(EngWorldMap *m, const Line *l, EngMapFeatureKind kind, char *err,
                             size_t cap)
{
    const uint32_t head = (kind == ENG_MAPF_PATH || kind == ENG_MAPF_LAVA) ? 2U : 3U;
    if (l->count < head + 4U || ((l->count - head) % 2U) != 0U) {
        return fail(err, cap, l->number, "« %s » : largeur%s puis au moins deux points x z",
                    l->tok[0], head == 2U ? "" : ", hauteur");
    }
    float w = 0.0F;
    float h = 0.0F;
    if (!number(l, 1U, &w) || !(w > 0.0F) || (head == 3U && !number(l, 2U, &h))) {
        return fail(err, cap, l->number, "« %s » : largeur ou hauteur illisible", l->tok[0]);
    }
    const uint32_t n = (l->count - head) / 2U;
    if (m->point_count + n > ENG_WORLDMAP_MAX_POINTS) {
        return fail(err, cap, l->number, "trop de points (au plus %u)",
                    (unsigned)ENG_WORLDMAP_MAX_POINTS);
    }
    EngMapFeature f = {.radius = w,
                       .height = h,
                       .first_point = m->point_count,
                       .point_count = n,
                       .kind = (uint8_t)kind};
    for (uint32_t i = 0; i < n; i++) {
        if (!numbers(l, head + (2U * i), 2U, m->points[m->point_count + i])) {
            return fail(err, cap, l->number, "« %s » : point %u illisible", l->tok[0], i + 1U);
        }
    }
    m->point_count += n;
    return add_feature(m, f, l->number, err, cap);
}

static bool parse_setting(EngWorldMap *m, const Line *l, char *err, size_t cap)
{
    const char *w = l->tok[0];
    float v[4] = {0};
    if (strcmp(w, "monde") == 0) { /* monde côté espacement */
        if (l->count != 3U || !numbers(l, 1U, 2U, v) || !(v[1] > 0.0F) || !(v[0] >= v[1] * 16.0F) ||
            (uint32_t)lroundf(v[0] / v[1]) + 1U > ENG_WORLDMAP_MAX_SIZE ||
            fabsf((v[0] / v[1]) - roundf(v[0] / v[1])) > 1e-3F) {
            return fail(err, cap, l->number,
                        "« monde côté espacement » : côté multiple de l'espacement, au plus %u "
                        "échantillons",
                        (unsigned)ENG_WORLDMAP_MAX_SIZE);
        }
        m->side = v[0];
        m->spacing = v[1];
    } else if (strcmp(w, "eau") == 0) {
        if (l->count != 2U || !number(l, 1U, &m->water)) {
            return fail(err, cap, l->number, "« eau niveau »");
        }
    } else if (strcmp(w, "bordure") == 0) {
        if (l->count != 3U || !numbers(l, 1U, 2U, v) || !(v[0] > 0.0F)) {
            return fail(err, cap, l->number, "« bordure début hauteur »");
        }
        m->border_start = v[0];
        m->border_height = v[1];
    } else if (strcmp(w, "depart") == 0) {
        if (l->count != 3U || !numbers(l, 1U, 2U, m->spawn)) {
            return fail(err, cap, l->number, "« depart x z »");
        }
    } else {
        return fail(err, cap, l->number, "mot-clé inconnu « %s »", w);
    }
    return true;
}

static bool parse_region_or_zone(EngWorldMap *m, const Line *l, char *err, size_t cap)
{
    const bool region = strcmp(l->tok[0], "region") == 0;
    const int biome = l->count > 1U ? biome_of(l->tok[1]) : -1;
    if (biome < 0) {
        return fail(err, cap, l->number, "biome inconnu (plaine, montagne, desert, volcan)");
    }
    float v[3] = {0};
    if (region) { /* region biome x z portée */
        if (l->count != 5U || !numbers(l, 2U, 3U, v) || !(v[2] > 0.0F)) {
            return fail(err, cap, l->number, "« region biome x z portée »");
        }
        if (m->region_count >= ENG_WORLDMAP_MAX_REGIONS) {
            return fail(err, cap, l->number, "trop de régions");
        }
        m->regions[m->region_count++] =
            (EngMapRegion){.x = v[0], .z = v[1], .reach = v[2], .biome = (uint8_t)biome};
        return true;
    }
    const int kind = l->count > 2U ? zone_kind_of(l->tok[2]) : -1; /* zone biome genre x z */
    if (kind < 0 || l->count != 5U || !numbers(l, 3U, 2U, v)) {
        return fail(err, cap, l->number, "« zone biome facile|dur|boss x z »");
    }
    EngMapZoneSpot *s = &m->zones[(biome * (int)ENG_MAPZONES_PER_BIOME) + kind];
    if (s->used) {
        return fail(err, cap, l->number, "zone déjà placée");
    }
    *s = (EngMapZoneSpot){
        .x = v[0], .z = v[1], .biome = (uint8_t)biome, .kind = (uint8_t)kind, .used = true};
    return true;
}

static bool parse_one(EngWorldMap *m, const Line *l, char *err, size_t cap)
{
    const char *w = l->tok[0];
    for (size_t i = 0; i < sizeof ROUND_SHAPES / sizeof ROUND_SHAPES[0]; i++) {
        if (strcmp(w, ROUND_SHAPES[i].word) == 0) {
            return parse_round(m, l, &ROUND_SHAPES[i], err, cap);
        }
    }
    if (strcmp(w, "crete") == 0) {
        return parse_line_shape(m, l, ENG_MAPF_RIDGE, err, cap);
    }
    if (strcmp(w, "riviere") == 0) {
        return parse_line_shape(m, l, ENG_MAPF_RIVER, err, cap);
    }
    if (strcmp(w, "chemin") == 0) {
        return parse_line_shape(m, l, ENG_MAPF_PATH, err, cap);
    }
    if (strcmp(w, "lave") == 0) {
        return parse_line_shape(m, l, ENG_MAPF_LAVA, err, cap);
    }
    if (strcmp(w, "region") == 0 || strcmp(w, "zone") == 0) {
        return parse_region_or_zone(m, l, err, cap);
    }
    return parse_setting(m, l, err, cap);
}

bool eng_worldmap_parse(EngWorldMap *map, const char *text, size_t length, char *err,
                        size_t err_cap)
{
    if (map == NULL || text == NULL) {
        return fail(err, err_cap, 0U, "argument absent");
    }
    memset(map, 0, sizeof *map);
    map->border_height = 0.0F;
    static Line line; /* ~3 Kio : hors pile ; la lecture n'est pas réentrante */
    uint32_t number_of_line = 0;
    size_t start = 0;
    while (start < length) {
        size_t end = start;
        while (end < length && text[end] != '\n' && text[end] != '\0') {
            end++;
        }
        number_of_line++;
        if (!split(text + start, end - start, &line)) {
            return fail(err, err_cap, number_of_line, "ligne trop longue ou mot trop long");
        }
        line.number = number_of_line;
        if (line.count > 0U && !parse_one(map, &line, err, err_cap)) {
            return false;
        }
        if (end < length && text[end] == '\0') {
            break;
        }
        start = end + 1U;
    }
    if (!(map->side > 0.0F)) {
        return fail(err, err_cap, number_of_line, "« monde côté espacement » manquant");
    }
    if (map->region_count == 0U) {
        return fail(err, err_cap, number_of_line, "aucune région");
    }
    return true;
}

const char *eng_mapfeature_name(EngMapFeatureKind kind)
{
    static const char *const names[ENG_MAPF_KINDS] = {"colline", "montagne", "crete",   "volcan",
                                                      "plateau", "dunes",    "aplanir", "chemin",
                                                      "lac",     "riviere",  "lave"};
    return (unsigned)kind < (unsigned)ENG_MAPF_KINDS ? names[kind] : "?";
}
