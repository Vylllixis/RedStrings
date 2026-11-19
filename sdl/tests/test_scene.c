#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <math.h>
#include <string.h>

#include "engine/files.h"
#include "engine/scene.h"
#include "utest.h"

/* Scène (M2-09) : format texte piloté par la description des composants. */

typedef struct Sample {
    int32_t count;
    uint32_t level;
    float ratio;
    float point[3];
    float turn[4];
    char label[40];
    bool flag;
} Sample;

static const EngFieldDesc SAMPLE_FIELDS[] = {
    {"count", ENG_FIELD_I32, offsetof(Sample, count), 0},
    {"level", ENG_FIELD_U32, offsetof(Sample, level), 0},
    {"ratio", ENG_FIELD_FLOAT, offsetof(Sample, ratio), 0},
    {"point", ENG_FIELD_VEC3, offsetof(Sample, point), 0},
    {"turn", ENG_FIELD_QUAT, offsetof(Sample, turn), 0},
    {"label", ENG_FIELD_STRING, offsetof(Sample, label), 40},
    {"flag", ENG_FIELD_BOOL, offsetof(Sample, flag), 0},
};

static const Sample SAMPLE_INITIAL = {.count = -3, .level = 7, .ratio = 0.5F, .flag = true};

/* Les tests : un monde avec le composant Sample enregistré (identifiant 4). */
enum { SAMPLE = 4 };

static bool make_world(EngWorld *world, uint32_t capacity)
{
    if (!eng_world_init(world, capacity)) {
        return false;
    }
    const EngComponentDesc desc = {.name = "Sample",
                                   .size = sizeof(Sample),
                                   .fields = SAMPLE_FIELDS,
                                   .field_count = 7,
                                   .initial = &SAMPLE_INITIAL};
    return eng_world_register(world, &desc) == SAMPLE;
}

static char *write_all(const EngWorld *world)
{
    size_t needed = 0;
    if (!eng_scene_write(world, NULL, 0, &needed)) {
        return NULL;
    }
    char *text = SDL_malloc(needed);
    if (text != NULL && !eng_scene_write(world, text, needed, &needed)) {
        SDL_free(text);
        return NULL;
    }
    return text;
}

/* Entités du monde en parcours en profondeur (racines dans l'ordre du monde) : l'ordre de la scène.
 */
static uint32_t preorder(const EngWorld *world, EngEntity *out, uint32_t max)
{
    uint32_t n = 0;
    uint32_t cursor = 0;
    EngEntity root;
    while (eng_world_next_entity(world, &cursor, &root)) {
        if (eng_world_parent(world, root).generation != 0) {
            continue;
        }
        for (EngEntity e = root; e.generation != 0; e = eng_world_next_in_subtree(world, root, e)) {
            if (n < max) {
                out[n] = e;
            }
            n++;
        }
    }
    return n;
}

static uint32_t index_of(const EngEntity *list, uint32_t n, EngEntity e)
{
    for (uint32_t i = 0; i < n; i++) {
        if (list[i].index == e.index && list[i].generation == e.generation) {
            return i;
        }
    }
    return UINT32_MAX;
}

