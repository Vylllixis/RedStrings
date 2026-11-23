#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/mesh_file.h"
#include "engine/tex_file.h"
#include "utest.h"

/* Fichiers .mesh (M2-02) : aller-retour exact, disposition documentée, refus de chaque incohérence
 * (un champ modifié à la fois), chargement complet avec ses textures. Les modèles sont fabriqués
 * en mémoire : aucune dépendance à un fichier d'asset. */

typedef struct Fixture {
    EngArena arena;
    EngArena scratch;
} Fixture;

static bool fixture_open(Fixture *f)
{
    if (!eng_arena_init(&f->arena, "mesh-test", (size_t)32 << 20)) {
        return false;
    }
    if (!eng_arena_init(&f->scratch, "mesh-scratch", (size_t)32 << 20)) {
        eng_arena_shutdown(&f->arena);
        return false;
    }
    return true;
}

static void fixture_close(Fixture *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

static const EngMeshTexture TEXTURES[2] = {{"m.0.tex", true}, {"m.1.tex", false}};

/* Une boîte (24 sommets, 36 index u16) dont le matériau 1 utilise les deux textures. */
static bool make_model(Fixture *f, EngModel *m)
{
    const float lo[3] = {-1.0F, -2.0F, -3.0F};
    const float hi[3] = {1.0F, 2.0F, 3.0F};
    EngModelMaterial mat = {.base_color = {0.25F, 0.5F, 0.75F, 1.0F},
                            .image = 0,
                            .metallic = 0.125F,
                            .roughness = 0.875F,
                            .metallic_roughness_image = -1,
                            .normal_scale = 1.5F,
                            .normal_image = 1,
                            .occlusion_strength = 0.5F,
                            .occlusion_image = -1,
                            .emissive = {0.1F, 0.2F, 0.3F},
                            .emissive_image = 0};
    return eng_model_make_box(&f->arena, lo, hi, &mat, m);
}

static bool same3(const float *a, const float *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static bool same_model(const EngModel *a, const EngModel *b)
{
    return a->vertex_count == b->vertex_count && a->index_count == b->index_count &&
           a->part_count == b->part_count && a->material_count == b->material_count &&
           memcmp(a->vertices, b->vertices, a->vertex_count * sizeof(EngModelVertex)) == 0 &&
           memcmp(a->indices, b->indices, a->index_count * sizeof(uint32_t)) == 0 &&
           memcmp(a->parts, b->parts, a->part_count * sizeof(EngModelPart)) == 0 &&
           memcmp(a->materials, b->materials, a->material_count * sizeof(EngModelMaterial)) == 0 &&
           same3(a->bounds_min, b->bounds_min) && same3(a->bounds_max, b->bounds_max);
}

static uint8_t *copy_of(const void *file, size_t size)
{
    uint8_t *copy = SDL_malloc(size);
    if (copy != NULL) {
        memcpy(copy, file, size);
    }
    return copy;
}

static uint8_t *section_bytes(uint8_t *file, size_t size, uint32_t tag)
{
    EngAssetView view;
    EngAssetSection s;
    if (eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH, &view) !=
            ENG_ASSET_OK ||
        !eng_asset_find_section(&view, tag, &s)) {
        return NULL;
    }
    return file + (s.data - file);
}

static void poke_u32(uint8_t *section, size_t at, uint32_t value)
{
    memcpy(section + at, &value, sizeof value);
}

static void poke_u16(uint8_t *section, size_t at, uint16_t value)
{
    memcpy(section + at, &value, sizeof value);
}

static bool decodes(const void *file, size_t size)
{
    EngArena arena;
    if (!eng_arena_init(&arena, "dec", (size_t)4 << 20)) {
        return false;
    }
    EngMeshFile mesh;
    const bool ok = eng_mesh_decode(&arena, file, size, "test", &mesh);
    eng_arena_shutdown(&arena);
    return ok;
}

/* Encode le modèle de test. */
static bool encode(Fixture *f, const EngModel *m, const void **file, size_t *size)
{
    return eng_mesh_encode(&f->arena, &f->scratch, m, TEXTURES, 2, file, size);
}

/* ── Aller-retour ── */

UTEST(eng_mesh_file, encode_then_decode_gives_the_same_model)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    EXPECT_EQ(((uintptr_t)file) % 16U, (uintptr_t)0);

    EngMeshFile back;
    ASSERT_TRUE(eng_mesh_decode(&f.arena, file, size, "rt", &back));
    EXPECT_TRUE(same_model(&m, &back.model));
    ASSERT_EQ(back.texture_count, (uint32_t)2);
    EXPECT_STREQ(back.textures[0].path, "m.0.tex");
    EXPECT_TRUE(back.textures[0].srgb);
    EXPECT_STREQ(back.textures[1].path, "m.1.tex");
    EXPECT_FALSE(back.textures[1].srgb);
    /* images : allouées, sans pixels (c'est eng_mesh_load qui les remplit) */
    EXPECT_EQ(back.model.image_count, (uint32_t)2);
    EXPECT_TRUE(back.model.images[0].pixels == NULL);
    /* le décodage ne dépend pas du tampon d'origine : copie dans l'arène */
    memset((void *)file, 0, size);
    EXPECT_TRUE(same_model(&m, &back.model));
    EXPECT_STREQ(back.textures[1].path, "m.1.tex");
    fixture_close(&f);
}

