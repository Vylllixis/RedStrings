#include <math.h>
#include <SDL3/SDL_stdinc.h>
#include <string.h>

#include "engine/world.h"
#include "utest.h"

/* Monde d'entités (M2-08) : handles, composants (tables denses), hiérarchie, matrices monde. */

#define EXPECT_NEAR_F(a, b) EXPECT_TRUE(fabsf((a) - (b)) < 1e-5F)

typedef struct Health {
    int32_t hp;
    float regen;
    char tag[8];
    bool alive;
} Health;

static const EngFieldDesc HEALTH_FIELDS[] = {
    {"hp", ENG_FIELD_I32, offsetof(Health, hp), 0},
    {"regen", ENG_FIELD_FLOAT, offsetof(Health, regen), 0},
    {"tag", ENG_FIELD_STRING, offsetof(Health, tag), 8},
    {"alive", ENG_FIELD_BOOL, offsetof(Health, alive), 0},
};

static const Health HEALTH_INITIAL = {.hp = 100, .regen = 0.5F, .alive = true};

static EngComponentDesc health_desc(void)
{
    return (EngComponentDesc){.name = "Health",
                              .size = sizeof(Health),
                              .fields = HEALTH_FIELDS,
                              .field_count = 4,
                              .initial = &HEALTH_INITIAL};
}

static bool same(EngEntity a, EngEntity b)
{
    return a.index == b.index && a.generation == b.generation;
}

UTEST(eng_world, native_components_exist_with_their_fields)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    EXPECT_EQ(eng_world_component_count(&w), (uint32_t)3);
    EXPECT_EQ(eng_world_find_component(&w, "Name"), (uint32_t)ENG_COMPONENT_NAME);
    EXPECT_EQ(eng_world_find_component(&w, "Transform"), (uint32_t)ENG_COMPONENT_TRANSFORM);
    EXPECT_EQ(eng_world_find_component(&w, "MeshRenderer"), (uint32_t)ENG_COMPONENT_MESH_RENDERER);
    EXPECT_EQ(eng_world_find_component(&w, "Nope"), (uint32_t)0);
    EXPECT_EQ(eng_world_find_component(&w, NULL), (uint32_t)0);

    const EngComponentDesc *t = eng_world_component(&w, ENG_COMPONENT_TRANSFORM);
    ASSERT_TRUE(t != NULL);
    EXPECT_EQ(t->size, (uint32_t)sizeof(EngTransform));
    ASSERT_EQ(t->field_count, (uint32_t)3);
    EXPECT_STREQ(t->fields[0].name, "position");
    EXPECT_EQ(t->fields[1].type, ENG_FIELD_QUAT);
    EXPECT_TRUE(eng_world_component(&w, 0) == NULL);
    EXPECT_TRUE(eng_world_component(&w, 4) == NULL);
    eng_world_shutdown(&w);
}

UTEST(eng_world, entities_are_handles_and_destroyed_ones_are_detected)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 4));
    const EngEntity a = eng_world_create(&w, "a");
    const EngEntity b = eng_world_create(&w, NULL);
    EXPECT_TRUE(eng_world_alive(&w, a));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)2);
    EXPECT_TRUE(eng_world_has(&w, a, ENG_COMPONENT_NAME));
    EXPECT_FALSE(eng_world_has(&w, b, ENG_COMPONENT_NAME)); /* sans nom : pas de composant */

    EXPECT_TRUE(eng_world_destroy(&w, a));
    EXPECT_FALSE(eng_world_alive(&w, a));
    EXPECT_FALSE(eng_world_destroy(&w, a)); /* déjà détruite */
    EXPECT_TRUE(eng_world_get(&w, a, ENG_COMPONENT_NAME) == NULL);
    EXPECT_TRUE(eng_world_add(&w, a, ENG_COMPONENT_TRANSFORM) == NULL);

    /* la case est réutilisée avec une autre génération : l'ancien handle reste périmé */
    const EngEntity c = eng_world_create(&w, "c");
    EXPECT_EQ(c.index, a.index);
    EXPECT_TRUE(c.generation != a.generation);
    EXPECT_FALSE(eng_world_alive(&w, a));
    EXPECT_TRUE(eng_world_get(&w, a, ENG_COMPONENT_NAME) == NULL);
    EXPECT_STREQ(((const EngName *)eng_world_get(&w, c, ENG_COMPONENT_NAME))->text, "c");
    EXPECT_FALSE(eng_world_alive(&w, ENG_ENTITY_NULL));
    eng_world_shutdown(&w);
}