/* Deux mondes ont les mêmes entités, la même hiérarchie et les mêmes champs persistants. */
static bool same_content(const EngWorld *a, const EngWorld *b)
{
    EngEntity la[256];
    EngEntity lb[256];
    const uint32_t na = preorder(a, la, 256);
    const uint32_t nb = preorder(b, lb, 256);
    if (na != nb || na > 256U) {
        return false;
    }
    for (uint32_t i = 0; i < na; i++) {
        const EngEntity pa = eng_world_parent(a, la[i]);
        const EngEntity pb = eng_world_parent(b, lb[i]);
        if ((pa.generation == 0) != (pb.generation == 0) ||
            (pa.generation != 0 && index_of(la, na, pa) != index_of(lb, nb, pb))) {
            return false;
        }
        for (uint32_t c = 1; c <= eng_world_component_count(a); c++) {
            const EngComponentDesc *desc = eng_world_component(a, c);
            const uint8_t *da = eng_world_get(a, la[i], c);
            const uint8_t *db = eng_world_get(b, lb[i], c);
            if ((da == NULL) != (db == NULL)) {
                return desc->field_count == 0; /* composant sans champ : non écrit */
            }
            for (uint32_t f = 0; da != NULL && f < desc->field_count; f++) {
                if (memcmp(da + desc->fields[f].offset, db + desc->fields[f].offset,
                           eng_field_size(&desc->fields[f])) != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

UTEST(eng_scene, the_text_format_is_exactly_what_the_documentation_shows)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 8));
    const EngEntity root = eng_world_create(&w, "root");
    EngTransform *t = eng_world_add(&w, root, ENG_COMPONENT_TRANSFORM);
    t->position[0] = 1.0F;
    t->position[1] = 2.0F;
    t->position[2] = 3.0F;
    const EngEntity child = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, child, root));
    EngMeshRenderer *m = eng_world_add(&w, child, ENG_COMPONENT_MESH_RENDERER);
    SDL_strlcpy(m->model, "m.mesh", sizeof m->model);
    m->cast_shadow = false;

    char *text = write_all(&w);
    ASSERT_TRUE(text != NULL);
    EXPECT_STREQ(text, "openworld-scene 1\n"
                       "\n"
                       "entity 0 parent -\n"
                       "[Name]\n"
                       "text = \"root\"\n"
                       "[Transform]\n"
                       "position = 1 2 3\n"
                       "rotation = 0 0 0 1\n"
                       "scale = 1 1 1\n"
                       "\n"
                       "entity 1 parent 0\n"
                       "[MeshRenderer]\n"
                       "model = \"m.mesh\"\n"
                       "visible = true\n"
                       "cast_shadow = false\n");
    SDL_free(text);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, an_empty_world_is_just_the_header)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    char *text = write_all(&w);
    ASSERT_TRUE(text != NULL);
    EXPECT_STREQ(text, "openworld-scene 1\n");
    EngWorld back;
    ASSERT_TRUE(make_world(&back, 4));
    EngSceneError error;
    EXPECT_TRUE(eng_scene_read(&back, text, strlen(text), &error));
    EXPECT_EQ(eng_world_entity_count(&back), (uint32_t)0);
    SDL_free(text);
    eng_world_shutdown(&back);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, writing_reports_the_needed_size_and_never_overflows_the_buffer)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    (void)eng_world_create(&w, "a");
    size_t needed = 0;
    ASSERT_TRUE(eng_scene_write(&w, NULL, 0, &needed));
    char *full = write_all(&w);
    ASSERT_TRUE(full != NULL);
    EXPECT_EQ(needed, strlen(full) + 1U);

    char small[16];
    memset(small, 'Z', sizeof small);
    size_t needed_small = 0;
    ASSERT_TRUE(eng_scene_write(&w, small, 10, &needed_small));
    EXPECT_EQ(needed_small, needed);
    EXPECT_EQ(small[9], '\0');             /* terminée dans la capacité donnée */
    EXPECT_EQ(strncmp(small, full, 9), 0); /* début identique */
    EXPECT_EQ(small[10], 'Z');             /* rien au-delà */
    EXPECT_EQ(small[15], 'Z');
    SDL_free(full);
    eng_world_shutdown(&w);
}