UTEST(eng_mesh_file, file_layout_is_the_documented_one)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    EngAssetView view;
    ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 1, &view), ENG_ASSET_OK);
    const uint32_t order[7] = {ENG_ASSET_TAG_INFO, ENG_ASSET_TAG_VERT, ENG_ASSET_TAG_INDX,
                               ENG_ASSET_TAG_PART, ENG_ASSET_TAG_MATL, ENG_ASSET_TAG_TREF,
                               ENG_ASSET_TAG_STRS};
    ASSERT_EQ(view.section_count, (uint32_t)7);
    const uint32_t sizes[7] = {56, 24 * 48, 36 * 2, 40, 2 * 64, 2 * 8, 16};
    const uint32_t counts[7] = {1, 24, 36, 1, 2, 2, 16};
    for (uint32_t i = 0; i < 7; i++) {
        EngAssetSection s;
        ASSERT_TRUE(eng_asset_section_at(&view, i, &s));
        EXPECT_EQ(s.tag, order[i]);
        EXPECT_EQ(s.size, sizes[i]);
        EXPECT_EQ(s.count, counts[i]);
    }
    EngAssetSection info;
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_INFO, &info));
    uint32_t fields[8];
    memcpy(fields, info.data, sizeof fields);
    EXPECT_EQ(fields[0], (uint32_t)24); /* sommets */
    EXPECT_EQ(fields[1], (uint32_t)36); /* index */
    EXPECT_EQ(fields[2], (uint32_t)2);  /* index u16 */
    EXPECT_EQ(fields[3], (uint32_t)1);  /* parties */
    EXPECT_EQ(fields[4], (uint32_t)2);  /* matériaux */
    EXPECT_EQ(fields[5], (uint32_t)2);  /* textures */
    EXPECT_EQ(fields[6], (uint32_t)0);
    EXPECT_EQ(fields[7], (uint32_t)0);
    float bounds[6];
    memcpy(bounds, info.data + 32, sizeof bounds);
    EXPECT_NEAR(bounds[0], -1.0F, 0.0F);
    EXPECT_NEAR(bounds[4], 2.0F, 0.0F);
    /* partie : boîte englobante calculée = celle de la boîte */
    EngAssetSection part;
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_PART, &part));
    float pb[6];
    memcpy(pb, part.data + 16, sizeof pb);
    EXPECT_NEAR(pb[0], -1.0F, 0.0F);
    EXPECT_NEAR(pb[1], -2.0F, 0.0F);
    EXPECT_NEAR(pb[2], -3.0F, 0.0F);
    EXPECT_NEAR(pb[3], 1.0F, 0.0F);
    EXPECT_NEAR(pb[4], 2.0F, 0.0F);
    EXPECT_NEAR(pb[5], 3.0F, 0.0F);
    /* chemins : 2 chaînes terminées par un octet nul, dans l'ordre */
    EngAssetSection strs;
    ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &strs));
    EXPECT_EQ(memcmp(strs.data, "m.0.tex\0m.1.tex\0", 16), 0);
    fixture_close(&f);
}

UTEST(eng_mesh_file, limits_are_inclusive_and_encoding_reads_exactly_its_arrays)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel good;
    ASSERT_TRUE(make_model(&f, &good));
    const void *file = NULL;
    size_t size = 0;

    /* boîte aplatie : min == max sur un axe est une boîte valide (un plan) */
    EngModel m = good;
    for (int axis = 0; axis < 3; axis++) {
        m = good;
        m.bounds_max[axis] = m.bounds_min[axis];
        EXPECT_TRUE(encode(&f, &m, &file, &size));
    }

    /* le remplissage après INDX n'est pas vérifié : un fichier à remplissage non nul se décode */
    ASSERT_TRUE(encode(&f, &good, &file, &size));
    uint8_t *padded = copy_of(file, size);
    ASSERT_TRUE(padded != NULL);
    uint8_t *indx = section_bytes(padded, size, ENG_ASSET_TAG_INDX);
    const bool found = indx != NULL;
    if (found) {
        memset(indx + (size_t)72, 0xFF, 8); /* 36 index u16 = 72 octets, remplis jusqu'à 80 */
        EXPECT_TRUE(decodes(padded, size));
    }
    SDL_free(padded);
    ASSERT_TRUE(found);

    /* chemin de 255 caractères accepté, de 256 refusé */
    char path[300];
    memset(path, 'a', sizeof path);
    path[255] = '\0';
    EngMeshTexture tex[2] = {{path, true}, {"m.1.tex", false}};
    EXPECT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &good, tex, 2, &file, &size));
    EXPECT_TRUE(decodes(file, size));
    path[255] = 'a';
    path[256] = '\0';
    EXPECT_FALSE(eng_mesh_encode(&f.arena, &f.scratch, &good, tex, 2, &file, &size));

    /* tableaux de la taille exacte, sur le tas : ASan voit toute lecture d'un élément de trop */
    EngModel e = good;
    e.vertices = SDL_malloc(good.vertex_count * sizeof(EngModelVertex));
    e.indices = SDL_malloc(good.index_count * sizeof(uint32_t));
    e.parts = SDL_malloc(good.part_count * sizeof(EngModelPart));
    e.materials = SDL_malloc(good.material_count * sizeof(EngModelMaterial));
    const bool allocated =
        e.vertices != NULL && e.indices != NULL && e.parts != NULL && e.materials != NULL;
    if (allocated) {
        memcpy(e.vertices, good.vertices, good.vertex_count * sizeof(EngModelVertex));
        memcpy(e.indices, good.indices, good.index_count * sizeof(uint32_t));
        memcpy(e.parts, good.parts, good.part_count * sizeof(EngModelPart));
        memcpy(e.materials, good.materials, good.material_count * sizeof(EngModelMaterial));
        EXPECT_TRUE(encode(&f, &e, &file, &size));
    }
    SDL_free(e.vertices);
    SDL_free(e.indices);
    SDL_free(e.parts);
    SDL_free(e.materials);
    ASSERT_TRUE(allocated);
    fixture_close(&f);
}

UTEST(eng_mesh_file, encoding_is_deterministic)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *a = NULL;
    const void *b = NULL;
    size_t sa = 0;
    size_t sb = 0;
    const size_t before = eng_arena_remaining(&f.scratch);
    ASSERT_TRUE(encode(&f, &m, &a, &sa));
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before);        /* travail restitué */
    memset(eng_arena_alloc(&f.scratch, 8192, 16), 0xCD, 8192); /* octets « sales » */
    eng_arena_reset(&f.scratch);
    ASSERT_TRUE(encode(&f, &m, &b, &sb));
    ASSERT_EQ(sa, sb);
    EXPECT_EQ(memcmp(a, b, sa), 0);
    fixture_close(&f);
}

UTEST(eng_mesh_file, model_without_textures_has_no_texture_sections)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float lo[3] = {0, 0, 0};
    const float hi[3] = {1, 1, 1};
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, NULL, 0, &file, &size));
    EngAssetView view;
    ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 1, &view), ENG_ASSET_OK);
    EXPECT_EQ(view.section_count, (uint32_t)5);
    EngAssetSection s;
    EXPECT_FALSE(eng_asset_find_section(&view, ENG_ASSET_TAG_TREF, &s));
    EXPECT_FALSE(eng_asset_find_section(&view, ENG_ASSET_TAG_STRS, &s));
    EngMeshFile back;
    ASSERT_TRUE(eng_mesh_decode(&f.arena, file, size, "nt", &back));
    EXPECT_TRUE(same_model(&m, &back.model));
    EXPECT_EQ(back.texture_count, (uint32_t)0);
    EXPECT_TRUE(back.model.images == NULL);
    fixture_close(&f);
}

/* Modèle à `vertices` sommets (positions nulles), un triangle utilisant les deux derniers sommets.
 */