UTEST(eng_world, the_world_is_bounded_and_says_so)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 3));
    EngEntity e[3];
    for (int i = 0; i < 3; i++) {
        e[i] = eng_world_create(&w, NULL);
        ASSERT_TRUE(e[i].generation != 0);
    }
    EXPECT_TRUE(eng_world_create(&w, "de trop").generation == 0);
    EXPECT_TRUE(eng_world_destroy(&w, e[1]));
    EXPECT_TRUE(eng_world_create(&w, "ok").generation != 0);
    eng_world_shutdown(&w);

    EngWorld bad;
    EXPECT_FALSE(eng_world_init(&bad, 0));
    EXPECT_FALSE(eng_world_init(&bad, (1U << 20) + 1U));
    EXPECT_TRUE(eng_world_create(&bad, NULL).generation == 0); /* non initialisé : refus propre */
    eng_world_shutdown(&bad);
}

UTEST(eng_world, defaults_and_add_semantics)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 4));
    const EngEntity e = eng_world_create(&w, NULL);
    EngTransform *t = eng_world_add(&w, e, ENG_COMPONENT_TRANSFORM);
    ASSERT_TRUE(t != NULL);
    EXPECT_NEAR_F(t->scale[0], 1.0F);
    EXPECT_NEAR_F(t->scale[2], 1.0F);
    EXPECT_NEAR_F(t->rotation[3], 1.0F);
    EXPECT_NEAR_F(t->position[1], 0.0F);
    t->position[0] = 7.0F;
    EXPECT_TRUE(eng_world_add(&w, e, ENG_COMPONENT_TRANSFORM) ==
                t); /* déjà là : données conservées */
    EXPECT_NEAR_F(t->position[0], 7.0F);

    const EngMeshRenderer *m = eng_world_add(&w, e, ENG_COMPONENT_MESH_RENDERER);
    ASSERT_TRUE(m != NULL);
    EXPECT_TRUE(m->visible);
    EXPECT_TRUE(m->cast_shadow);
    EXPECT_EQ(m->model[0], '\0');
    EXPECT_TRUE(m->asset.generation == 0);

    EXPECT_TRUE(eng_world_add(&w, e, 99) == NULL); /* composant inconnu */
    EXPECT_TRUE(eng_world_get(&w, e, 0) == NULL);
    EXPECT_FALSE(eng_world_remove(&w, e, 99));
    EXPECT_TRUE(eng_world_remove(&w, e, ENG_COMPONENT_TRANSFORM));
    EXPECT_FALSE(eng_world_remove(&w, e, ENG_COMPONENT_TRANSFORM));
    EXPECT_FALSE(eng_world_has(&w, e, ENG_COMPONENT_TRANSFORM));
    eng_world_shutdown(&w);
}

UTEST(eng_world, tables_stay_dense_when_components_are_removed)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 16));
    const EngComponentDesc desc = health_desc();
    const uint32_t health = eng_world_register(&w, &desc);
    ASSERT_EQ(health, (uint32_t)4);

    EngEntity e[5];
    for (int i = 0; i < 5; i++) {
        e[i] = eng_world_create(&w, NULL);
        Health *h = eng_world_add(&w, e[i], health);
        ASSERT_TRUE(h != NULL);
        EXPECT_EQ(h->hp, 100); /* valeur initiale de la description */
        EXPECT_TRUE(h->alive);
        h->hp = 10 * (i + 1);
    }
    EXPECT_EQ(eng_world_table_count(&w, health), (uint32_t)5);

    EXPECT_TRUE(eng_world_remove(&w, e[1], health)); /* le dernier prend sa place */
    EXPECT_EQ(eng_world_table_count(&w, health), (uint32_t)4);
    for (int i = 0; i < 5; i++) {
        const Health *h = eng_world_get(&w, e[i], health);
        if (i == 1) {
            EXPECT_TRUE(h == NULL);
        } else {
            ASSERT_TRUE(h != NULL);
            EXPECT_EQ(h->hp, 10 * (i + 1)); /* chacun garde ses données */
        }
    }
    /* parcours : chaque entité du tableau possède bien l'élément à son indice */
    for (uint32_t i = 0; i < eng_world_table_count(&w, health); i++) {
        const EngEntity owner = eng_world_table_entity(&w, health, i);
        EXPECT_TRUE(eng_world_table_data(&w, health, i) == eng_world_get(&w, owner, health));
    }
    EXPECT_TRUE(eng_world_table_data(&w, health, 4) == NULL); /* hors table */
    EXPECT_TRUE(eng_world_table_entity(&w, health, 4).generation == 0);
    EXPECT_EQ(eng_world_table_count(&w, 99), (uint32_t)0);

    /* détruire une entité retire ses composants de toutes les tables */
    EXPECT_TRUE(eng_world_destroy(&w, e[0]));
    EXPECT_EQ(eng_world_table_count(&w, health), (uint32_t)3);
    EXPECT_EQ(eng_world_get(&w, e[4], health) != NULL, true);
    eng_world_shutdown(&w);
}