static uint32_t float_bits(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

UTEST(eng_scene, floats_survive_a_round_trip_bit_for_bit)
{
    const float values[] = {0.0F,
                            -0.0F,
                            0.1F,
                            -0.1F,
                            1.0F / 3.0F,
                            1.0e-30F,
                            1.17549435e-38F, /* plus petit normalisé */
                            1.40129846e-45F, /* plus petit dénormalisé */
                            3.40282347e38F,  /* plus grand fini */
                            -3.40282347e38F,
                            123456789.0F,
                            16777217.0F};
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const EngEntity e = eng_world_create(&w, NULL);
    Sample *s = eng_world_add(&w, e, SAMPLE);
    ASSERT_TRUE(s != NULL);
    for (size_t i = 0; i + 3 < sizeof values / sizeof values[0]; i++) {
        s = eng_world_get(&w, e, SAMPLE);
        s->ratio = values[i];
        memcpy(s->point, &values[i], sizeof s->point);
        s->turn[0] = values[i + 3];
        char *text = write_all(&w);
        ASSERT_TRUE(text != NULL);
        EngWorld back;
        ASSERT_TRUE(make_world(&back, 4));
        EngSceneError error;
        ASSERT_TRUE(eng_scene_read(&back, text, strlen(text), &error));
        EngEntity list[2];
        ASSERT_EQ(preorder(&back, list, 2), (uint32_t)1);
        const Sample *r = eng_world_get(&back, list[0], SAMPLE);
        ASSERT_TRUE(r != NULL);
        EXPECT_EQ(float_bits(r->ratio), float_bits(s->ratio));
        for (int k = 0; k < 3; k++) {
            EXPECT_EQ(float_bits(r->point[k]), float_bits(s->point[k]));
        }
        EXPECT_EQ(float_bits(r->turn[0]), float_bits(s->turn[0]));
        SDL_free(text);
        eng_world_shutdown(&back);
    }
    eng_world_shutdown(&w);
}

UTEST(eng_scene, strings_with_quotes_newlines_controls_and_utf8_survive)
{
    const char *samples[] = {"simple",
                             "guillemet \" et antislash \\",
                             "ligne 1\nligne 2\r\n\ttab",
                             "contrôle \x01\x1f\x7f fin",
                             "accentué : é à ü 日本",
                             "",
                             "#pas un commentaire",
                             "a = b"};
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        EngWorld w;
        ASSERT_TRUE(make_world(&w, 4));
        const EngEntity e = eng_world_create(&w, samples[i]);
        Sample *s = eng_world_add(&w, e, SAMPLE);
        SDL_strlcpy(s->label, samples[i], sizeof s->label);
        char *text = write_all(&w);
        ASSERT_TRUE(text != NULL);
        EngWorld back;
        ASSERT_TRUE(make_world(&back, 4));
        EngSceneError error;
        ASSERT_TRUE(eng_scene_read(&back, text, strlen(text), &error));
        EXPECT_TRUE(same_content(&w, &back));
        EngEntity found = eng_world_find_by_name(&back, samples[i][0] != '\0' ? samples[i] : "x");
        if (samples[i][0] != '\0') {
            EXPECT_TRUE(found.generation != 0);
        }
        char *again = write_all(&back);
        ASSERT_TRUE(again != NULL);
        EXPECT_STREQ(again, text);
        SDL_free(again);
        SDL_free(text);
        eng_world_shutdown(&back);
        eng_world_shutdown(&w);
    }
}

/* Générateur pseudo-aléatoire déterministe. */
static uint32_t next_random(uint32_t *state)
{
    *state = (*state * 1664525U) + 1013904223U;
    return *state >> 8;
}

static float random_float(uint32_t *state)
{
    for (;;) {
        const uint32_t bits = (next_random(state) << 16) ^ next_random(state);
        float v;
        memcpy(&v, &bits, sizeof v);
        if (isfinite(v)) {
            return v;
        }
    }
}

UTEST(eng_scene, random_worlds_round_trip_exactly_and_the_text_is_stable)
{
    uint32_t seed = 12345U;
    for (int iteration = 0; iteration < 60; iteration++) {
        EngWorld w;
        ASSERT_TRUE(make_world(&w, 64));
        EngEntity made[40];
        const uint32_t count = 1U + (next_random(&seed) % 40U);
        for (uint32_t i = 0; i < count; i++) {
            char name[16];
            SDL_snprintf(name, sizeof name, "e%u", (unsigned)(next_random(&seed) % 1000U));
            made[i] = eng_world_create(&w, (next_random(&seed) % 4U) != 0 ? name : NULL);
            ASSERT_TRUE(made[i].generation != 0);
            if (i > 0 && (next_random(&seed) % 3U) != 0) {
                (void)eng_world_set_parent(&w, made[i], made[next_random(&seed) % i]);
            }
            if ((next_random(&seed) % 2U) != 0) {
                EngTransform *t = eng_world_add(&w, made[i], ENG_COMPONENT_TRANSFORM);
                for (int k = 0; k < 3; k++) {
                    t->position[k] = random_float(&seed);
                    t->scale[k] = random_float(&seed);
                }
                for (int k = 0; k < 4; k++) {
                    t->rotation[k] = random_float(&seed);
                }
            }
            if ((next_random(&seed) % 2U) != 0) {
                Sample *s = eng_world_add(&w, made[i], SAMPLE);
                s->count = (int32_t)next_random(&seed) - 4000000;
                s->level = next_random(&seed) * 257U;
                s->ratio = random_float(&seed);
                s->flag = (next_random(&seed) % 2U) != 0;
                SDL_snprintf(s->label, sizeof s->label, "v%u \"q\"", (unsigned)next_random(&seed));
            }
        }
        char *text = write_all(&w);
        ASSERT_TRUE(text != NULL);
        EngWorld back;
        ASSERT_TRUE(make_world(&back, 64));
        EngSceneError error;
        ASSERT_TRUE(eng_scene_read(&back, text, strlen(text), &error));
        EXPECT_TRUE(same_content(&w, &back));
        char *again = write_all(&back);
        ASSERT_TRUE(again != NULL);
        EXPECT_STREQ(again, text); /* texte identique à l'octet près */
        SDL_free(again);
        SDL_free(text);
        eng_world_shutdown(&back);
        eng_world_shutdown(&w);
    }
}