static bool make_big(Fixture *f, uint32_t vertices, EngModel *m)
{
    *m = (EngModel){0};
    m->vertices = ENG_ARENA_NEW_ARRAY(&f->arena, EngModelVertex, vertices);
    m->indices = ENG_ARENA_NEW_ARRAY(&f->arena, uint32_t, 3);
    m->parts = ENG_ARENA_NEW_ARRAY(&f->arena, EngModelPart, 1);
    m->materials = ENG_ARENA_NEW_ARRAY(&f->arena, EngModelMaterial, 1);
    if (m->vertices == NULL || m->indices == NULL || m->parts == NULL || m->materials == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < vertices; i++) {
        m->vertices[i].position[0] = (float)i;
        m->vertices[i].normal[2] = 1.0F;
    }
    m->vertex_count = vertices;
    m->indices[0] = 0;
    m->indices[1] = vertices - 2U;
    m->indices[2] = vertices - 1U;
    m->index_count = 3;
    m->parts[0] = (EngModelPart){0, 3, 0};
    m->part_count = 1;
    m->materials[0].base_color[0] = 1.0F;
    m->materials[0].image = -1;
    m->materials[0].metallic_roughness_image = -1;
    m->materials[0].normal_image = -1;
    m->materials[0].occlusion_image = -1;
    m->materials[0].emissive_image = -1;
    m->material_count = 1;
    m->bounds_max[0] = (float)vertices;
    return true;
}

UTEST(eng_mesh_file, index_width_switches_at_65536_vertices)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const uint32_t counts[3] = {65535, 65536, 65537};
    const uint32_t widths[3] = {2, 2, 4};
    for (int k = 0; k < 3; k++) {
        eng_arena_reset(&f.arena);
        EngModel m;
        ASSERT_TRUE(make_big(&f, counts[k], &m));
        const void *file = NULL;
        size_t size = 0;
        ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, NULL, 0, &file, &size));
        EngAssetView view;
        ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 1, &view), ENG_ASSET_OK);
        EngAssetSection indx;
        ASSERT_TRUE(eng_asset_find_section(&view, ENG_ASSET_TAG_INDX, &indx));
        EXPECT_EQ(indx.size, 3U * widths[k]);
        EngMeshFile back;
        ASSERT_TRUE(eng_mesh_decode(&f.arena, file, size, "big", &back));
        EXPECT_EQ(back.model.indices[2], counts[k] - 1U); /* 65535 tient sur 16 bits, 65536 non */
        EXPECT_EQ(back.model.indices[1], counts[k] - 2U);
        EXPECT_EQ(memcmp(back.model.vertices, m.vertices, counts[k] * sizeof(EngModelVertex)), 0);
    }
    fixture_close(&f);
}

/* ── Refus à l'écriture ── */

UTEST(eng_mesh_file, encode_refuses_invalid_models_and_restores_the_arenas)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel good;
    ASSERT_TRUE(make_model(&f, &good));
    const size_t arena_before = eng_arena_remaining(&f.arena);
    const size_t scratch_before = eng_arena_remaining(&f.scratch);
    const void *file = (const void *)1;
    size_t size = 7;

    EngModel m = good;
    uint32_t saved = m.indices[5];
    m.indices[5] = m.vertex_count; /* index hors limites */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    EXPECT_TRUE(file == NULL);
    EXPECT_EQ(size, (size_t)0);
    m.indices[5] = saved;

    EngModelPart part = good.parts[0];
    m = good;
    m.parts[0].first_index = 33; /* 33 + 36 dépasse */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.parts[0] = part;
    m.parts[0].index_count = 35; /* pas un multiple de 3 */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.parts[0] = part;
    m.parts[0].material = good.material_count;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.parts[0] = part;

    const EngModelMaterial mat = good.materials[1];
    m.materials[1].normal_image = 2; /* il n'y a que 2 textures : 0 et 1 */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.materials[1].normal_image = -2;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.materials[1] = mat;
    m.materials[1].roughness = NAN;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.materials[1] = mat;

    const float pos = m.vertices[3].position[1];
    m.vertices[3].position[1] = NAN; /* position non numérique : refusée à la boîte de la partie */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.vertices[3].position[1] = pos;

    m.bounds_min[0] = 5.0F; /* min > max */
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.bounds_min[0] = good.bounds_min[0];

    m.index_count = 35;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.index_count = good.index_count;
    m.vertex_count = 0;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m.vertex_count = good.vertex_count;
    m.vertices = NULL;
    EXPECT_FALSE(encode(&f, &m, &file, &size));
    m = good;

    /* textures : tableau absent, chemin absent ou dangereux */
    EXPECT_FALSE(eng_mesh_encode(&f.arena, &f.scratch, &m, NULL, 2, &file, &size));
    const char *bad_paths[] = {NULL,       "",        "/abs.tex", "../x.tex", "a/../x.tex",
                               "a//b.tex", "./x.tex", "a\\b.tex", "c:x.tex",  "a/b/.."};
    for (size_t i = 0; i < sizeof bad_paths / sizeof bad_paths[0]; i++) {
        const EngMeshTexture tex[2] = {{bad_paths[i], true}, {"ok.tex", false}};
        EXPECT_FALSE(eng_mesh_encode(&f.arena, &f.scratch, &m, tex, 2, &file, &size));
    }
    char longer[300];
    memset(longer, 'a', sizeof longer);
    longer[sizeof longer - 1U] = '\0';
    const EngMeshTexture too_long[2] = {{longer, true}, {"ok.tex", false}};
    EXPECT_FALSE(eng_mesh_encode(&f.arena, &f.scratch, &m, too_long, 2, &file, &size));
    static EngMeshTexture many[ENG_MESH_MAX_TEXTURES + 1U];
    for (uint32_t i = 0; i < ENG_MESH_MAX_TEXTURES + 1U; i++) {
        many[i] = (EngMeshTexture){"t.tex", false};
    }
    EXPECT_FALSE(
        eng_mesh_encode(&f.arena, &f.scratch, &m, many, ENG_MESH_MAX_TEXTURES + 1U, &file, &size));

    EXPECT_EQ(eng_arena_remaining(&f.arena), arena_before);
    EXPECT_EQ(eng_arena_remaining(&f.scratch), scratch_before);

    /* contrôle : des chemins avec sous-dossiers et points internes sont valides */
    const EngMeshTexture ok_paths[2] = {{"sub/dir/a.0.tex", true}, {"..a/b..tex", false}};
    EXPECT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, ok_paths, 2, &file, &size));
    fixture_close(&f);
}