UTEST(eng_world, three_level_hierarchy_composes_transforms)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    const EngEntity root = eng_world_create(&w, "racine");
    const EngEntity mid = eng_world_create(&w, "milieu");
    const EngEntity leaf = eng_world_create(&w, "feuille");
    ASSERT_TRUE(eng_world_set_parent(&w, mid, root));
    ASSERT_TRUE(eng_world_set_parent(&w, leaf, mid));

    /* racine : position (10,0,0), rotation de 90° autour de Y, échelle 2 */
    EngTransform *tr = eng_world_add(&w, root, ENG_COMPONENT_TRANSFORM);
    const float half = sqrtf(0.5F);
    tr->position[0] = 10.0F;
    tr->rotation[1] = half;
    tr->rotation[3] = half;
    tr->scale[0] = tr->scale[1] = tr->scale[2] = 2.0F;
    EngTransform *tm = eng_world_add(&w, mid, ENG_COMPONENT_TRANSFORM);
    tm->position[2] = 5.0F;
    EngTransform *tl = eng_world_add(&w, leaf, ENG_COMPONENT_TRANSFORM);
    tl->position[0] = 1.0F;

    EXPECT_EQ(eng_world_depth(&w, root), (uint32_t)0);
    EXPECT_EQ(eng_world_depth(&w, mid), (uint32_t)1);
    EXPECT_EQ(eng_world_depth(&w, leaf), (uint32_t)2);

    mat4 m;
    ASSERT_TRUE(eng_world_world_matrix(&w, leaf, m));
    /* l'origine de la feuille : (1,0,5) dans le repère de la racine, x2 -> (2,0,10), tourné de 90°
     * autour de Y (x' = z, z' = -x) -> (10,0,-2), plus la translation -> (20,0,-2) */
    EXPECT_NEAR_F(m[3][0], 20.0F);
    EXPECT_NEAR_F(m[3][1], 0.0F);
    EXPECT_NEAR_F(m[3][2], -2.0F);
    /* axes : X du monde-objet = (0,0,-2), Y = (0,2,0), Z = (2,0,0) */
    EXPECT_NEAR_F(m[0][0], 0.0F);
    EXPECT_NEAR_F(m[0][2], -2.0F);
    EXPECT_NEAR_F(m[1][1], 2.0F);
    EXPECT_NEAR_F(m[2][0], 2.0F);
    EXPECT_NEAR_F(m[2][2], 0.0F);

    /* le milieu seul : (0,0,5) -> x2 -> (0,0,10) -> tourné -> (10,0,0) -> +10 -> (20,0,0) */
    ASSERT_TRUE(eng_world_world_matrix(&w, mid, m));
    EXPECT_NEAR_F(m[3][0], 20.0F);
    EXPECT_NEAR_F(m[3][2], 0.0F);

    /* bouger la racine déplace toute la chaîne ; la feuille n'a pas changé de transformation locale
     */
    tr->position[1] = 3.0F;
    ASSERT_TRUE(eng_world_world_matrix(&w, leaf, m));
    EXPECT_NEAR_F(m[3][1], 3.0F);
    EXPECT_NEAR_F(tl->position[0], 1.0F);
    eng_world_shutdown(&w);
}

UTEST(eng_world, an_entity_without_transform_is_transparent_in_the_chain)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    const EngEntity root = eng_world_create(&w, NULL);
    const EngEntity group = eng_world_create(&w, NULL); /* pas de Transform : identité */
    const EngEntity leaf = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, group, root));
    ASSERT_TRUE(eng_world_set_parent(&w, leaf, group));
    ((EngTransform *)eng_world_add(&w, root, ENG_COMPONENT_TRANSFORM))->position[0] = 4.0F;
    ((EngTransform *)eng_world_add(&w, leaf, ENG_COMPONENT_TRANSFORM))->position[1] = 6.0F;
    mat4 m;
    ASSERT_TRUE(eng_world_world_matrix(&w, leaf, m));
    EXPECT_NEAR_F(m[3][0], 4.0F);
    EXPECT_NEAR_F(m[3][1], 6.0F);
    ASSERT_TRUE(eng_world_world_matrix(&w, group, m)); /* aucun Transform nulle part : identité */
    EXPECT_NEAR_F(m[3][0], 4.0F);

    EXPECT_FALSE(eng_world_world_matrix(&w, ENG_ENTITY_NULL, m)); /* invalide : false et identité */
    EXPECT_NEAR_F(m[0][0], 1.0F);
    EXPECT_NEAR_F(m[3][0], 0.0F);
    eng_world_shutdown(&w);
}

UTEST(eng_world, a_non_unit_quaternion_is_normalized)
{
    EngTransform t = {.position = {0, 0, 0}, .rotation = {0, 2.0F, 0, 2.0F}, .scale = {1, 1, 1}};
    mat4 m;
    eng_transform_matrix(&t, m); /* (0, 2, 0, 2) est 90° autour de Y, non normalisé */
    EXPECT_NEAR_F(m[0][2], -1.0F);
    EXPECT_NEAR_F(m[2][0], 1.0F);
    EXPECT_NEAR_F(m[1][1], 1.0F);
}