UTEST(eng_scene, entities_come_parents_first_and_forward_references_are_accepted)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 8));
    const char *text = "openworld-scene 1\n"
                       "entity 0 parent 2\n[Name]\ntext = \"enfant\"\n"
                       "entity 1 parent -\n[Name]\ntext = \"autre racine\"\n"
                       "entity 2 parent -\n[Name]\ntext = \"parent\"\n";
    EngSceneError error;
    ASSERT_TRUE(eng_scene_read(&w, text, strlen(text), &error));
    const EngEntity parent = eng_world_find_by_name(&w, "parent");
    const EngEntity child = eng_world_find_by_name(&w, "enfant");
    EXPECT_TRUE(eng_world_parent(&w, child).index == parent.index);
    char *out = write_all(&w);
    ASSERT_TRUE(out != NULL);
    /* réécrit : « autre racine » (racine 1), puis « parent » avec son enfant */
    const char *p_other = strstr(out, "autre racine");
    const char *p_parent = strstr(out, "\"parent\"");
    const char *p_child = strstr(out, "enfant");
    ASSERT_TRUE(p_other != NULL && p_parent != NULL && p_child != NULL);
    EXPECT_TRUE(p_parent < p_child);
    EXPECT_TRUE(strstr(out, "entity 2 parent 1") != NULL);
    SDL_free(out);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, comments_blank_lines_indentation_and_crlf_are_accepted)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const char *text = "# une scène\r\n\r\nopenworld-scene 1\r\n   # commentaire indenté\r\n"
                       "entity 0 parent -   \r\n  [Transform]\r\n  position =   1   2\t3  \r\n"
                       "\tscale=2 2 2\r\n";
    EngSceneError error;
    ASSERT_TRUE(eng_scene_read(&w, text, strlen(text), &error));
    EngEntity list[2];
    ASSERT_EQ(preorder(&w, list, 2), (uint32_t)1);
    const EngTransform *t = eng_world_get(&w, list[0], ENG_COMPONENT_TRANSFORM);
    ASSERT_TRUE(t != NULL);
    EXPECT_TRUE(t->position[0] == 1.0F && t->position[1] == 2.0F && t->position[2] == 3.0F);
    EXPECT_TRUE(t->scale[1] == 2.0F);
    EXPECT_TRUE(t->rotation[3] == 1.0F); /* champ absent : valeur par défaut */
    eng_world_shutdown(&w);
}

UTEST(eng_scene, the_last_line_needs_no_newline_and_the_length_decides_where_text_ends)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const char text[] = "openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"x\"GARBAGE";
    EngSceneError error;
    ASSERT_TRUE(eng_scene_read(&w, text, sizeof text - 1U - 7U, &error)); /* sans « GARBAGE » */
    EXPECT_TRUE(eng_world_find_by_name(&w, "x").generation != 0);
    eng_world_shutdown(&w);
}

typedef struct BadCase {
    const char *text;
    uint32_t line;
    const char *message_part;
} BadCase;

