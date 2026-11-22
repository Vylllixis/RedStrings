#include <SDL3/SDL.h>
#include <string.h>

#include "engine/asset_types.h"
#include "engine/assets.h"
#include "utest.h"

/* Gestionnaire d'assets (M2-05), sans GPU ni fichier : un type de test compte ses chargements et
 * déchargements. Critères d'acceptation : un asset demandé deux fois n'est chargé qu'une fois ;
 * déchargement sans fuite. */

typedef struct Counters {
    int loads;
    int unloads;
    int live; /* payloads actuellement chargés (allocations du chargeur) */
    EngAssets *assets;
    uint32_t type;
    const char *nested; /* si non NULL : le chargeur demande aussi cet asset */
    EngAssetHandle nested_handle;
    bool fail_next;
    bool many_results_ok; /* « manydeps » : 4096 premières déclarations acceptées, la suivante
                             refusée */
    uint32_t many_declared;
} Counters;

typedef struct Payload {
    int value;
    char *owned; /* allocation du chargeur, rendue par unload */
} Payload;

static bool test_load(EngAssetLoad *load, void *payload, const char *path, void *user)
{
    Counters *c = user;
    Payload *p = payload;
    if (c->fail_next) {
        c->fail_next = false;
        return false;
    }
    if (strcmp(path, "refuse") == 0) {
        return false;
    }
    if (strcmp(path, "emptydep") == 0) {
        if (!eng_asset_load_dependency(load, "")) {
            return false;
        }
        c->loads++; /* ne doit pas arriver : une dépendance vide est refusée */
        return true;
    }
    if (strcmp(path, "nulldep") == 0) {
        if (!eng_asset_load_dependency(load, NULL)) {
            return false;
        }
        c->loads++;
        return true;
    }
    if (strcmp(path, "manydeps") == 0) {
        c->many_results_ok = true;
        for (uint32_t i = 0; i < ENG_ASSET_MAX_DEPENDENCIES; i++) {
            char name[24];
            SDL_snprintf(name, sizeof name, "dep%u", (unsigned)i);
            c->many_results_ok = c->many_results_ok && eng_asset_load_dependency(load, name);
        }
        c->many_declared = eng_asset_load_dependency(load, "one-too-many") ? 1U : 0U;
        c->loads++;
        c->live++;
        return true;
    }
    if (!eng_asset_load_dependency(load, path) || !eng_asset_load_dependency(load, "extra.bin") ||
        !eng_asset_load_dependency(load, path)) { /* doublon : ignoré */
        return false;
    }
    if (c->nested != NULL) {
        c->nested_handle = eng_assets_acquire(c->assets, c->type, c->nested);
        if (c->nested_handle.generation == 0) {
            return false;
        }
    }
    p->value = (int)strlen(path);
    p->owned = SDL_malloc(8);
    c->loads++;
    c->live++;
    return true;
}

static void test_unload(void *payload, void *user)
{
    Counters *c = user;
    Payload *p = payload;
    SDL_free(p->owned);
    c->unloads++;
    c->live--;
}

typedef struct Fixture {
    EngAssets assets;
    Counters c;
    uint32_t type;
} Fixture;

static bool setup(Fixture *f, uint32_t capacity)
{
    *f = (Fixture){0};
    if (!eng_assets_init(&f->assets, capacity)) {
        return false;
    }
    f->c.assets = &f->assets;
    f->type = eng_assets_register(&f->assets, &(EngAssetType){.name = "test",
                                                              .payload_size = sizeof(Payload),
                                                              .load = test_load,
                                                              .unload = test_unload,
                                                              .user = &f->c});
    f->c.type = f->type;
    return f->type == 1U;
}