/* Les enfants d'`parent`, dans l'ordre ; rend leur nombre. */
static uint32_t children(const EngWorld *w, EngEntity parent, EngEntity *out, uint32_t max)
{
    uint32_t n = 0;
    for (EngEntity c = eng_world_first_child(w, parent); c.generation != 0;
         c = eng_world_next_sibling(w, c)) {
        if (n < max) {
            out[n] = c;
        }
        n++;
    }
    return n;
}

UTEST(eng_world, children_are_kept_in_order_and_reparenting_updates_them)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 16));
    const EngEntity p = eng_world_create(&w, "p");
    const EngEntity q = eng_world_create(&w, "q");
    EngEntity c[4];
    for (int i = 0; i < 4; i++) {
        c[i] = eng_world_create(&w, NULL);
        ASSERT_TRUE(eng_world_set_parent(&w, c[i], p));
        EXPECT_TRUE(same(eng_world_parent(&w, c[i]), p));
    }
    EngEntity list[8];
    ASSERT_EQ(children(&w, p, list, 8), (uint32_t)4);
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(same(list[i], c[i])); /* ordre d'ajout */
    }

    ASSERT_TRUE(eng_world_set_parent(&w, c[1], q)); /* un du milieu part chez q */
    ASSERT_EQ(children(&w, p, list, 8), (uint32_t)3);
    EXPECT_TRUE(same(list[0], c[0]) && same(list[1], c[2]) && same(list[2], c[3]));
    ASSERT_EQ(children(&w, q, list, 8), (uint32_t)1);
    EXPECT_TRUE(same(list[0], c[1]));

    ASSERT_TRUE(eng_world_set_parent(&w, c[0], ENG_ENTITY_NULL)); /* le premier devient racine */
    EXPECT_TRUE(eng_world_parent(&w, c[0]).generation == 0);
    ASSERT_EQ(children(&w, p, list, 8), (uint32_t)2);
    EXPECT_TRUE(same(list[0], c[2]) && same(list[1], c[3]));
    ASSERT_TRUE(eng_world_set_parent(&w, c[3], q)); /* le dernier part aussi */
    ASSERT_EQ(children(&w, p, list, 8), (uint32_t)1);
    ASSERT_TRUE(eng_world_set_parent(&w, c[2], ENG_ENTITY_NULL)); /* le seul enfant restant */
    EXPECT_EQ(children(&w, p, list, 8), (uint32_t)0);
    EXPECT_TRUE(eng_world_first_child(&w, p).generation == 0);
    /* p accepte de nouveau un enfant (sa liste est cohérente) */
    ASSERT_TRUE(eng_world_set_parent(&w, c[2], p));
    ASSERT_EQ(children(&w, p, list, 8), (uint32_t)1);
    ASSERT_EQ(children(&w, q, list, 8), (uint32_t)2);
    EXPECT_TRUE(same(list[0], c[1]) && same(list[1], c[3]));

    /* rattacher à son parent actuel : sans effet visible, toujours un seul exemplaire */
    ASSERT_TRUE(eng_world_set_parent(&w, c[2], p));
    EXPECT_EQ(children(&w, p, list, 8), (uint32_t)1);
    eng_world_shutdown(&w);
}

UTEST(eng_world, depths_follow_reparenting_of_whole_subtrees)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    const EngEntity a = eng_world_create(&w, NULL);
    const EngEntity b = eng_world_create(&w, NULL);
    const EngEntity c = eng_world_create(&w, NULL);
    const EngEntity d = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, c, b));
    ASSERT_TRUE(eng_world_set_parent(&w, d, c)); /* b > c > d */
    EXPECT_EQ(eng_world_depth(&w, d), (uint32_t)2);
    ASSERT_TRUE(eng_world_set_parent(&w, b, a)); /* a > b > c > d */
    EXPECT_EQ(eng_world_depth(&w, b), (uint32_t)1);
    EXPECT_EQ(eng_world_depth(&w, c), (uint32_t)2);
    EXPECT_EQ(eng_world_depth(&w, d), (uint32_t)3);
    ASSERT_TRUE(eng_world_set_parent(&w, b, ENG_ENTITY_NULL));
    EXPECT_EQ(eng_world_depth(&w, b), (uint32_t)0);
    EXPECT_EQ(eng_world_depth(&w, d), (uint32_t)2);
    EXPECT_EQ(eng_world_depth(&w, ENG_ENTITY_NULL), (uint32_t)0);
    eng_world_shutdown(&w);
}