UTEST(eng_scene, every_kind_of_defect_is_reported_with_its_line_and_adds_nothing)
{
    static const BadCase cases[] = {
        {"", 0, "entête"},
        {"# seulement un commentaire\n", 1, "entête"},
        {"openworld-scene 2\n", 1, "version"},
        {"openworld-scene\n", 1, "entête"},
        {"scene 1\n", 1, "entête"},
        {"openworld-scene 1\nentity 1 parent -\n", 2, "consécutifs"},
        {"openworld-scene 1\nentity 0\n", 2, "entity N parent P"},
        {"openworld-scene 1\nentity 0 parent x\n", 2, "invalide"},
        {"openworld-scene 1\n[Name]\n", 2, "avant la première entité"},
        {"openworld-scene 1\nentity 0 parent -\n[Nope]\n", 3, "composant inconnu"},
        {"openworld-scene 1\nentity 0 parent -\n[Name\n", 3, "[Composant]"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\n[Name]\n", 4, "en double"},
        {"openworld-scene 1\nentity 0 parent -\nposition = 1 2 3\n", 3, "inattendue"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition 1 2 3\n", 4,
         "champ = valeur"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\ncolor = 1 2 3\n", 4, "champ inconnu"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 3\nposition = 1 2 3\n",
         5, "en double"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2\n", 4, "trois valeurs"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 3 4\n", 4,
         "trois valeurs"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nrotation = 0 0 1\n", 4,
         "quatre valeurs"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 abc\n", 4,
         "nombre invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 nan\n", 4,
         "nombre invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 inf\n", 4,
         "nombre invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 1e39\n", 4,
         "nombre invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 3x\n", 4,
         "nombre invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[MeshRenderer]\nvisible = yes\n", 4, "true"},
        {"openworld-scene 1\nentity 0 parent -\n[MeshRenderer]\nvisible = true false\n", 4,
         "une seule valeur"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\ncount = 2147483648\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\ncount = 1.5\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\ncount = 0x10\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\nlevel = -1\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\nlevel = 4294967296\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = sans guillemets\n", 4, "guillemets"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"ouverte\n", 4, "non terminée"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"a\" b\n", 4, "après la chaîne"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"a\\q\"\n", 4, "échappement"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"a\\x4\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"a\\x00b\"\n", 4, "nul"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = "
         "\"123456789012345678901234567890123456789012345678\"\n",
         4, "trop longue"},
        {"openworld-scene 1\nentity 0 parent 5\n", 2, "inexistant"},
        {"openworld-scene 1\nentity 0 parent 1\n", 2, "inexistant"},
        {"openworld-scene 1 2 3\n", 1, "entête"},
        {"openworld-scene 1\nentity 0 parent - x y\n", 2, "entity N parent P"},
        {"openworld-scene 1\nentityx 0 parent -\n", 2, "inattendue"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\nposition = 1 2 3 4 5\n", 4,
         "trois valeurs"},
        {"openworld-scene 1\nentity 0 parent -\n[Transform]\n= 1 2 3\n", 4, "champ inconnu"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\xGG\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\x1g\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\x/0\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\x:0\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\x@0\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"\\x`0\"\n", 4, "\\x"},
        {"openworld-scene 1\nentity 0 parent -\n[Sample]\ncount = -2147483649\n", 4, "invalide"},
        {"openworld-scene 1\nentity 0 parent 0\n", 2, "refusé"},
        {"openworld-scene 1\nentity 0 parent 1\nentity 1 parent 0\n", 3, "refusé"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        EngWorld w;
        ASSERT_TRUE(make_world(&w, 8));
        const EngEntity before = eng_world_create(&w, "déjà là");
        EngSceneError error = {.line = 999};
        const bool ok = eng_scene_read(&w, cases[i].text, strlen(cases[i].text), &error);
        EXPECT_FALSE(ok);
        if (ok) {
            UTEST_PRINTF("cas %zu accepté à tort : %s\n", i, cases[i].text);
        } else {
            if (error.line != cases[i].line ||
                strstr(error.message, cases[i].message_part) == NULL) {
                UTEST_PRINTF("cas %zu : ligne %u, « %s » (attendu ligne %u, « %s »)\n", i,
                             (unsigned)error.line, error.message, (unsigned)cases[i].line,
                             cases[i].message_part);
            }
            EXPECT_EQ(error.line, cases[i].line);
            EXPECT_TRUE(strstr(error.message, cases[i].message_part) != NULL);
        }
        /* rien n'a été ajouté, rien n'a bougé */
        EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)1);
        EXPECT_TRUE(eng_world_alive(&w, before));
        EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_NAME), (uint32_t)1);
        EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_TRANSFORM), (uint32_t)0);
        EXPECT_EQ(eng_world_table_count(&w, SAMPLE), (uint32_t)0);
        eng_world_shutdown(&w);
    }
}