UTEST(eng_mesh_file, encode_fails_cleanly_when_memory_runs_out)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    uint8_t storage[512];
    EngArena tiny;
    eng_arena_init_buffer(&tiny, "tiny", storage, sizeof storage);
    const void *file = NULL;
    size_t size = 0;
    const size_t tiny_start = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_mesh_encode(&tiny, &f.scratch, &m, TEXTURES, 2, &file, &size));
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_start);
    EngArena tiny_scratch;
    uint8_t work[64];
    eng_arena_init_buffer(&tiny_scratch, "tiny2", work, sizeof work);
    const size_t before = eng_arena_remaining(&tiny_scratch);
    EXPECT_FALSE(eng_mesh_encode(&f.arena, &tiny_scratch, &m, TEXTURES, 2, &file, &size));
    EXPECT_EQ(eng_arena_remaining(&tiny_scratch), before);
    /* décodage dans une arène trop petite : refusé, arène restituée */
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    EngMeshFile back;
    const size_t tiny_before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_mesh_decode(&tiny, file, size, "tiny", &back));
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_before);
    EXPECT_TRUE(back.model.vertices == NULL);
    fixture_close(&f);
}

/* ── Refus à la lecture : un champ à la fois ── */

typedef struct Bad {
    const char *what;
    uint32_t tag;
    size_t at;
    uint32_t value; /* écrit sur 4 octets ; kind sélectionne autre chose */
    int kind;       /* 0 : u32, 1 : u16, 2 : float (value = motif binaire), 3 : octets */
    const char *bytes;
} Bad;

#define U32 0
#define U16 1
#define F32 2
#define STR 3

static uint32_t float_bits(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

UTEST(eng_mesh_file, each_corrupted_field_is_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    ASSERT_TRUE(decodes(file, size));

    const uint32_t nan = float_bits(NAN);
    const uint32_t inf = float_bits(INFINITY);
    const Bad bad[] = {
        /* INFO : sommets@0 index@4 largeur@8 parties@12 matériaux@16 textures@20 flags@24 res@28
         *        min@32 max@44 */
        {"sommets +1", ENG_ASSET_TAG_INFO, 0, 25, U32, NULL},
        {"sommets -1", ENG_ASSET_TAG_INFO, 0, 23, U32, NULL},
        {"sommets 0", ENG_ASSET_TAG_INFO, 0, 0, U32, NULL},
        {"sommets énormes", ENG_ASSET_TAG_INFO, 0, 0xFFFFFFFFU, U32, NULL},
        {"index +3", ENG_ASSET_TAG_INFO, 4, 39, U32, NULL},
        {"index trop nombreux", ENG_ASSET_TAG_INFO, 4, ENG_MODEL_MAX_INDICES + 3U, U32, NULL},
        {"largeur 3", ENG_ASSET_TAG_INFO, 8, 3, U32, NULL},
        {"largeur 4 pour des index u16", ENG_ASSET_TAG_INFO, 8, 4, U32, NULL},
        {"largeur 0", ENG_ASSET_TAG_INFO, 8, 0, U32, NULL},
        {"parties +1", ENG_ASSET_TAG_INFO, 12, 2, U32, NULL},
        {"parties 0", ENG_ASSET_TAG_INFO, 12, 0, U32, NULL},
        {"parties trop nombreuses", ENG_ASSET_TAG_INFO, 12, ENG_MODEL_MAX_PARTS + 1U, U32, NULL},
        {"matériaux +1", ENG_ASSET_TAG_INFO, 16, 3, U32, NULL},
        {"matériaux 0", ENG_ASSET_TAG_INFO, 16, 0, U32, NULL},
        {"textures +1", ENG_ASSET_TAG_INFO, 20, 3, U32, NULL},
        {"textures 0 (TREF reste)", ENG_ASSET_TAG_INFO, 20, 0, U32, NULL},
        {"textures trop nombreuses", ENG_ASSET_TAG_INFO, 20, ENG_MESH_MAX_TEXTURES + 1U, U32, NULL},
        {"flags", ENG_ASSET_TAG_INFO, 24, 1, U32, NULL},
        {"reserved", ENG_ASSET_TAG_INFO, 28, 1, U32, NULL},
        {"boîte NaN", ENG_ASSET_TAG_INFO, 32, nan, F32, NULL},
        {"boîte infinie", ENG_ASSET_TAG_INFO, 44, inf, F32, NULL},
        {"boîte min > max", ENG_ASSET_TAG_INFO, 36, float_bits(99.0F), F32, NULL},
        /* INDX (u16) */
        {"index 0 vise le sommet 24", ENG_ASSET_TAG_INDX, 0, 24, U16, NULL},
        {"dernier index 65535", ENG_ASSET_TAG_INDX, 70, 0xFFFF, U16, NULL},
        /* PART : premier@0 compte@4 matériau@8 flags@12 min@16 max@28 */
        {"partie : premier index 1", ENG_ASSET_TAG_PART, 0, 1, U32, NULL},
        {"partie : compte 39", ENG_ASSET_TAG_PART, 4, 39, U32, NULL},
        {"partie : compte 0", ENG_ASSET_TAG_PART, 4, 0, U32, NULL},
        {"partie : compte énorme", ENG_ASSET_TAG_PART, 4, 0xFFFFFFFFU, U32, NULL},
        {"partie : matériau 2", ENG_ASSET_TAG_PART, 8, 2, U32, NULL},
        {"partie : flags", ENG_ASSET_TAG_PART, 12, 1, U32, NULL},
        {"partie : boîte NaN", ENG_ASSET_TAG_PART, 16, nan, F32, NULL},
        {"partie : boîte min > max", ENG_ASSET_TAG_PART, 16, float_bits(50.0F), F32, NULL},
        /* MATL : matériau 1 à 64 : couleur@0 métal@16 rugosité@20 normale@24 occl@28 émis@32
         *        textures@44 (couleur, mr, normale, occlusion, émissif) */
        {"couleur NaN", ENG_ASSET_TAG_MATL, 64, nan, F32, NULL},
        {"métal infini", ENG_ASSET_TAG_MATL, 80, inf, F32, NULL},
        {"rugosité NaN", ENG_ASSET_TAG_MATL, 84, nan, F32, NULL},
        {"échelle normale NaN", ENG_ASSET_TAG_MATL, 88, nan, F32, NULL},
        {"occlusion NaN", ENG_ASSET_TAG_MATL, 92, nan, F32, NULL},
        {"émissif NaN", ENG_ASSET_TAG_MATL, 96, nan, F32, NULL},
        {"texture de couleur 2", ENG_ASSET_TAG_MATL, 64 + 44, 2, U32, NULL},
        {"texture mr -2", ENG_ASSET_TAG_MATL, 64 + 48, 0xFFFFFFFEU, U32, NULL},
        {"texture normale 99", ENG_ASSET_TAG_MATL, 64 + 52, 99, U32, NULL},
        {"texture d'occlusion 2", ENG_ASSET_TAG_MATL, 64 + 56, 2, U32, NULL},
        {"texture émissive 2", ENG_ASSET_TAG_MATL, 64 + 60, 2, U32, NULL},
        {"matériau 0 : texture 2", ENG_ASSET_TAG_MATL, 44, 2, U32, NULL},
        /* TREF : décalage@0 drapeaux@4 ; STRS = "m.0.tex\0m.1.tex\0" */
        {"chemin : décalage hors STRS", ENG_ASSET_TAG_TREF, 0, 16, U32, NULL},
        {"chemin : décalage énorme", ENG_ASSET_TAG_TREF, 8, 0xFFFFFFFFU, U32, NULL},
        {"texture : drapeau inconnu", ENG_ASSET_TAG_TREF, 4, 2, U32, NULL},
        {"chemin sans nul final", ENG_ASSET_TAG_STRS, 15, 'x', STR, "x"},
        {"chemin vide", ENG_ASSET_TAG_STRS, 0, 0, STR, "\0"},
        {"chemin absolu", ENG_ASSET_TAG_STRS, 0, 0, STR, "/"},
        {"chemin « .. »", ENG_ASSET_TAG_STRS, 0, 0, STR, "../"},
        {"chemin « . »", ENG_ASSET_TAG_STRS, 0, 0, STR, "./"},
        {"chemin « .. » au milieu", ENG_ASSET_TAG_STRS, 8, 0, STR, "a/../"},
        {"chemin avec « \\ »", ENG_ASSET_TAG_STRS, 1, 0, STR, "\\"},
        {"chemin avec « : »", ENG_ASSET_TAG_STRS, 1, 0, STR, ":"},
        {"chemin avec « // »", ENG_ASSET_TAG_STRS, 1, 0, STR, "//"},
        {"chemin avec caractère de contrôle", ENG_ASSET_TAG_STRS, 2, 0, STR, "\x01"},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint8_t *copy = copy_of(file, size);
        ASSERT_TRUE(copy != NULL);
        uint8_t *section = section_bytes(copy, size, bad[i].tag);
        if (section == NULL) {
            SDL_free(copy);
        }
        ASSERT_TRUE(section != NULL);
        switch (bad[i].kind) {
        case U16:
            poke_u16(section, bad[i].at, (uint16_t)bad[i].value);
            break;
        case STR:
            memcpy(section + bad[i].at, bad[i].bytes,
                   bad[i].bytes[0] == '\0' ? 1 : strlen(bad[i].bytes));
            break;
        default:
            poke_u32(section, bad[i].at, bad[i].value);
            break;
        }
        if (decodes(copy, size)) {
            printf("accepté à tort : %s\n", bad[i].what);
            EXPECT_TRUE(false);
        }
        SDL_free(copy);
    }
    fixture_close(&f);
}

UTEST(eng_mesh_file, section_table_inconsistencies_are_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));

    /* entrée i de la table : 32 + 16 i ; champs tag@0 offset@4 taille@8 compte@12 */
    const struct {
        const char *what;
        uint32_t section;
        uint32_t field;
        uint32_t value;
    } bad[] = {
        {"VERT : compte faux", 1, 12, 23},
        {"VERT : taille fausse", 1, 8, 24 * 48 - 4},
        {"INDX : compte faux", 2, 12, 35},
        {"PART : taille fausse", 3, 8, 80},
        {"MATL : compte faux", 4, 12, 1},
        {"TREF : taille fausse", 5, 8, 8},
        {"STRS : compte différent de la taille", 6, 12, 15},
        {"INFO : taille fausse", 0, 8, 55},
        {"INFO : compte 2", 0, 12, 2},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint8_t *copy = copy_of(file, size);
        ASSERT_TRUE(copy != NULL);
        poke_u32(copy, 32U + 16U * bad[i].section + bad[i].field, bad[i].value);
        if (decodes(copy, size)) {
            printf("accepté à tort : %s\n", bad[i].what);
            EXPECT_TRUE(false);
        }
        SDL_free(copy);
    }

    /* section obligatoire absente : refait le fichier sans elle */
    EngAssetView view;
    ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 1, &view), ENG_ASSET_OK);
    for (uint32_t drop = 0; drop < view.section_count; drop++) {
        EngAssetSectionDesc descs[8];
        uint32_t n = 0;
        for (uint32_t i = 0; i < view.section_count; i++) {
            if (i == drop) {
                continue;
            }
            EngAssetSection s;
            ASSERT_TRUE(eng_asset_section_at(&view, i, &s));
            descs[n++] = (EngAssetSectionDesc){s.tag, s.data, s.size, s.count};
        }
        const size_t out_size = eng_asset_file_size(descs, n);
        uint8_t *out = SDL_malloc(out_size);
        ASSERT_TRUE(out != NULL);
        ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, descs, n, out, out_size, NULL));
        EXPECT_FALSE(decodes(out, out_size)); /* toutes sont nécessaires ici (textures déclarées) */
        SDL_free(out);
    }
    fixture_close(&f);
}