UTEST(eng_world, loops_invalid_parents_and_excess_depth_are_refused)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 64));
    const EngEntity a = eng_world_create(&w, NULL);
    const EngEntity b = eng_world_create(&w, NULL);
    const EngEntity c = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, b, a));
    ASSERT_TRUE(eng_world_set_parent(&w, c, b));
    EXPECT_FALSE(eng_world_set_parent(&w, a, a)); /* soi-même */
    EXPECT_FALSE(eng_world_set_parent(&w, a, c)); /* un descendant : boucle */
    EXPECT_FALSE(eng_world_set_parent(&w, a, b));
    EXPECT_TRUE(eng_world_parent(&w, a).generation == 0); /* rien n'a bougé */
    EXPECT_TRUE(same(eng_world_parent(&w, c), b));

    const EngEntity dead = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_destroy(&w, dead));
    EXPECT_FALSE(eng_world_set_parent(&w, a, dead));
    EXPECT_FALSE(eng_world_set_parent(&w, dead, a));
    EXPECT_FALSE(eng_world_set_parent(&w, ENG_ENTITY_NULL, a));

    /* profondeur : niveaux 0 à ENG_WORLD_MAX_DEPTH - 1 */
    EngEntity chain[ENG_WORLD_MAX_DEPTH + 1U];
    for (uint32_t i = 0; i < ENG_WORLD_MAX_DEPTH; i++) {
        chain[i] = eng_world_create(&w, NULL);
        if (i > 0) {
            ASSERT_TRUE(eng_world_set_parent(&w, chain[i], chain[i - 1U]));
        }
    }
    EXPECT_EQ(eng_world_depth(&w, chain[ENG_WORLD_MAX_DEPTH - 1U]), ENG_WORLD_MAX_DEPTH - 1U);
    chain[ENG_WORLD_MAX_DEPTH] = eng_world_create(&w, NULL);
    EXPECT_FALSE(
        eng_world_set_parent(&w, chain[ENG_WORLD_MAX_DEPTH], chain[ENG_WORLD_MAX_DEPTH - 1U]));
    EXPECT_TRUE(eng_world_parent(&w, chain[ENG_WORLD_MAX_DEPTH]).generation == 0);
    /* un sous-arbre qui, rattaché, dépasserait la limite : refusé, même si sa racine seule
     * passerait */
    EXPECT_FALSE(
        eng_world_set_parent(&w, a, chain[ENG_WORLD_MAX_DEPTH - 3U])); /* a > b > c : 3 niveaux */
    EXPECT_TRUE(
        eng_world_set_parent(&w, a, chain[ENG_WORLD_MAX_DEPTH - 4U])); /* juste assez de place */
    EXPECT_EQ(eng_world_depth(&w, c), ENG_WORLD_MAX_DEPTH - 1U);

    /* la matrice monde d'une chaîne maximale se calcule */
    ((EngTransform *)eng_world_add(&w, chain[0], ENG_COMPONENT_TRANSFORM))->position[0] = 1.0F;
    mat4 m;
    EXPECT_TRUE(eng_world_world_matrix(&w, chain[ENG_WORLD_MAX_DEPTH - 1U], m));
    EXPECT_NEAR_F(m[3][0], 1.0F);
    eng_world_shutdown(&w);
}

UTEST(eng_world, destroying_an_entity_destroys_its_subtree_and_unlinks_it)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 16));
    const EngEntity root = eng_world_create(&w, "root");
    const EngEntity left = eng_world_create(&w, "left");
    const EngEntity right = eng_world_create(&w, "right");
    const EngEntity gl = eng_world_create(&w, "gl");
    const EngEntity gr = eng_world_create(&w, "gr");
    const EngEntity other = eng_world_create(&w, "other");
    ASSERT_TRUE(eng_world_set_parent(&w, left, root));
    ASSERT_TRUE(eng_world_set_parent(&w, right, root));
    ASSERT_TRUE(eng_world_set_parent(&w, gl, left));
    ASSERT_TRUE(eng_world_set_parent(&w, gr, right));
    for (uint32_t i = 0; i < eng_world_entity_count(&w); i++) {
        (void)i;
    }
    EngEntity e;
    uint32_t cursor = 0;
    while (eng_world_next_entity(&w, &cursor, &e)) {
        (void)eng_world_add(&w, e, ENG_COMPONENT_TRANSFORM);
    }
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_TRANSFORM), (uint32_t)6);

    /* détruire `left` : son enfant part avec lui ; `root` garde `right` comme seul enfant */
    ASSERT_TRUE(eng_world_destroy(&w, left));
    EXPECT_FALSE(eng_world_alive(&w, left));
    EXPECT_FALSE(eng_world_alive(&w, gl));
    EXPECT_TRUE(eng_world_alive(&w, gr));
    EngEntity list[4];
    ASSERT_EQ(children(&w, root, list, 4), (uint32_t)1);
    EXPECT_TRUE(same(list[0], right));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)4);
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_TRANSFORM), (uint32_t)4);
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_NAME), (uint32_t)4);

    /* détruire la racine : tout son sous-arbre, rien d'autre */
    ASSERT_TRUE(eng_world_destroy(&w, root));
    EXPECT_FALSE(eng_world_alive(&w, right));
    EXPECT_FALSE(eng_world_alive(&w, gr));
    EXPECT_TRUE(eng_world_alive(&w, other));
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)1);
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_TRANSFORM), (uint32_t)1);
    EXPECT_EQ(eng_world_table_count(&w, ENG_COMPONENT_NAME), (uint32_t)1);

    /* le monde reste utilisable : toutes les cases sont rendues */
    for (int i = 0; i < 15; i++) {
        EXPECT_TRUE(eng_world_create(&w, NULL).generation != 0);
    }
    eng_world_shutdown(&w);
}