UTEST(eng_assets, same_asset_requested_twice_loads_once)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 8));
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "a.mesh");
    const EngAssetHandle b = eng_assets_acquire(&f.assets, f.type, "a.mesh");
    ASSERT_TRUE(a.generation != 0);
    EXPECT_TRUE(a.index == b.index && a.generation == b.generation);
    EXPECT_EQ(f.c.loads, 1);
    EXPECT_EQ(eng_assets_refs(&f.assets, a), (uint32_t)2);
    const EngAssetStats stats = eng_assets_stats(&f.assets);
    EXPECT_EQ(stats.loads, (uint32_t)1);
    EXPECT_EQ(stats.hits, (uint32_t)1);
    EXPECT_EQ(stats.loaded, (uint32_t)1);
    EXPECT_EQ(((Payload *)eng_assets_get(&f.assets, a))->value, 6);
    EXPECT_STREQ(eng_assets_path(&f.assets, a), "a.mesh");
    EXPECT_EQ(eng_assets_type_of(&f.assets, a), f.type);
    EXPECT_EQ(eng_assets_shutdown(&f.assets),
              (uint32_t)1); /* un asset, deux références jamais rendues */
    EXPECT_EQ(f.c.live, 0);
}

UTEST(eng_assets, last_release_unloads_and_handle_goes_stale)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 8));
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "a");
    const EngAssetHandle again = eng_assets_acquire(&f.assets, f.type, "a");
    EXPECT_TRUE(eng_assets_release(&f.assets, a));
    EXPECT_EQ(f.c.unloads, 0); /* une référence reste */
    EXPECT_TRUE(eng_assets_get(&f.assets, a) != NULL);
    EXPECT_TRUE(eng_assets_release(&f.assets, again));
    EXPECT_EQ(f.c.unloads, 1);
    EXPECT_EQ(f.c.live, 0);
    EXPECT_TRUE(eng_assets_get(&f.assets, a) == NULL); /* périmé */
    EXPECT_FALSE(eng_assets_release(&f.assets, a));    /* double release refusé */
    EXPECT_EQ(eng_assets_refs(&f.assets, a), (uint32_t)0);
    EXPECT_TRUE(eng_assets_path(&f.assets, a) == NULL);
    EXPECT_EQ(eng_assets_stats(&f.assets).loaded, (uint32_t)0);

    /* redemandé : rechargé, avec un handle différent de l'ancien */
    const EngAssetHandle fresh = eng_assets_acquire(&f.assets, f.type, "a");
    EXPECT_EQ(f.c.loads, 2);
    EXPECT_TRUE(fresh.index != a.index || fresh.generation != a.generation);
    EXPECT_TRUE(eng_assets_get(&f.assets, a) == NULL);
    EXPECT_TRUE(eng_assets_release(&f.assets, fresh));
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, distinct_paths_and_types_are_distinct_assets)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 8));
    const uint32_t other =
        eng_assets_register(&f.assets, &(EngAssetType){.name = "autre",
                                                       .payload_size = sizeof(Payload),
                                                       .load = test_load,
                                                       .unload = test_unload,
                                                       .user = &f.c});
    ASSERT_EQ(other, (uint32_t)2);
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "x");
    const EngAssetHandle b = eng_assets_acquire(&f.assets, f.type, "y");
    const EngAssetHandle c =
        eng_assets_acquire(&f.assets, other, "x"); /* même chemin, autre type */
    EXPECT_EQ(f.c.loads, 3);
    EXPECT_TRUE(a.index != b.index && a.index != c.index && b.index != c.index);
    EXPECT_TRUE(eng_assets_release(&f.assets, a));
    EXPECT_TRUE(eng_assets_release(&f.assets, b));
    EXPECT_TRUE(eng_assets_release(&f.assets, c));
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, failed_load_is_not_cached_and_leaves_nothing)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 2));
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "refuse").generation == 0);
    EXPECT_EQ(eng_assets_stats(&f.assets).failures, (uint32_t)1);
    EXPECT_EQ(eng_assets_stats(&f.assets).loaded, (uint32_t)0);
    /* La case est rendue : on peut encore charger `capacity` assets */
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "a");
    const EngAssetHandle b = eng_assets_acquire(&f.assets, f.type, "b");
    EXPECT_TRUE(a.generation != 0 && b.generation != 0);
    /* Plein : refus propre, sans rien charger */
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "c").generation == 0);
    EXPECT_EQ(f.c.loads, 2);
    /* Un échec ponctuel n'est pas mémorisé : le chemin se recharge ensuite */
    EXPECT_TRUE(eng_assets_release(&f.assets, a));
    f.c.fail_next = true;
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "a").generation == 0);
    const EngAssetHandle retry = eng_assets_acquire(&f.assets, f.type, "a");
    EXPECT_TRUE(retry.generation != 0);
    EXPECT_TRUE(eng_assets_release(&f.assets, retry));
    EXPECT_TRUE(eng_assets_release(&f.assets, b));
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, dependencies_are_recorded_without_duplicates)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "m/a.mesh");
    ASSERT_EQ(eng_assets_dependency_count(&f.assets, a), (uint32_t)2);
    EXPECT_STREQ(eng_assets_dependency(&f.assets, a, 0), "m/a.mesh");
    EXPECT_STREQ(eng_assets_dependency(&f.assets, a, 1), "extra.bin");
    EXPECT_TRUE(eng_assets_dependency(&f.assets, a, 2) == NULL);
    EXPECT_TRUE(eng_assets_release(&f.assets, a));
    EXPECT_EQ(eng_assets_dependency_count(&f.assets, a), (uint32_t)0); /* périmé */
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, a_dependency_needs_a_path_and_their_number_is_bounded)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "emptydep").generation == 0);
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "nulldep").generation == 0);
    EXPECT_EQ(f.c.loads, 0);

    const EngAssetHandle many = eng_assets_acquire(&f.assets, f.type, "manydeps");
    ASSERT_TRUE(many.generation != 0);
    EXPECT_TRUE(f.c.many_results_ok);          /* les 4096 premières passent */
    EXPECT_EQ(f.c.many_declared, (uint32_t)0); /* la 4097e est refusée */
    EXPECT_EQ(eng_assets_dependency_count(&f.assets, many), ENG_ASSET_MAX_DEPENDENCIES);
    EXPECT_STREQ(eng_assets_dependency(&f.assets, many, 4095), "dep4095");
    EXPECT_TRUE(eng_assets_release(&f.assets, many));
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, loader_may_acquire_other_assets_but_not_itself)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    f.c.nested = "inner"; /* « outer » charge « inner » : "inner" charge à son tour "inner" */
    const EngAssetHandle outer = eng_assets_acquire(&f.assets, f.type, "outer");
    /* "inner" demande "inner" pendant son propre chargement : refusé, donc "inner" échoue, donc
     * "outer" aussi */
    EXPECT_TRUE(outer.generation == 0);
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_stats(&f.assets).loaded, (uint32_t)0);

    f.c.nested = NULL;
    const EngAssetHandle inner = eng_assets_acquire(&f.assets, f.type, "inner");
    ASSERT_TRUE(inner.generation != 0);
    f.c.nested = "inner";
    /* « outer » charge « inner » (déjà présent : simple référence en plus) */
    const EngAssetHandle outer2 = eng_assets_acquire(&f.assets, f.type, "outer");
    ASSERT_TRUE(outer2.generation != 0);
    EXPECT_EQ(eng_assets_refs(&f.assets, inner), (uint32_t)2);
    EXPECT_TRUE(eng_assets_release(&f.assets, outer2));
    EXPECT_TRUE(eng_assets_release(&f.assets, f.c.nested_handle));
    EXPECT_TRUE(eng_assets_release(&f.assets, inner));
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, retain_adds_a_reference)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "a");
    const EngAssetHandle copy = eng_assets_retain(&f.assets, a);
    EXPECT_TRUE(copy.generation == a.generation && copy.index == a.index);
    EXPECT_EQ(eng_assets_refs(&f.assets, a), (uint32_t)2);
    EXPECT_TRUE(eng_assets_release(&f.assets, a));
    EXPECT_TRUE(eng_assets_release(&f.assets, copy));
    EXPECT_EQ(f.c.unloads, 1);
    EXPECT_TRUE(eng_assets_retain(&f.assets, a).generation == 0); /* périmé */
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