UTEST(eng_mesh_file, a_stray_texture_table_without_declared_textures_is_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float lo[3] = {0, 0, 0};
    const float hi[3] = {1, 1, 1};
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, NULL, 0, &file, &size));
    EngAssetView view;
    ASSERT_EQ(eng_asset_check(file, size, ENG_ASSET_MAGIC_MESH, 1, &view), ENG_ASSET_OK);
    EngAssetSectionDesc descs[6];
    for (uint32_t i = 0; i < 5; i++) {
        EngAssetSection s;
        ASSERT_TRUE(eng_asset_section_at(&view, i, &s));
        descs[i] = (EngAssetSectionDesc){s.tag, s.data, s.size, s.count};
    }
    const uint8_t extra[8] = {0};
    descs[5] = (EngAssetSectionDesc){ENG_ASSET_TAG_TREF, extra, 8, 1};
    uint8_t out[8192];
    size_t out_size = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, descs, 6, out, sizeof out, &out_size));
    EXPECT_FALSE(decodes(out, out_size));
    descs[5].size = 0; /* une section TREF vide est inoffensive */
    descs[5].count = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_MESH, 1, descs, 6, out, sizeof out, &out_size));
    EXPECT_TRUE(decodes(out, out_size));
    fixture_close(&f);
}