UTEST(eng_world, iteration_tolerates_destroying_the_current_entity)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    for (int i = 0; i < 6; i++) {
        (void)eng_world_create(&w, NULL);
    }
    uint32_t visited = 0;
    EngEntity e;
    uint32_t cursor = 0;
    while (eng_world_next_entity(&w, &cursor, &e)) {
        visited++;
        EXPECT_TRUE(eng_world_destroy(&w, e));
    }
    EXPECT_EQ(visited, (uint32_t)6);
    EXPECT_EQ(eng_world_entity_count(&w), (uint32_t)0);
    eng_world_shutdown(&w);
}

UTEST(eng_world, names_can_be_searched_and_are_truncated)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 8));
    const EngEntity a = eng_world_create(&w, "arbre");
    const EngEntity b = eng_world_create(&w, "rocher");
    EXPECT_TRUE(same(eng_world_find_by_name(&w, "rocher"), b));
    EXPECT_TRUE(same(eng_world_find_by_name(&w, "arbre"), a));
    EXPECT_TRUE(eng_world_find_by_name(&w, "arbr").generation == 0); /* comparaison exacte */
    EXPECT_TRUE(eng_world_find_by_name(&w, "").generation == 0);
    EXPECT_TRUE(eng_world_find_by_name(&w, NULL).generation == 0);
    EXPECT_TRUE(eng_world_destroy(&w, b));
    EXPECT_TRUE(eng_world_find_by_name(&w, "rocher").generation == 0);

    char long_name[200];
    memset(long_name, 'x', sizeof long_name);
    long_name[sizeof long_name - 1U] = '\0';
    const EngEntity c = eng_world_create(&w, long_name);
    const EngName *name = eng_world_get(&w, c, ENG_COMPONENT_NAME);
    ASSERT_TRUE(name != NULL);
    EXPECT_EQ(strlen(name->text), (size_t)ENG_NAME_MAX - 1U);
    EXPECT_TRUE(same(eng_world_find_by_name(&w, name->text), c));
    eng_world_shutdown(&w);
}

UTEST(eng_world, component_descriptions_are_validated)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 4));
    const EngComponentDesc ok = health_desc();
    EXPECT_EQ(eng_world_register(&w, &ok), (uint32_t)4);
    EXPECT_EQ(eng_world_register(&w, &ok), (uint32_t)0); /* nom déjà pris */
    EXPECT_EQ(eng_world_register(&w, NULL), (uint32_t)0);

    EngComponentDesc d = ok;
    d.name = "";
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    d = ok;
    d.name = "Autre";
    d.size = 0;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    d.size = ENG_WORLD_MAX_COMPONENT_SIZE + 1U;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);

    /* un champ qui dépasse la structure */
    const EngFieldDesc past[] = {{"hp", ENG_FIELD_I32, (uint32_t)sizeof(Health) - 2U, 0}};
    d = ok;
    d.name = "Past";
    d.fields = past;
    d.field_count = 1;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* un champ mal aligné */
    const EngFieldDesc misaligned[] = {{"hp", ENG_FIELD_I32, 2, 0}};
    d.name = "Misaligned";
    d.fields = misaligned;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* chaîne sans longueur ; longueur sur un autre type ; type inconnu */
    const EngFieldDesc no_length[] = {{"tag", ENG_FIELD_STRING, 8, 0}};
    d.name = "NoLength";
    d.fields = no_length;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    const EngFieldDesc stray_length[] = {{"hp", ENG_FIELD_I32, 0, 4}};
    d.name = "StrayLength";
    d.fields = stray_length;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const EngFieldDesc unknown[] = {{"hp", (EngFieldType)99, 0, 0}};
    d.name = "Unknown";
    d.fields = unknown;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* nom de champ vide ou répété */
    const EngFieldDesc unnamed[] = {{"", ENG_FIELD_I32, 0, 0}};
    d.name = "Unnamed";
    d.fields = unnamed;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    const EngFieldDesc twice[] = {{"hp", ENG_FIELD_I32, 0, 0}, {"hp", ENG_FIELD_FLOAT, 4, 0}};
    d.name = "Twice";
    d.fields = twice;
    d.field_count = 2;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* champs annoncés mais absents */
    d.name = "Missing";
    d.fields = NULL;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    d.name = "TooMany";
    d.fields = HEALTH_FIELDS;
    d.field_count = ENG_WORLD_MAX_FIELDS + 1U;
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);

    /* sans champ : valide (état d'exécution pur) */
    d = (EngComponentDesc){.name = "Tag", .size = 4};
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)5);
    /* la limite du nombre de composants */
    char names[ENG_WORLD_MAX_COMPONENTS][16];
    uint32_t registered = eng_world_component_count(&w);
    for (uint32_t i = registered; i < ENG_WORLD_MAX_COMPONENTS; i++) {
        SDL_snprintf(names[i], sizeof names[i], "C%u", (unsigned)i);
        d = (EngComponentDesc){.name = names[i], .size = 4};
        EXPECT_EQ(eng_world_register(&w, &d), i + 1U);
    }
    d = (EngComponentDesc){.name = "Dernier", .size = 4};
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    EXPECT_EQ(eng_world_component_count(&w), ENG_WORLD_MAX_COMPONENTS);
    eng_world_shutdown(&w);
}