UTEST(eng_assets, invalid_arguments_are_refused)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    EXPECT_TRUE(eng_assets_acquire(&f.assets, 0, "a").generation == 0);
    EXPECT_TRUE(eng_assets_acquire(&f.assets, 9, "a").generation == 0);
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, NULL).generation == 0);
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, "").generation == 0);
    char long_path[ENG_ASSET_PATH_MAX + 1U];
    memset(long_path, 'a', sizeof long_path);
    long_path[ENG_ASSET_PATH_MAX] = '\0';
    EXPECT_TRUE(eng_assets_acquire(&f.assets, f.type, long_path).generation == 0);
    long_path[ENG_ASSET_PATH_MAX - 1U] = '\0'; /* 255 octets : accepté */
    const EngAssetHandle ok = eng_assets_acquire(&f.assets, f.type, long_path);
    EXPECT_TRUE(ok.generation != 0);
    EXPECT_TRUE(eng_assets_release(&f.assets, ok));
    EXPECT_TRUE(eng_assets_get(&f.assets, ENG_HANDLE_NULL) == NULL);
    EXPECT_FALSE(eng_assets_release(&f.assets, ENG_HANDLE_NULL));

    EXPECT_EQ(
        eng_assets_register(&f.assets, &(EngAssetType){.name = "sans chargeur", .payload_size = 4}),
        (uint32_t)0);
    EXPECT_EQ(
        eng_assets_register(&f.assets, &(EngAssetType){.name = "taille nulle", .load = test_load}),
        (uint32_t)0);
    EXPECT_EQ(eng_assets_register(&f.assets, NULL), (uint32_t)0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);

    EngAssets none;
    EXPECT_FALSE(eng_assets_init(&none, 0));
}