/* « - » pour la racine d'une chaîne, sinon le numéro du maillon précédent. */
static const char *parent_token(int i, char *buffer, size_t size)
{
    if (i == 0) {
        return "-";
    }
    SDL_snprintf(buffer, size, "%d", i - 1);
    return buffer;
}

UTEST(eng_scene, accepted_spellings_and_numeric_boundaries)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 8));
    const char *text = "openworld-scene 1\n"
                       "entity\t0\tparent\t-\n"
                       "[Sample]\n"
                       "count = -2147483648\n"
                       "level = 4294967295\n"
                       "label = \"\\x9F\\xFA\\xaf\\xf9\\xA0\\x0F\"   \t \n"
                       "entity 1 parent -\n"
                       "[Sample]\n"
                       "count = 2147483647\n"
                       "level = 0\n"
                       "label = \"x\"\n";
    EngSceneError error;
    ASSERT_TRUE(eng_scene_read(&w, text, strlen(text), &error));
    EngEntity list[3];
    ASSERT_EQ(preorder(&w, list, 3), (uint32_t)2);
    const Sample *a = eng_world_get(&w, list[0], SAMPLE);
    const Sample *b = eng_world_get(&w, list[1], SAMPLE);
    ASSERT_TRUE(a != NULL && b != NULL);
    EXPECT_EQ(a->count, INT32_MIN);
    EXPECT_EQ(a->level, UINT32_MAX);
    EXPECT_EQ(b->count, INT32_MAX);
    EXPECT_EQ(b->level, (uint32_t)0);
    const unsigned char expected[] = {0x9F, 0xFA, 0xAF, 0xF9, 0xA0, 0x0F, 0};
    EXPECT_EQ(memcmp(a->label, expected, sizeof expected), 0);
    EXPECT_STREQ(b->label, "x");
    eng_world_shutdown(&w);
}

UTEST(eng_scene, the_writer_respects_the_buffer_it_is_given)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const EngEntity e = eng_world_create(&w, "a");
    size_t needed = 0;
    char one[2] = {'Z', 'Z'};
    ASSERT_TRUE(eng_scene_write(&w, one, 0, &needed)); /* capacité nulle : rien n'est écrit */
    EXPECT_EQ(one[0], 'Z');
    EXPECT_EQ(one[1], 'Z');
    EXPECT_TRUE(needed > 1U);
    size_t measured = 0;
    ASSERT_TRUE(eng_scene_write(&w, NULL, 100, &measured)); /* sortie NULL : on mesure seulement */
    EXPECT_EQ(measured, needed);

    /* contrôles et chaîne sans zéro final : jamais de lecture au-delà du champ */
    Sample *s = eng_world_add(&w, e, SAMPLE);
    memset(s->label, 'x', sizeof s->label); /* 40 octets, aucun zéro */
    s->flag = true;                         /* l'octet qui suit le tableau est 0x01 */
    char *text = write_all(&w);
    ASSERT_TRUE(text != NULL);
    const char *label = strstr(text, "label = \"");
    ASSERT_TRUE(label != NULL);
    EXPECT_EQ(strncmp(label + 9, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"\n", 42), 0);
    SDL_free(text);

    SDL_strlcpy(s->label, "a\x01\x1f\x7f z", sizeof s->label);
    text = write_all(&w);
    ASSERT_TRUE(text != NULL);
    EXPECT_TRUE(strstr(text, "label = \"a\\x01\\x1F\\x7F z\"\n") != NULL);
    SDL_free(text);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, an_over_long_path_is_reported_for_both_load_and_save)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    char path[700];
    memset(path, 'a', sizeof path);
    path[sizeof path - 1U] = '\0';
    EngSceneError error = {.line = 3};
    EXPECT_FALSE(eng_scene_load(&w, path, &error));
    EXPECT_EQ(error.line, (uint32_t)0);
    EXPECT_TRUE(strstr(error.message, "trop long") != NULL);
    EXPECT_FALSE(eng_scene_load(&w, path, NULL));
    EXPECT_FALSE(eng_scene_save(&w, path));
    eng_world_shutdown(&w);
}