UTEST(eng_mesh_file, wrong_type_version_and_truncation_are_rejected)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    uint8_t *copy = copy_of(file, size);
    ASSERT_TRUE(copy != NULL);
    copy[8] = 2; /* version 2 : le moteur lit la 1 */
    EXPECT_FALSE(decodes(copy, size));
    copy[8] = 0;
    EXPECT_FALSE(decodes(copy, size));
    copy[8] = 1;
    EXPECT_TRUE(decodes(copy, size));
    copy[0] = 'T'; /* « TWMH » */
    EXPECT_FALSE(decodes(copy, size));
    SDL_free(copy);
    for (size_t n = 0; n < size; n++) {
        uint8_t *cut = copy_of(file, n > 0 ? n : 1);
        ASSERT_TRUE(cut != NULL);
        EXPECT_FALSE(decodes(cut, n));
        SDL_free(cut);
    }
    EngMeshFile back;
    memset(&back, 0xAB, sizeof back);
    EXPECT_FALSE(eng_mesh_decode(&f.arena, NULL, 0, NULL, &back));
    EXPECT_TRUE(back.model.vertices == NULL);
    EXPECT_EQ(back.texture_count, (uint32_t)0);
    fixture_close(&f);
}

UTEST(eng_mesh_file, byte_flip_fuzz_decoded_files_always_re_encode)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    const uint8_t values[3] = {0x00, 0x7F, 0xFF};
    EngArena arena;
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&arena, "fz", (size_t)2 << 20));
    ASSERT_TRUE(eng_arena_init(&scratch, "fz2", (size_t)2 << 20));
    for (size_t at = 0; at < size; at++) {
        for (int v = 0; v < 3; v++) {
            uint8_t *copy = copy_of(file, size); /* tampon exact : ASan voit tout dépassement */
            ASSERT_TRUE(copy != NULL);
            copy[at] = values[v];
            eng_arena_reset(&arena);
            EngMeshFile mesh;
            if (eng_mesh_decode(&arena, copy, size, "fuzz", &mesh)) {
                /* tout ce que le décodeur accepte, l'encodeur l'accepte aussi : mêmes règles, à
                 * une exception documentée : le décodeur ne regarde pas chaque coordonnée */
                bool positions_finite = true;
                for (uint32_t i = 0; i < mesh.model.vertex_count; i++) {
                    for (int a = 0; a < 3; a++) {
                        positions_finite =
                            positions_finite && isfinite(mesh.model.vertices[i].position[a]);
                    }
                }
                const void *again = NULL;
                size_t again_size = 0;
                if (positions_finite) {
                    EXPECT_TRUE(eng_mesh_encode(&arena, &scratch, &mesh.model, mesh.textures,
                                                mesh.texture_count, &again, &again_size));
                }
            } else {
                EXPECT_TRUE(mesh.model.vertices == NULL);
            }
            SDL_free(copy);
        }
    }
    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&arena);
    fixture_close(&f);
}

/* ── Chargement complet depuis le disque ── */

static bool write_file(const char *relative, const void *data, size_t size)
{
    char path[512];
    return eng_path_resolve(path, sizeof path, relative) && SDL_SaveFile(path, data, size);
}

static bool make_texture_file(Fixture *f, uint32_t w, uint32_t h, uint8_t seed, bool srgb,
                              const char *relative, EngImage *source)
{
    if (!eng_image_create(w, h, NULL, source)) {
        return false;
    }
    for (uint32_t i = 0; i < w * h * 4U; i++) {
        source->pixels[i] = (uint8_t)(seed + i * 5U);
    }
    const void *file = NULL;
    size_t size = 0;
    return eng_tex_encode(&f->arena, &f->scratch, source, srgb, &file, &size) &&
           write_file(relative, file, size);
}

UTEST(eng_mesh_file, load_reads_the_mesh_and_its_textures)
{
    char dir[512];
    ASSERT_TRUE(eng_path_resolve(dir, sizeof dir, "mesh_test"));
    ASSERT_TRUE(SDL_CreateDirectory(dir));
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    ASSERT_TRUE(encode(&f, &m, &file, &size));
    ASSERT_TRUE(write_file("mesh_test/m.mesh", file, size));
    EngImage tex0;
    EngImage tex1;
    ASSERT_TRUE(make_texture_file(&f, 8, 4, 10, true, "mesh_test/m.0.tex", &tex0));
    ASSERT_TRUE(make_texture_file(&f, 3, 5, 99, false, "mesh_test/m.1.tex", &tex1));

    EngArena arena;
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&arena, "ld", (size_t)2 << 20));
    ASSERT_TRUE(eng_arena_init(&scratch, "ld2", (size_t)2 << 20));
    const size_t scratch_before = eng_arena_remaining(&scratch);
    EngModel loaded;
    ASSERT_TRUE(eng_mesh_load(&arena, &scratch, "mesh_test/m.mesh", &loaded));
    EXPECT_EQ(eng_arena_remaining(&scratch), scratch_before);
    EXPECT_TRUE(same_model(&m, &loaded));
    ASSERT_EQ(loaded.image_count, (uint32_t)2);
    EXPECT_EQ(loaded.images[0].width, (uint32_t)8);
    EXPECT_EQ(loaded.images[0].height, (uint32_t)4);
    EXPECT_EQ(memcmp(loaded.images[0].pixels, tex0.pixels, (size_t)128), 0);
    EXPECT_EQ(loaded.images[1].width, (uint32_t)3);
    EXPECT_EQ(memcmp(loaded.images[1].pixels, tex1.pixels, (size_t)60), 0);
    eng_model_free(&loaded);
    EXPECT_TRUE(loaded.vertices == NULL);

    /* un modèle sans texture se charge sans eux */
    const float lo[3] = {0, 0, 0};
    const float hi[3] = {1, 1, 1};
    EngModel box;
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &box));
    ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &box, NULL, 0, &file, &size));
    ASSERT_TRUE(write_file("mesh_test/box.mesh", file, size));
    ASSERT_TRUE(eng_mesh_load(&arena, &scratch, "mesh_test/box.mesh", &loaded));
    EXPECT_TRUE(same_model(&box, &loaded));
    EXPECT_EQ(loaded.image_count, (uint32_t)0);
    eng_model_free(&loaded);

    eng_image_free(&tex0);
    eng_image_free(&tex1);
    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&arena);
    fixture_close(&f);
}

static bool make_bc7_texture_file(Fixture *f, uint32_t w, uint32_t h, uint8_t seed, bool srgb,
                                  const char *relative)
{
    EngImage source;
    if (!eng_image_create(w, h, NULL, &source)) {
        return false;
    }
    for (uint32_t i = 0; i < w * h * 4U; i++) {
        source.pixels[i] = (uint8_t)(seed + i * 5U);
    }
    const void *file = NULL;
    size_t size = 0;
    const bool ok = eng_tex_encode_bc7(&f->arena, &f->scratch, &source, srgb, &file, &size) &&
                    write_file(relative, file, size);
    eng_image_free(&source);
    return ok;
}