UTEST(eng_assets, type_table_is_bounded)
{
    EngAssets assets;
    ASSERT_TRUE(eng_assets_init(&assets, 1));
    Counters c = {0};
    for (uint32_t i = 0; i < ENG_ASSET_MAX_TYPES; i++) {
        EXPECT_EQ(eng_assets_register(&assets, &(EngAssetType){.name = "t",
                                                               .payload_size = sizeof(Payload),
                                                               .load = test_load,
                                                               .user = &c}),
                  i + 1U);
    }
    EXPECT_EQ(eng_assets_register(&assets, &(EngAssetType){.name = "t",
                                                           .payload_size = sizeof(Payload),
                                                           .load = test_load,
                                                           .user = &c}),
              (uint32_t)0);
    EXPECT_EQ(eng_assets_shutdown(&assets), (uint32_t)0);
}

UTEST(eng_assets, shutdown_unloads_everything_and_counts_leaks)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 8));
    const EngAssetHandle a = eng_assets_acquire(&f.assets, f.type, "a");
    const EngAssetHandle b = eng_assets_acquire(&f.assets, f.type, "b");
    EXPECT_TRUE(eng_assets_release(&f.assets, b)); /* b rendu : pas une fuite */
    (void)a;                                       /* a jamais rendu : une fuite */
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)1);
    EXPECT_EQ(f.c.live, 0); /* mais tout est déchargé quand même */
    EXPECT_EQ(f.c.unloads, 2);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0); /* second arrêt : sans effet */
}