UTEST(eng_scene, a_defect_after_many_entities_removes_them_all)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 64));
    char text[4096];
    char parent_text[16];
    size_t n = (size_t)SDL_snprintf(text, sizeof text, "openworld-scene 1\n");
    for (int i = 0; i < 30; i++) {
        n += (size_t)SDL_snprintf(text + n, sizeof text - n, "entity %d parent %s\n[Transform]\n",
                                  i, parent_token(i, parent_text, sizeof parent_text));
    }
    n += (size_t)SDL_snprintf(text + n, sizeof text - n, "bad line\n");
    EngSceneError error;
    EXPECT_FALSE(eng_scene_read(&w, text, n, &error));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)0);
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_TRANSFORM), (uint32_t)0);
    EXPECT_EQ(error.line, (uint32_t)(2 + 60));
    eng_world_shutdown(&w);
}

UTEST(eng_scene, a_chain_deeper_than_the_world_allows_is_refused_whole)
{
    char parent_text[16];
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 64));
    char text[4096];
    size_t n = (size_t)SDL_snprintf(text, sizeof text, "openworld-scene 1\n");
    for (uint32_t i = 0; i <= ENG_WORLD_MAX_DEPTH; i++) { /* un niveau de trop */
        n += (size_t)SDL_snprintf(text + n, sizeof text - n, "entity %u parent %s\n", (unsigned)i,
                                  parent_token((int)i, parent_text, sizeof parent_text));
    }
    EngSceneError error;
    EXPECT_FALSE(eng_scene_read(&w, text, n, &error));
    EXPECT_TRUE(strstr(error.message, "trop profonde") != NULL);
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)0);
    /* à la limite exacte : accepté */
    n = (size_t)SDL_snprintf(text, sizeof text, "openworld-scene 1\n");
    for (uint32_t i = 0; i < ENG_WORLD_MAX_DEPTH; i++) {
        n += (size_t)SDL_snprintf(text + n, sizeof text - n, "entity %u parent %s\n", (unsigned)i,
                                  parent_token((int)i, parent_text, sizeof parent_text));
    }
    EXPECT_TRUE(eng_scene_read(&w, text, n, &error));
    EXPECT_EQ(eng_world_entity_count(&w), ENG_WORLD_MAX_DEPTH);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, a_full_world_and_over_long_lines_are_reported)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 2));
    const char *three =
        "openworld-scene 1\nentity 0 parent -\nentity 1 parent -\nentity 2 parent -\n";
    EngSceneError error;
    EXPECT_FALSE(eng_scene_read(&w, three, strlen(three), &error));
    EXPECT_TRUE(strstr(error.message, "monde plein") != NULL);
    EXPECT_EQ(error.line, (uint32_t)4);
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)0);
    const char *two = "openworld-scene 1\nentity 0 parent -\nentity 1 parent -\n";
    EXPECT_TRUE(eng_scene_read(&w, two, strlen(two), &error));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)2);

    char longline[ENG_SCENE_MAX_LINE + 8];
    memset(longline, 'a', sizeof longline);
    memcpy(longline, "# ", 2);
    longline[ENG_SCENE_MAX_LINE] = '\0';
    char text[ENG_SCENE_MAX_LINE + 64];
    const size_t n = (size_t)SDL_snprintf(text, sizeof text, "openworld-scene 1\n%s\n", longline);
    EXPECT_FALSE(eng_scene_read(&w, text, n, &error));
    EXPECT_TRUE(strstr(error.message, "trop longue") != NULL);
    EXPECT_EQ(error.line, (uint32_t)2);
    /* la plus longue ligne admise passe */
    longline[ENG_SCENE_MAX_LINE - 1U] = '\0';
    const size_t m = (size_t)SDL_snprintf(text, sizeof text, "openworld-scene 1\n%s\n", longline);
    EXPECT_TRUE(eng_scene_read(&w, text, m, &error));
    eng_world_shutdown(&w);
}