UTEST(eng_mesh_file, load_reads_bc7_textures_and_checks_their_color_space)
{
    char dir[512];
    ASSERT_TRUE(eng_path_resolve(dir, sizeof dir, "mesh_test"));
    ASSERT_TRUE(SDL_CreateDirectory(dir));
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    const EngMeshTexture names[2] = {{"b.0.tex", true}, {"b.1.tex", false}};
    ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, names, 2, &file, &size));
    ASSERT_TRUE(write_file("mesh_test/b.mesh", file, size));
    ASSERT_TRUE(make_bc7_texture_file(&f, 8, 4, 10, true, "mesh_test/b.0.tex"));
    ASSERT_TRUE(make_bc7_texture_file(&f, 3, 5, 99, false, "mesh_test/b.1.tex"));

    EngArena arena;
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&arena, "ld", (size_t)2 << 20));
    ASSERT_TRUE(eng_arena_init(&scratch, "ld2", (size_t)2 << 20));
    EngModel loaded;
    ASSERT_TRUE(eng_mesh_load(&arena, &scratch, "mesh_test/b.mesh", &loaded));
    EXPECT_TRUE(same_model(&m, &loaded));
    ASSERT_EQ(loaded.image_count, (uint32_t)2);
    EXPECT_EQ(loaded.images[0].encoding, ENG_IMAGE_BC7_SRGB);
    EXPECT_EQ(loaded.images[0].width, (uint32_t)8);
    EXPECT_EQ(loaded.images[0].height, (uint32_t)4);
    EXPECT_EQ(loaded.images[0].mip_count, (uint32_t)4);
    EXPECT_EQ(loaded.images[1].encoding, ENG_IMAGE_BC7);
    EXPECT_EQ(loaded.images[1].width, (uint32_t)3);
    EXPECT_EQ(loaded.images[1].mip_count, (uint32_t)3);
    eng_model_free(&loaded);

    /* une texture RGBA8 et une BC7 dans le même modèle */
    EngImage raw;
    ASSERT_TRUE(make_texture_file(&f, 8, 4, 10, true, "mesh_test/b.0.tex", &raw));
    ASSERT_TRUE(eng_mesh_load(&arena, &scratch, "mesh_test/b.mesh", &loaded));
    EXPECT_EQ(loaded.images[0].encoding, ENG_IMAGE_RGBA8);
    EXPECT_EQ(loaded.images[1].encoding, ENG_IMAGE_BC7);
    eng_model_free(&loaded);
    eng_image_free(&raw);

    /* espace de couleur du fichier différent de celui que le .mesh annonce : refusé, rien ne reste
     */
    ASSERT_TRUE(
        make_bc7_texture_file(&f, 8, 4, 10, false, "mesh_test/b.0.tex")); /* devrait être sRGB */
    const size_t arena_before = eng_arena_remaining(&arena);
    const size_t scratch_before = eng_arena_remaining(&scratch);
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/b.mesh", &loaded));
    EXPECT_TRUE(loaded.vertices == NULL);
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);
    EXPECT_EQ(eng_arena_remaining(&scratch), scratch_before);
    ASSERT_TRUE(make_bc7_texture_file(&f, 8, 4, 10, true, "mesh_test/b.0.tex"));
    ASSERT_TRUE(
        make_bc7_texture_file(&f, 3, 5, 99, true, "mesh_test/b.1.tex")); /* devrait être linéaire */
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/b.mesh", &loaded));
    EXPECT_TRUE(loaded.vertices == NULL);
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);

    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&arena);
    fixture_close(&f);
}

UTEST(eng_mesh_file, load_failures_leave_nothing_behind)
{
    char dir[512];
    ASSERT_TRUE(eng_path_resolve(dir, sizeof dir, "mesh_test"));
    ASSERT_TRUE(SDL_CreateDirectory(dir));
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    ASSERT_TRUE(make_model(&f, &m));
    const void *file = NULL;
    size_t size = 0;
    const EngMeshTexture own_textures[2] = {{"f.0.tex", true}, {"f.1.tex", false}};
    ASSERT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &m, own_textures, 2, &file, &size));
    ASSERT_TRUE(write_file("mesh_test/f.mesh", file, size));

    EngArena arena;
    EngArena scratch;
    ASSERT_TRUE(eng_arena_init(&arena, "ld", (size_t)2 << 20));
    ASSERT_TRUE(eng_arena_init(&scratch, "ld2", (size_t)2 << 20));
    const size_t arena_before = eng_arena_remaining(&arena);
    const size_t scratch_before = eng_arena_remaining(&scratch);
    EngModel loaded;

    /* aucune texture : échec dès la première */
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/absent.mesh", &loaded));
    char gone[512];
    ASSERT_TRUE(eng_path_resolve(gone, sizeof gone, "mesh_test/f.1.tex"));
    (void)SDL_RemovePath(gone);
    ASSERT_TRUE(eng_path_resolve(gone, sizeof gone, "mesh_test/f.0.tex"));
    (void)SDL_RemovePath(gone);
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/f.mesh", &loaded));
    EXPECT_TRUE(loaded.vertices == NULL);

    /* la première texture est valide (image créée), la deuxième manque : l'image créée est libérée
     * (ASan détecterait la fuite) et les arènes sont restituées */
    EngImage t0;
    EngImage t1;
    ASSERT_TRUE(make_texture_file(&f, 4, 4, 1, true, "mesh_test/f.0.tex", &t0));
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/f.mesh", &loaded));
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);
    EXPECT_EQ(eng_arena_remaining(&scratch), scratch_before);

    /* espace de couleur inversé : f.0.tex doit être sRGB, f.1.tex linéaire */
    ASSERT_TRUE(
        make_texture_file(&f, 4, 4, 1, false, "mesh_test/f.1.tex", &t1)); /* linéaire : bon */
    EXPECT_TRUE(eng_mesh_load(&arena, &scratch, "mesh_test/f.mesh", &loaded));
    eng_model_free(&loaded);
    eng_arena_reset(&arena);
    eng_image_free(&t1);
    ASSERT_TRUE(make_texture_file(&f, 4, 4, 1, true, "mesh_test/f.1.tex", &t1)); /* sRGB : faux */
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/f.mesh", &loaded));
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);
    EXPECT_EQ(eng_arena_remaining(&scratch), scratch_before);
    EXPECT_TRUE(loaded.vertices == NULL);
    EXPECT_TRUE(loaded.images == NULL);

    /* .tex corrompu */
    ASSERT_TRUE(
        write_file("mesh_test/f.1.tex", "pas un fichier tex du tout, mais assez long......", 48));
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/f.mesh", &loaded));
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);

    /* .mesh corrompu */
    ASSERT_TRUE(write_file("mesh_test/bad.mesh", "OWMH", 4));
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, "mesh_test/bad.mesh", &loaded));
    EXPECT_EQ(eng_arena_remaining(&arena), arena_before);

    /* chemin trop long pour le tampon interne */
    char huge[600];
    memset(huge, 'a', sizeof huge);
    huge[sizeof huge - 1U] = '\0';
    EXPECT_FALSE(eng_mesh_load(&arena, &scratch, huge, &loaded));

    eng_image_free(&t0);
    eng_image_free(&t1);
    eng_arena_shutdown(&scratch);
    eng_arena_shutdown(&arena);
    fixture_close(&f);
}