/* Beaucoup d'allers-retours : aucun asset ne reste, aucune case ne se perd. */
UTEST(eng_assets, churn_leaves_no_residue)
{
    Fixture f;
    ASSERT_TRUE(setup(&f, 4));
    for (int round = 0; round < 200; round++) {
        char path[16];
        SDL_snprintf(path, sizeof path, "asset%d", round % 7);
        const EngAssetHandle h = eng_assets_acquire(&f.assets, f.type, path);
        ASSERT_TRUE(h.generation != 0);
        EXPECT_TRUE(eng_assets_release(&f.assets, h));
    }
    EXPECT_EQ(f.c.loads, 200);
    EXPECT_EQ(f.c.unloads, 200);
    EXPECT_EQ(f.c.live, 0);
    EXPECT_EQ(eng_assets_stats(&f.assets).loaded, (uint32_t)0);
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
}

/* Le type « modèle » exige un renderer (le chargement proprement dit est testé avec le GPU,
 * test_assets_gpu.c). */
UTEST(eng_assets, the_model_type_requires_a_renderer)
{
    EngAssets assets;
    ASSERT_TRUE(eng_assets_init(&assets, 2));
    EngModelAssetContext without_renderer = {0};
    EXPECT_EQ(eng_model_assets_register(&assets, &without_renderer), (uint32_t)0);
    EXPECT_EQ(eng_model_assets_register(&assets, NULL), (uint32_t)0);
    EXPECT_TRUE(eng_model_asset_gpu(&assets, 0, ENG_HANDLE_NULL) == NULL);
    EXPECT_TRUE(eng_model_asset_gpu(&assets, 1, ENG_HANDLE_NULL) == NULL);
    EXPECT_EQ(eng_assets_shutdown(&assets), (uint32_t)0);
}

/* ── Rechargement (M2-06) : un type « fichier texte » dont le payload est le contenu du fichier ──
 */

typedef struct TextPayload {
    char text[64];
} TextPayload;

typedef struct TextCounters {
    int loads;
    int unloads;
    EngAssets *assets;
    uint32_t type;
    bool acquire_self; /* le chargeur redemande l'asset qu'il charge */
    bool self_refused; /* ... et la demande a été refusée */
} TextCounters;

static bool text_load(EngAssetLoad *load, void *payload, const char *path, void *user)
{
    TextCounters *c = user;
    TextPayload *p = payload;
    size_t size = 0;
    char *data = SDL_LoadFile(path, &size);
    if (data == NULL || size == 0 || size >= sizeof p->text) {
        SDL_free(data);
        return false;
    }
    memcpy(p->text, data, size);
    SDL_free(data);
    if (!eng_asset_load_dependency(load, path)) {
        return false;
    }
    if (c->acquire_self) {
        c->self_refused = eng_assets_acquire(c->assets, c->type, path).generation == 0;
    }
    c->loads++;
    return true;
}

static void text_unload(void *payload, void *user)
{
    (void)payload;
    ((TextCounters *)user)->unloads++;
}

#define HOT_FILE "assets_hot_test.txt"

typedef struct HotFixture {
    EngAssets assets;
    TextCounters c;
    uint32_t type;
    EngAssetHandle handle;
} HotFixture;

static bool write_text(const char *text)
{
    return SDL_SaveFile(HOT_FILE, text, strlen(text));
}

static bool hot_setup(HotFixture *f, const char *initial)
{
    *f = (HotFixture){0};
    if (!write_text(initial) || !eng_assets_init(&f->assets, 4)) {
        return false;
    }
    f->c.assets = &f->assets;
    f->type = eng_assets_register(&f->assets, &(EngAssetType){.name = "texte",
                                                              .payload_size = sizeof(TextPayload),
                                                              .load = text_load,
                                                              .unload = text_unload,
                                                              .user = &f->c});
    f->c.type = f->type;
    f->handle = eng_assets_acquire(&f->assets, f->type, HOT_FILE);
    return f->handle.generation != 0;
}