UTEST(eng_world, field_sizes)
{
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_BOOL, 0, 0}), (uint32_t)1);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_I32, 0, 0}), (uint32_t)4);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_U32, 0, 0}), (uint32_t)4);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_FLOAT, 0, 0}), (uint32_t)4);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_VEC3, 0, 0}), (uint32_t)12);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_QUAT, 0, 0}), (uint32_t)16);
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", ENG_FIELD_STRING, 0, 24}), (uint32_t)24);
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    EXPECT_EQ(eng_field_size(&(EngFieldDesc){"a", (EngFieldType)0, 0, 0}), (uint32_t)0);
}

UTEST(eng_world, description_limits_are_inclusive_and_offsets_checked_against_the_size)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 4));
    /* taille maximale exacte : acceptée ; un octet de plus : refusée */
    EngComponentDesc d = {.name = "Big", .size = ENG_WORLD_MAX_COMPONENT_SIZE};
    EXPECT_TRUE(eng_world_register(&w, &d) != 0);
    d = (EngComponentDesc){.name = "TooBig", .size = ENG_WORLD_MAX_COMPONENT_SIZE + 1U};
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);

    /* exactement ENG_WORLD_MAX_FIELDS champs : accepté */
    static char names[ENG_WORLD_MAX_FIELDS][8];
    EngFieldDesc fields[ENG_WORLD_MAX_FIELDS];
    for (uint32_t i = 0; i < ENG_WORLD_MAX_FIELDS; i++) {
        SDL_snprintf(names[i], sizeof names[i], "f%u", (unsigned)i);
        fields[i] = (EngFieldDesc){names[i], ENG_FIELD_U32, i * 4U, 0};
    }
    d = (EngComponentDesc){.name = "Full",
                           .size = ENG_WORLD_MAX_FIELDS * 4U,
                           .fields = fields,
                           .field_count = ENG_WORLD_MAX_FIELDS};
    EXPECT_TRUE(eng_world_register(&w, &d) != 0);

    /* champ entièrement au-delà de la structure (et non seulement à cheval sur la fin) */
    const EngFieldDesc beyond[] = {{"x", ENG_FIELD_U32, 64, 0}};
    d = (EngComponentDesc){.name = "Beyond", .size = 8, .fields = beyond, .field_count = 1};
    EXPECT_EQ(eng_world_register(&w, &d), (uint32_t)0);
    /* champ qui finit exactement à la fin : accepté */
    const EngFieldDesc exact[] = {{"x", ENG_FIELD_U32, 4, 0}};
    d = (EngComponentDesc){.name = "Exact", .size = 8, .fields = exact, .field_count = 1};
    EXPECT_TRUE(eng_world_register(&w, &d) != 0);
    eng_world_shutdown(&w);
}

UTEST(eng_world, the_largest_world_can_be_created)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 1U << 20));
    const EngEntity e = eng_world_create(&w, "x");
    EXPECT_TRUE(e.generation != 0);
    eng_world_shutdown(&w);
}