/* ── Préparation de la cuisson ── */

static EngModelMaterial plain_material(void)
{
    EngModelMaterial m = {.base_color = {1, 1, 1, 1},
                          .image = -1,
                          .metallic = 1.0F,
                          .roughness = 1.0F,
                          .metallic_roughness_image = -1,
                          .normal_scale = 1.0F,
                          .normal_image = -1,
                          .occlusion_strength = 1.0F,
                          .occlusion_image = -1,
                          .emissive_image = -1};
    return m;
}

UTEST(eng_mesh_file, plan_lists_each_image_per_color_space_in_first_use_order)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float lo[3] = {0, 0, 0};
    const float hi[3] = {1, 1, 1};
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    EngModelMaterial *mats = ENG_ARENA_NEW_ARRAY(&f.arena, EngModelMaterial, 3);
    ASSERT_TRUE(mats != NULL);
    mats[0] = plain_material();
    mats[1] = plain_material();
    mats[1].image = 2;          /* couleur de base : image 2, sRGB */
    mats[1].normal_image = 2;   /* la même image en linéaire : une autre texture */
    mats[1].emissive_image = 2; /* émissif : sRGB, donc la texture 0 */
    mats[2] = plain_material();
    mats[2].image = 2;                    /* déjà vue */
    mats[2].metallic_roughness_image = 0; /* image 0, linéaire (0 est un indice valide) */
    mats[2].occlusion_image = 0;          /* même image, même espace : même texture */
    mats[2].roughness = 0.25F;
    m.materials = mats;
    m.material_count = 3;
    m.parts[0].material = 2;
    EngImage images[3];
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_image_create(2, 2, NULL, &images[i]));
    }
    m.images = images;
    m.image_count = 3;

    EngMeshPlan plan;
    const size_t before = eng_arena_remaining(&f.arena);
    ASSERT_TRUE(eng_mesh_plan(&f.arena, &m, "scene", &plan));
    ASSERT_EQ(plan.texture_count, (uint32_t)3);
    EXPECT_STREQ(plan.textures[0].path, "scene.0.tex");
    EXPECT_STREQ(plan.textures[1].path, "scene.1.tex");
    EXPECT_STREQ(plan.textures[2].path, "scene.2.tex");
    EXPECT_TRUE(plan.textures[0].srgb);
    EXPECT_FALSE(plan.textures[1].srgb);
    EXPECT_FALSE(plan.textures[2].srgb);
    EXPECT_EQ(plan.source_images[0], (uint32_t)2);
    EXPECT_EQ(plan.source_images[1], (uint32_t)2);
    EXPECT_EQ(plan.source_images[2], (uint32_t)0);
    const EngModelMaterial *a = &plan.model.materials[1];
    EXPECT_EQ(a->image, 0);
    EXPECT_EQ(a->metallic_roughness_image, -1);
    EXPECT_EQ(a->normal_image, 1);
    EXPECT_EQ(a->occlusion_image, -1);
    EXPECT_EQ(a->emissive_image, 0);
    const EngModelMaterial *b = &plan.model.materials[2];
    EXPECT_EQ(b->image, 0);
    EXPECT_EQ(b->metallic_roughness_image, 2);
    EXPECT_EQ(b->normal_image, -1);
    EXPECT_EQ(b->occlusion_image, 2);
    EXPECT_EQ(b->emissive_image, -1);
    EXPECT_NEAR(b->roughness, 0.25F, 0.0F); /* le reste du matériau est intact */
    EXPECT_EQ(plan.model.materials[0].image, -1);
    /* la source n'est pas modifiée ; sommets, index et parties sont partagés */
    EXPECT_EQ(mats[1].image, 2);
    EXPECT_TRUE(plan.model.vertices == m.vertices);
    EXPECT_TRUE(plan.model.indices == m.indices);
    EXPECT_TRUE(plan.model.materials != m.materials);
    /* le plan s'encode tel quel */
    const void *file = NULL;
    size_t size = 0;
    EXPECT_TRUE(eng_mesh_encode(&f.arena, &f.scratch, &plan.model, plan.textures,
                                plan.texture_count, &file, &size));

    /* refus : arène restituée, plan nul */
    const size_t mid = eng_arena_remaining(&f.arena);
    EXPECT_LT(mid, before);
    const char *bad_stems[] = {NULL, "", "a/b", "..", ".", "a\\b", "x:y"};
    for (size_t i = 0; i < sizeof bad_stems / sizeof bad_stems[0]; i++) {
        EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, bad_stems[i], &plan));
        EXPECT_EQ(plan.texture_count, (uint32_t)0);
    }
    char long_stem[210];
    memset(long_stem, 'a', sizeof long_stem);
    long_stem[201] = '\0';
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, long_stem, &plan));
    long_stem[200] = '\0';
    EXPECT_TRUE(eng_mesh_plan(&f.arena, &m, long_stem, &plan));
    eng_arena_reset(&f.arena);

    for (int i = 0; i < 3; i++) {
        eng_image_free(&images[i]);
    }
    fixture_close(&f);
}

UTEST(eng_mesh_file, plan_refuses_missing_images_and_small_arenas)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float lo[3] = {0, 0, 0};
    const float hi[3] = {1, 1, 1};
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    EngModelMaterial mat = plain_material();
    mat.image = 0;
    m.materials = &mat;
    m.material_count = 1;
    EngMeshPlan plan;
    /* aucune image */
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    EngImage image = {.width = 2, .height = 2}; /* image non décodée : pixels absents */
    m.images = &image;
    m.image_count = 1;
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    mat.image = 1; /* hors limites */
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    mat.image = -2;
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    mat.image = -1;
    EXPECT_TRUE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    EXPECT_EQ(plan.texture_count, (uint32_t)0); /* aucune texture : rien à cuire */
    EXPECT_TRUE(plan.textures == NULL);
    m.material_count = 0;
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    m.materials = NULL;
    m.material_count = 1;
    EXPECT_FALSE(eng_mesh_plan(&f.arena, &m, "s", &plan));
    m.materials = &mat;

    uint8_t storage[32];
    EngArena tiny;
    eng_arena_init_buffer(&tiny, "tiny", storage, sizeof storage);
    const size_t before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_mesh_plan(&tiny, &m, "s", &plan));
    EXPECT_EQ(eng_arena_remaining(&tiny), before);
    fixture_close(&f);
}