UTEST(eng_assets_reload, reloads_in_place_keeping_handle_address_and_references)
{
    HotFixture f;
    ASSERT_TRUE(hot_setup(&f, "premier"));
    const EngAssetHandle second = eng_assets_acquire(&f.assets, f.type, HOT_FILE);
    TextPayload *before = eng_assets_get(&f.assets, f.handle);
    ASSERT_TRUE(before != NULL);
    EXPECT_STREQ(before->text, "premier");

    ASSERT_TRUE(write_text("deuxieme version"));
    EXPECT_TRUE(eng_assets_reload(&f.assets, f.handle));
    TextPayload *after = eng_assets_get(&f.assets, f.handle);
    EXPECT_TRUE(after == before); /* même adresse : les pointeurs gardés restent valides */
    EXPECT_STREQ(after->text, "deuxieme version");
    EXPECT_EQ(eng_assets_refs(&f.assets, f.handle), (uint32_t)2);
    EXPECT_EQ(f.c.unloads, 1); /* l'ancienne version est déchargée une fois */
    EXPECT_EQ(f.c.loads, 2);
    EXPECT_EQ(eng_assets_stats(&f.assets).reloads, (uint32_t)1);
    EXPECT_EQ(eng_assets_dependency_count(&f.assets, f.handle), (uint32_t)1);

    EXPECT_TRUE(eng_assets_release(&f.assets, second));
    EXPECT_TRUE(eng_assets_release(&f.assets, f.handle));
    EXPECT_EQ(f.c.unloads, 2); /* aucune version oubliée */
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
    SDL_RemovePath(HOT_FILE);
}

UTEST(eng_assets_reload, a_failed_reload_keeps_the_old_version)
{
    HotFixture f;
    ASSERT_TRUE(hot_setup(&f, "bon"));
    ASSERT_TRUE(SDL_RemovePath(HOT_FILE));
    EXPECT_FALSE(eng_assets_reload(&f.assets, f.handle));
    const TextPayload *p = eng_assets_get(&f.assets, f.handle);
    ASSERT_TRUE(p != NULL);
    EXPECT_STREQ(p->text, "bon");
    EXPECT_EQ(f.c.unloads, 0); /* l'ancienne version n'a pas été touchée */
    EXPECT_EQ(eng_assets_stats(&f.assets).reload_failures, (uint32_t)1);
    EXPECT_EQ(eng_assets_stats(&f.assets).reloads, (uint32_t)0);
    EXPECT_EQ(eng_assets_dependency_count(&f.assets, f.handle), (uint32_t)1);

    /* fichier invalide (vide) : même résultat */
    ASSERT_TRUE(SDL_SaveFile(HOT_FILE, "", 0));
    EXPECT_FALSE(eng_assets_reload(&f.assets, f.handle));
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text, "bon");

    /* l'asset reste rechargeable une fois le fichier réparé */
    ASSERT_TRUE(write_text("repare"));
    EXPECT_TRUE(eng_assets_reload(&f.assets, f.handle));
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text, "repare");
    EXPECT_TRUE(eng_assets_release(&f.assets, f.handle));
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
    SDL_RemovePath(HOT_FILE);
}

UTEST(eng_assets_reload, a_loader_cannot_reacquire_the_asset_it_reloads)
{
    HotFixture f;
    ASSERT_TRUE(hot_setup(&f, "a"));
    f.c.acquire_self = true;
    ASSERT_TRUE(write_text("b"));
    EXPECT_TRUE(eng_assets_reload(&f.assets, f.handle)); /* le refus n'empêche pas le chargement */
    f.c.acquire_self = false;
    EXPECT_TRUE(f.c.self_refused);
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text, "b");
    EXPECT_EQ(eng_assets_refs(&f.assets, f.handle), (uint32_t)1); /* aucune référence ajoutée */
    EXPECT_TRUE(eng_assets_release(&f.assets, f.handle));
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
    SDL_RemovePath(HOT_FILE);
}