UTEST(eng_world, branching_subtrees_get_every_depth_updated_and_the_depth_limit_counts_the_deepest)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 64));
    /* r > (a > a1, a2 > a2x) , (b) , (c > c1 > c2) : plusieurs frères à chaque niveau */
    const EngEntity r = eng_world_create(&w, NULL);
    const EngEntity a = eng_world_create(&w, NULL);
    const EngEntity b = eng_world_create(&w, NULL);
    const EngEntity c = eng_world_create(&w, NULL);
    const EngEntity a1 = eng_world_create(&w, NULL);
    const EngEntity a2 = eng_world_create(&w, NULL);
    const EngEntity a2x = eng_world_create(&w, NULL);
    const EngEntity c1 = eng_world_create(&w, NULL);
    const EngEntity c2 = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, a, r));
    ASSERT_TRUE(eng_world_set_parent(&w, b, r));
    ASSERT_TRUE(eng_world_set_parent(&w, c, r));
    ASSERT_TRUE(eng_world_set_parent(&w, a1, a));
    ASSERT_TRUE(eng_world_set_parent(&w, a2, a));
    ASSERT_TRUE(eng_world_set_parent(&w, a2x, a2));
    ASSERT_TRUE(eng_world_set_parent(&w, c1, c));
    ASSERT_TRUE(eng_world_set_parent(&w, c2, c1));
    const EngEntity all[] = {r, a, b, c, a1, a2, a2x, c1, c2};
    const uint32_t expected[] = {0, 1, 1, 1, 2, 2, 3, 2, 3};
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) {
        EXPECT_EQ(eng_world_depth(&w, all[i]), expected[i]);
    }

    /* tout le sous-arbre descend de deux niveaux quand `r` est rattaché sous `top > mid` */
    const EngEntity top = eng_world_create(&w, NULL);
    const EngEntity mid = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, mid, top));
    ASSERT_TRUE(eng_world_set_parent(&w, r, mid));
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) {
        EXPECT_EQ(eng_world_depth(&w, all[i]), expected[i] + 2U);
    }

    /* la profondeur du sous-arbre (3 sous `r`) compte, pas celle de ses autres branches : placé à
     * la limite, `r` (hauteur 3) ne rentre pas sous une chaîne de ENG_WORLD_MAX_DEPTH - 3 niveaux.
     */
    EngEntity chain[ENG_WORLD_MAX_DEPTH];
    for (uint32_t i = 0; i < ENG_WORLD_MAX_DEPTH - 3U; i++) {
        chain[i] = eng_world_create(&w, NULL);
        if (i > 0) {
            ASSERT_TRUE(eng_world_set_parent(&w, chain[i], chain[i - 1U]));
        }
    }
    /* la dernière de la chaîne est au niveau MAX - 4 ; r au niveau MAX - 3 aurait des feuilles au
     * niveau MAX : refusé ; un cran plus haut : accepté */
    EXPECT_FALSE(eng_world_set_parent(&w, r, chain[ENG_WORLD_MAX_DEPTH - 4U]));
    EXPECT_TRUE(eng_world_set_parent(&w, r, chain[ENG_WORLD_MAX_DEPTH - 5U]));
    EXPECT_EQ(eng_world_depth(&w, a2x), ENG_WORLD_MAX_DEPTH - 5U + 1U + 3U);
    eng_world_shutdown(&w);
}

UTEST(eng_world, subtree_traversal_is_depth_first_and_stays_inside_the_subtree)
{
    EngWorld w;
    ASSERT_TRUE(eng_world_init(&w, 16));
    /* r > (a > (a1, a2), b) ; s (voisine de r, ne doit jamais être visitée) */
    const EngEntity r = eng_world_create(&w, NULL);
    const EngEntity a = eng_world_create(&w, NULL);
    const EngEntity b = eng_world_create(&w, NULL);
    const EngEntity a1 = eng_world_create(&w, NULL);
    const EngEntity a2 = eng_world_create(&w, NULL);
    const EngEntity s = eng_world_create(&w, NULL);
    ASSERT_TRUE(eng_world_set_parent(&w, a, r));
    ASSERT_TRUE(eng_world_set_parent(&w, b, r));
    ASSERT_TRUE(eng_world_set_parent(&w, a1, a));
    ASSERT_TRUE(eng_world_set_parent(&w, a2, a));

    const EngEntity expected[] = {r, a, a1, a2, b};
    EngEntity e = r;
    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; i++) {
        ASSERT_TRUE(same(e, expected[i]));
        e = eng_world_next_in_subtree(&w, r, e);
    }
    EXPECT_TRUE(e.generation == 0);

    /* depuis un sous-arbre plus petit : s'arrête à sa fin, sans passer aux frères de la racine */
    EXPECT_TRUE(same(eng_world_next_in_subtree(&w, a, a), a1));
    EXPECT_TRUE(same(eng_world_next_in_subtree(&w, a, a1), a2));
    EXPECT_TRUE(eng_world_next_in_subtree(&w, a, a2).generation == 0); /* b n'est pas dans a */
    EXPECT_TRUE(eng_world_next_in_subtree(&w, s, s).generation == 0);  /* feuille seule */
    EXPECT_TRUE(eng_world_next_in_subtree(&w, b, b).generation == 0);

    /* arguments invalides, ou entité hors du sous-arbre : fin, sans boucler */
    EXPECT_TRUE(eng_world_next_in_subtree(&w, ENG_ENTITY_NULL, a).generation == 0);
    EXPECT_TRUE(eng_world_next_in_subtree(&w, a, ENG_ENTITY_NULL).generation == 0);
    EXPECT_TRUE(eng_world_next_in_subtree(&w, s, a2).generation == 0);
    EXPECT_TRUE(eng_world_next_in_subtree(&w, a, b).generation == 0);
    ASSERT_TRUE(eng_world_destroy(&w, s));
    EXPECT_TRUE(eng_world_next_in_subtree(&w, s, s).generation == 0); /* détruite */
    eng_world_shutdown(&w);
}