UTEST(eng_scene, reading_adds_to_a_world_that_already_has_entities)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 8));
    const EngEntity old = eng_world_create(&w, "ancienne");
    const char *text = "openworld-scene 1\nentity 0 parent -\n[Name]\ntext = \"nouvelle\"\n";
    EngSceneError error;
    ASSERT_TRUE(eng_scene_read(&w, text, strlen(text), &error));
    ASSERT_TRUE(eng_scene_read(&w, text, strlen(text), &error));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)3);
    EXPECT_TRUE(eng_world_alive(&w, old));
    EXPECT_TRUE(eng_scene_read(&w, text, strlen(text), NULL)); /* error optionnel */
    EXPECT_FALSE(eng_scene_read(&w, "x", 1, NULL));
    eng_world_shutdown(&w);
}

UTEST(eng_scene, a_value_that_cannot_be_written_stops_the_writer)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const EngEntity e = eng_world_create(&w, NULL);
    EngTransform *t = eng_world_add(&w, e, ENG_COMPONENT_TRANSFORM);
    t->position[1] = INFINITY;
    size_t needed = 0;
    EXPECT_FALSE(eng_scene_write(&w, NULL, 0, &needed));
    EXPECT_TRUE(write_all(&w) == NULL);
    t->position[1] = NAN;
    EXPECT_TRUE(write_all(&w) == NULL);
    t->position[1] = 1.0F;
    char *ok = write_all(&w);
    EXPECT_TRUE(ok != NULL);
    SDL_free(ok);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, components_without_fields_are_runtime_only_and_not_written)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    const EngComponentDesc runtime = {.name = "Runtime", .size = 8};
    const uint32_t id = eng_world_register(&w, &runtime);
    ASSERT_TRUE(id != 0);
    const EngEntity e = eng_world_create(&w, "x");
    ASSERT_TRUE(eng_world_add(&w, e, id) != NULL);
    char *text = write_all(&w);
    ASSERT_TRUE(text != NULL);
    EXPECT_TRUE(strstr(text, "Runtime") == NULL);
    SDL_free(text);
    eng_world_shutdown(&w);
}

#define SCENE_FILE "scene_test_file.scene"
#define SCENE_FILE_TMP "scene_test_file.scene.tmp"

UTEST(eng_scene, a_scene_goes_through_a_file_and_leaves_no_temporary_behind)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 8));
    const EngEntity a = eng_world_create(&w, "a");
    const EngEntity b = eng_world_create(&w, "b");
    ASSERT_TRUE(eng_world_set_parent(&w, b, a));
    ((EngTransform *)eng_world_add(&w, b, ENG_COMPONENT_TRANSFORM))->position[2] = 4.5F;

    ASSERT_TRUE(eng_scene_save(&w, SCENE_FILE));
    char tmp[512];
    ASSERT_TRUE(eng_path_resolve(tmp, sizeof tmp, SCENE_FILE_TMP));
    EXPECT_FALSE(eng_file_exists(tmp));
    ASSERT_TRUE(eng_scene_save(&w, SCENE_FILE)); /* écrase l'existant */

    EngWorld back;
    ASSERT_TRUE(make_world(&back, 8));
    EngSceneError error;
    ASSERT_TRUE(eng_scene_load(&back, SCENE_FILE, &error));
    EXPECT_TRUE(same_content(&w, &back));

    char resolved[512];
    ASSERT_TRUE(eng_path_resolve(resolved, sizeof resolved, SCENE_FILE));
    (void)SDL_RemovePath(resolved);
    eng_world_shutdown(&back);
    eng_world_shutdown(&w);
}

UTEST(eng_scene, file_errors_are_reported_without_touching_the_world)
{
    EngWorld w;
    ASSERT_TRUE(make_world(&w, 4));
    EngSceneError error = {.line = 7};
    EXPECT_FALSE(eng_scene_load(&w, "n_existe_pas.scene", &error));
    EXPECT_EQ(error.line, (uint32_t)0);
    EXPECT_TRUE(error.message[0] != '\0');
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)0);
    EXPECT_FALSE(eng_scene_save(&w, "dossier_inexistant/x.scene"));
    EXPECT_FALSE(eng_scene_load(&w, "n_existe_pas.scene", NULL));
    eng_world_shutdown(&w);
}