UTEST(eng_assets_reload, invalid_handles_are_refused)
{
    HotFixture f;
    ASSERT_TRUE(hot_setup(&f, "x"));
    EXPECT_FALSE(eng_assets_reload(&f.assets, ENG_HANDLE_NULL));
    const EngAssetHandle old = f.handle;
    EXPECT_TRUE(eng_assets_release(&f.assets, f.handle));
    EXPECT_FALSE(eng_assets_reload(&f.assets, old)); /* périmé */
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
    SDL_RemovePath(HOT_FILE);
}

/* Avance le temps par pas de 50 ms jusqu'à un rechargement ou `limit` secondes ; rend le temps
 * écoulé en secondes simulées, ou -1. */
static double poll_until_reload(EngAssets *assets, double start, double limit)
{
    const int steps = (int)(limit / 0.05);
    for (int step = 0; step < steps; step++) {
        const double t = start + ((double)step * 0.05);
        if (eng_assets_poll(assets, t) > 0) {
            return t - start;
        }
    }
    return -1.0;
}

UTEST(eng_assets_reload, poll_reloads_an_asset_whose_file_changed)
{
    HotFixture f;
    ASSERT_TRUE(hot_setup(&f, "version 1"));
    EXPECT_EQ(eng_assets_poll(&f.assets, 0.0), (uint32_t)0); /* surveillance coupée par défaut */
    eng_assets_set_hot_reload(&f.assets, true, 0.1);

    EXPECT_EQ(eng_assets_poll(&f.assets, 10.0), (uint32_t)0); /* crée les surveillances */
    EXPECT_EQ(poll_until_reload(&f.assets, 10.1, 1.0),
              -1.0); /* rien n'a changé : pas de rechargement */
    EXPECT_EQ(f.c.loads, 1);

    ASSERT_TRUE(write_text("version 2 plus longue"));
    const double elapsed = poll_until_reload(&f.assets, 11.0, 2.0);
    EXPECT_TRUE(elapsed >= 0.0);
    EXPECT_TRUE(elapsed < 0.5); /* détection + stabilité : au plus deux périodes de contrôle */
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text,
                 "version 2 plus longue");
    EXPECT_EQ(f.c.loads, 2);

    /* une seule fois par modification */
    EXPECT_EQ(poll_until_reload(&f.assets, 20.0, 1.0), -1.0);
    EXPECT_EQ(f.c.loads, 2);

    /* une modification invalide : échec, l'ancienne version reste, pas de nouvelle tentative */
    ASSERT_TRUE(SDL_SaveFile(HOT_FILE, "", 0));
    EXPECT_EQ(poll_until_reload(&f.assets, 30.0, 1.0), -1.0);
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text,
                 "version 2 plus longue");
    EXPECT_EQ(eng_assets_stats(&f.assets).reload_failures, (uint32_t)1);
    EXPECT_EQ(poll_until_reload(&f.assets, 40.0, 1.0), -1.0);
    EXPECT_EQ(eng_assets_stats(&f.assets).reload_failures, (uint32_t)1);

    /* réparé : rechargé */
    ASSERT_TRUE(write_text("v3 reparee"));
    EXPECT_TRUE(poll_until_reload(&f.assets, 50.0, 2.0) >= 0.0);
    EXPECT_STREQ(((const TextPayload *)eng_assets_get(&f.assets, f.handle))->text, "v3 reparee");

    /* surveillance coupée : plus de rechargement (les surveillances existent déjà : le changement
     * serait vu si la coupure n'était pas respectée) */
    EXPECT_EQ(eng_assets_poll(&f.assets, 55.0), (uint32_t)0);
    eng_assets_set_hot_reload(&f.assets, false, 0.0);
    ASSERT_TRUE(write_text("v4 ignoree par la coupure"));
    EXPECT_EQ(poll_until_reload(&f.assets, 60.0, 1.0), -1.0);

    EXPECT_TRUE(eng_assets_release(&f.assets, f.handle));
    EXPECT_EQ(eng_assets_shutdown(&f.assets), (uint32_t)0);
    SDL_RemovePath(HOT_FILE);
}
