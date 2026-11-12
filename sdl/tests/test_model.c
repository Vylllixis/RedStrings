#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/model.h"
#include "engine/model_gpu.h"
#include "test_gltf.h"
#include "test_util.h"
#include "utest.h"

/* Chargement glTF (M1-10), sans GPU. Les fichiers sont fabriqués EN MÉMOIRE par un petit
 * constructeur (un triangle de référence : positions (0,0,0) (1,0,0) (0,1,0), normales +Z, UV
 * (0,0) (1,0) (0,1), index 0 1 2) que chaque test déforme : transformation de nœud, attributs
 * absents, données corrompues... Les valeurs attendues sont calculées à la main. */

#define EPS 1e-5F
/* ── Banc d'essai ── */

typedef struct Fixture {
    EngArena arena;
    EngArena scratch;
    uint8_t file[BUF_SIZE];
    size_t file_size;
} Fixture;

static bool fixture_open(Fixture *f)
{
    if (!eng_arena_init(&f->arena, "model-test", (size_t)1 << 20)) {
        return false;
    }
    if (!eng_arena_init(&f->scratch, "model-scratch", (size_t)1 << 20)) {
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

static bool load_spec(Fixture *f, const Spec *s, EngModel *model)
{
    f->file_size = build(s, f->file, sizeof f->file, NULL, NULL);
    return eng_model_parse(&f->arena, &f->scratch, f->file, f->file_size, NULL, model);
}

#define EXPECT_VEC3(v, ex, ey, ez)                                                                 \
    do {                                                                                           \
        EXPECT_NEAR((v)[0], (ex), EPS);                                                            \
        EXPECT_NEAR((v)[1], (ey), EPS);                                                            \
        EXPECT_NEAR((v)[2], (ez), EPS);                                                            \
    } while (0)

/* Normale géométrique du premier triangle (non normalisée), dans l'ordre des index. */
static void face_normal(const EngModel *m, float out[3])
{
    const float *a = m->vertices[m->indices[0]].position;
    const float *b = m->vertices[m->indices[1]].position;
    const float *c = m->vertices[m->indices[2]].position;
    const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    out[0] = (ab[1] * ac[2]) - (ab[2] * ac[1]);
    out[1] = (ab[2] * ac[0]) - (ab[0] * ac[2]);
    out[2] = (ab[0] * ac[1]) - (ab[1] * ac[0]);
}

/* ── Contenu de base ── */

/* Égalité champ à champ (EngLighting n'est que des float : 16 valeurs). */
static bool same_lighting(const EngLighting *a, const EngLighting *b)
{
    const float *x = (const float *)a;
    const float *y = (const float *)b;
    for (size_t i = 0; i < sizeof *a / sizeof(float); i++) {
        if (fabsf(x[i] - y[i]) > 0.0F) {
            return false;
        }
    }
    return true;
}

UTEST(eng_model, loads_a_triangle_with_all_attributes)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {0};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(m.vertex_count, (uint32_t)3);
    EXPECT_EQ(m.index_count, (uint32_t)3);
    EXPECT_EQ(m.part_count, (uint32_t)1);
    EXPECT_EQ(m.indices[0], (uint32_t)0);
    EXPECT_EQ(m.indices[1], (uint32_t)1);
    EXPECT_EQ(m.indices[2], (uint32_t)2);
    EXPECT_VEC3(m.vertices[0].position, 0.0F, 0.0F, 0.0F);
    EXPECT_VEC3(m.vertices[1].position, 1.0F, 0.0F, 0.0F);
    EXPECT_VEC3(m.vertices[2].position, 0.0F, 1.0F, 0.0F);
    EXPECT_VEC3(m.vertices[1].normal, 0.0F, 0.0F, 1.0F);
    EXPECT_NEAR(m.vertices[1].uv[0], 1.0F, EPS);
    EXPECT_NEAR(m.vertices[1].uv[1], 0.0F, EPS);
    EXPECT_NEAR(m.vertices[2].uv[0], 0.0F, EPS);
    EXPECT_NEAR(m.vertices[2].uv[1], 1.0F, EPS);
    EXPECT_VEC3(m.bounds_min, 0.0F, 0.0F, 0.0F);
    EXPECT_VEC3(m.bounds_max, 1.0F, 1.0F, 0.0F);
    EXPECT_EQ(m.parts[0].first_index, (uint32_t)0);
    EXPECT_EQ(m.parts[0].index_count, (uint32_t)3);
    EXPECT_EQ(m.parts[0].material, (uint32_t)0);
    EXPECT_EQ(m.material_count, (uint32_t)1);
    EXPECT_NEAR(m.materials[0].base_color[0], 1.0F, EPS);
    EXPECT_NEAR(m.materials[0].base_color[3], 1.0F, EPS);
    EXPECT_EQ(m.materials[0].image, (int32_t)-1);
    EXPECT_EQ(m.image_count, (uint32_t)0);
    eng_model_free(&m);
    EXPECT_TRUE(m.vertices == NULL);
    eng_model_free(&m); /* deuxième appel sans effet */
    fixture_close(&f);
}

UTEST(eng_model, a_glb_gives_the_same_model_as_the_gltf)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel a;
    EngModel b;
    Spec s = {0};
    ASSERT_TRUE(load_spec(&f, &s, &a));
    s.glb = true;
    ASSERT_TRUE(load_spec(&f, &s, &b));
    EXPECT_EQ(a.vertex_count, b.vertex_count);
    EXPECT_EQ(memcmp(a.vertices, b.vertices, a.vertex_count * sizeof(EngModelVertex)), 0);
    EXPECT_EQ(memcmp(a.indices, b.indices, a.index_count * sizeof(uint32_t)), 0);
    eng_model_free(&a);
    eng_model_free(&b);
    fixture_close(&f);
}

UTEST(eng_model, index_types_u8_u16_u32_and_missing_indices)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const int sizes[3] = {1, 2, 4};
    for (int i = 0; i < 3; i++) {
        EngModel m;
        const Spec s = {.index_bytes = sizes[i]};
        ASSERT_TRUE(load_spec(&f, &s, &m));
        EXPECT_EQ(m.index_count, (uint32_t)3);
        EXPECT_EQ(m.indices[0], (uint32_t)0);
        EXPECT_EQ(m.indices[1], (uint32_t)1);
        EXPECT_EQ(m.indices[2], (uint32_t)2);
        eng_model_free(&m);
    }
    EngModel m;
    const Spec none = {.no_indices = true};
    ASSERT_TRUE(load_spec(&f, &none, &m));
    EXPECT_EQ(m.index_count, (uint32_t)3); /* 3 sommets : 0 1 2 */
    EXPECT_EQ(m.indices[2], (uint32_t)2);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, missing_uvs_are_zero_and_missing_normals_are_computed)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.no_uvs = true, .no_normals = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (uint32_t v = 0; v < 3; v++) {
        EXPECT_NEAR(m.vertices[v].uv[0], 0.0F, EPS);
        EXPECT_NEAR(m.vertices[v].uv[1], 0.0F, EPS);
        /* (1,0,0) x (0,1,0) = +Z : le triangle est anti-horaire vu de +Z */
        EXPECT_VEC3(m.vertices[v].normal, 0.0F, 0.0F, 1.0F);
    }
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, normals_of_the_file_are_normalized_and_kept)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.normal = {0.0F, 3.0F, 0.0F}};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 1.0F, 0.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

/* ── Transformations de nœuds ── */

UTEST(eng_model, node_translation_and_scale_are_baked_into_the_vertices)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.node_json = "\"translation\":[10,20,30],\"scale\":[2,3,4]"};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC3(m.vertices[0].position, 10.0F, 20.0F, 30.0F);
    EXPECT_VEC3(m.vertices[1].position, 12.0F, 20.0F, 30.0F); /* (1,0,0) -> 2 * 1 + 10 */
    EXPECT_VEC3(m.vertices[2].position, 10.0F, 23.0F, 30.0F);
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 0.0F, 1.0F);
    EXPECT_VEC3(m.bounds_min, 10.0F, 20.0F, 30.0F);
    EXPECT_VEC3(m.bounds_max, 12.0F, 23.0F, 30.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, node_rotation_turns_positions_and_normals)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* quaternion (x, y, z, w) : 90° autour de Z. (1,0,0) -> (0,1,0) ; (0,1,0) -> (-1,0,0) */
    const Spec s = {.node_json = "\"rotation\":[0,0,0.70710678,0.70710678]",
                    .normal = {1.0F, 0.0F, 0.0F}};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC3(m.vertices[1].position, 0.0F, 1.0F, 0.0F);
    EXPECT_VEC3(m.vertices[2].position, -1.0F, 0.0F, 0.0F);
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 1.0F, 0.0F); /* la normale (1,0,0) tourne aussi */
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, non_uniform_scale_uses_the_inverse_transpose_for_normals)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* Échelle (2, 1, 1) : la normale (1,1,0)/sqrt2 devient (1/2, 1, 0) normalisée = (0.4472,
     * 0.8944, 0) ; avec la matrice du modèle on aurait (2, 1, 0) -> (0.8944, 0.4472, 0). */
    const Spec s = {.node_json = "\"scale\":[2,1,1]", .normal = {1.0F, 1.0F, 0.0F}};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_NEAR(m.vertices[0].normal[0], 0.4472136F, EPS);
    EXPECT_NEAR(m.vertices[0].normal[1], 0.8944272F, EPS);
    EXPECT_NEAR(m.vertices[0].normal[2], 0.0F, EPS);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, a_mirroring_node_flips_the_triangle_winding)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.node_json = "\"scale\":[-1,1,1]", .no_normals = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC3(m.vertices[1].position, -1.0F, 0.0F, 0.0F);
    EXPECT_EQ(m.indices[0], (uint32_t)0);
    EXPECT_EQ(m.indices[1], (uint32_t)2); /* 1 et 2 échangés */
    EXPECT_EQ(m.indices[2], (uint32_t)1);
    float n[3];
    face_normal(&m, n);
    EXPECT_GT(n[2], 0.0F); /* toujours anti-horaire vu de +Z */
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 0.0F, 1.0F);

    /* une symétrie double (deux échelles négatives) conserve l'orientation : pas d'échange */
    EngModel keep;
    const Spec twice = {.node_json = "\"scale\":[-1,-1,1]"};
    ASSERT_TRUE(load_spec(&f, &twice, &keep));
    EXPECT_EQ(keep.indices[1], (uint32_t)1);
    EXPECT_EQ(keep.indices[2], (uint32_t)2);
    eng_model_free(&keep);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, parent_and_child_transforms_are_composed)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.hierarchy = true,
                    .parent_json = "\"translation\":[1,0,0],\"scale\":[2,2,2]",
                    .node_json = "\"translation\":[0,3,0]"};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    /* monde = parent * enfant : (0,0,0) -> enfant (0,3,0) -> parent 2*(0,3,0)+(1,0,0) = (1,6,0) */
    EXPECT_VEC3(m.vertices[0].position, 1.0F, 6.0F, 0.0F);
    EXPECT_VEC3(m.vertices[1].position, 3.0F, 6.0F, 0.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, a_mesh_used_by_two_nodes_is_copied_twice_with_global_indices)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.instances = 2};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(m.vertex_count, (uint32_t)6);
    EXPECT_EQ(m.index_count, (uint32_t)6);
    EXPECT_EQ(m.part_count, (uint32_t)2);
    EXPECT_EQ(m.parts[1].first_index, (uint32_t)3);
    EXPECT_EQ(m.parts[1].index_count, (uint32_t)3);
    EXPECT_EQ(m.indices[3], (uint32_t)3); /* indices globaux : décalés du nombre de sommets */
    EXPECT_EQ(m.indices[5], (uint32_t)5);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, singular_node_matrix_is_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.node_json = "\"scale\":[0,1,1]"};
    EXPECT_FALSE(load_spec(&f, &s, &m));
    EXPECT_TRUE(m.vertices == NULL);
    fixture_close(&f);
}

/* ── Matériaux et images ── */

UTEST(eng_model, material_factor_and_default_material_numbering)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material =
                        "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.2,0.4,0.6,0.5]}}"};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(m.material_count, (uint32_t)2); /* défaut + 1 */
    EXPECT_EQ(m.parts[0].material, (uint32_t)1);
    EXPECT_NEAR(m.materials[1].base_color[0], 0.2F, EPS);
    EXPECT_NEAR(m.materials[1].base_color[1], 0.4F, EPS);
    EXPECT_NEAR(m.materials[1].base_color[2], 0.6F, EPS);
    EXPECT_NEAR(m.materials[1].base_color[3], 0.5F, EPS);
    EXPECT_EQ(m.materials[1].image, (int32_t)-1);
    EXPECT_NEAR(m.materials[0].base_color[1], 1.0F, EPS); /* le défaut reste blanc */
    eng_model_free(&m);

    /* matériau sans bloc PBR : couleur par défaut (1, 1, 1, 1) */
    const Spec plain = {.material = "{\"name\":\"nu\"}"};
    ASSERT_TRUE(load_spec(&f, &plain, &m));
    EXPECT_NEAR(m.materials[1].base_color[2], 1.0F, EPS);
    eng_model_free(&m);
    fixture_close(&f);
}

#define TEXTURED "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}"

#define EXPECT_PNG_DECODED(m)                                                                      \
    do {                                                                                           \
        EXPECT_EQ((m).image_count, (uint32_t)2);                                                   \
        EXPECT_EQ((m).materials[1].image, (int32_t)0);                                             \
        EXPECT_EQ((m).images[0].width, (uint32_t)2);                                               \
        EXPECT_EQ((m).images[0].height, (uint32_t)2);                                              \
        EXPECT_EQ((m).images[0].pixels[0], (uint8_t)255); /* haut-gauche rouge */                  \
        EXPECT_EQ((m).images[0].pixels[1], (uint8_t)0);                                            \
        EXPECT_TRUE((m).images[1].pixels == NULL); /* image non utilisée : pas décodée */       \
    } while (0)

UTEST(eng_model, base_color_texture_from_a_data_uri)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material = TEXTURED, .texture = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_PNG_DECODED(m);
    eng_model_free(&m);
    EXPECT_TRUE(m.images == NULL);
    fixture_close(&f);
}

UTEST(eng_model, base_color_texture_from_a_glb_buffer_view)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material = TEXTURED, .texture = true, .glb = true, .png_in_view = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_PNG_DECODED(m);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, a_corrupt_texture_fails_the_load_without_leaking)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* image 0 = octets invalides (base64 de « abc ») : décodage refusé */
    const Spec s = {
        .material = TEXTURED, .texture = true, .image_uri = "data:image/png;base64,YWJj"};
    EXPECT_FALSE(load_spec(&f, &s, &m));
    EXPECT_TRUE(m.images == NULL);
    fixture_close(&f);
}

UTEST(eng_model, failure_after_images_are_decoded_frees_them)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* l'image 0 est décodée (matériaux d'abord), puis la matrice singulière du nœud fait échouer le
     * remplissage (après la validation de cgltf) : les pixels doivent être libérés (ASan
     * détecterait la fuite) */
    const Spec s = {.material = TEXTURED, .texture = true, .node_json = "\"scale\":[0,1,1]"};
    const size_t before = eng_arena_remaining(&f.arena);
    EXPECT_FALSE(load_spec(&f, &s, &m));
    EXPECT_TRUE(m.vertices == NULL);
    EXPECT_EQ(eng_arena_remaining(&f.arena), before); /* l'arène est restituée */
    fixture_close(&f);
}

UTEST(eng_model, external_image_without_a_base_path_is_an_error)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material = TEXTURED, .texture = true, .image_uri = "t.png"};
    EXPECT_FALSE(load_spec(&f, &s, &m));
    fixture_close(&f);
}

/* ── Données invalides ── */

UTEST(eng_model, malformed_files_are_refused_with_a_zeroed_model)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m = {.vertex_count = 9};
    EXPECT_FALSE(eng_model_parse(&f.arena, &f.scratch, NULL, 10, NULL, &m));
    EXPECT_EQ(m.vertex_count, (uint32_t)0);
    EXPECT_FALSE(eng_model_parse(&f.arena, &f.scratch, "{}", 0, NULL, &m));
    const char *json = "{ ceci n'est pas du json";
    EXPECT_FALSE(eng_model_parse(&f.arena, &f.scratch, json, strlen(json), NULL, &m));
    const char *old = "{\"asset\":{\"version\":\"1.0\"}}";
    EXPECT_FALSE(eng_model_parse(&f.arena, &f.scratch, old, strlen(old), NULL, &m));
    const char *empty = "{\"asset\":{\"version\":\"2.0\"}}"; /* aucun maillage */
    EXPECT_FALSE(eng_model_parse(&f.arena, &f.scratch, empty, strlen(empty), NULL, &m));
    fixture_close(&f);
}

UTEST(eng_model, truncated_binary_data_is_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.truncate = true};
    EXPECT_FALSE(load_spec(&f, &s, &m));
    const Spec glb = {.truncate = true, .glb = true};
    EXPECT_FALSE(load_spec(&f, &glb, &m));
    fixture_close(&f);
}

UTEST(eng_model, out_of_range_index_is_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.bad_index = 3}; /* 3 sommets : les index valides vont de 0 à 2 */
    EXPECT_FALSE(load_spec(&f, &s, &m));
    const Spec ok = {.bad_index = 2};
    EXPECT_TRUE(load_spec(&f, &ok, &m)); /* la limite exacte passe */
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, index_count_must_be_a_multiple_of_three)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec four = {.index_count = 4};
    EXPECT_FALSE(load_spec(&f, &four, &m));
    const Spec six = {.index_count = 6};
    ASSERT_TRUE(load_spec(&f, &six, &m));
    EXPECT_EQ(m.index_count, (uint32_t)6);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, non_triangle_primitives_are_skipped_but_a_file_without_triangles_fails)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec mixed = {.extra_lines = true};
    ASSERT_TRUE(load_spec(&f, &mixed, &m));
    EXPECT_EQ(m.part_count, (uint32_t)1); /* les lignes sont ignorées */
    eng_model_free(&m);
    const Spec lines = {.mode = 1, .no_indices = true};
    EXPECT_FALSE(load_spec(&f, &lines, &m));
    fixture_close(&f);
}

UTEST(eng_model, unsupported_required_extensions_are_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const char *names[3] = {"KHR_draco_mesh_compression", "EXT_meshopt_compression",
                            "KHR_texture_basisu"};
    for (int i = 0; i < 3; i++) {
        char extra[128];
        SDL_snprintf(extra, sizeof extra,
                     "\"extensionsUsed\":[\"%s\"],\"extensionsRequired\":[\"%s\"]", names[i],
                     names[i]);
        const Spec s = {.extra_top = extra};
        EXPECT_FALSE(load_spec(&f, &s, &m));
    }
    /* une extension facultative inconnue ne gêne pas */
    const Spec optional = {.extra_top = "\"extensionsUsed\":[\"KHR_lights_punctual\"]"};
    ASSERT_TRUE(load_spec(&f, &optional, &m));
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, a_too_small_arena_fails_cleanly_and_is_restored)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngArena tiny;
    uint8_t storage[64];
    eng_arena_init_buffer(&tiny, "tiny", storage, sizeof storage);
    const Spec s = {0};
    f.file_size = build(&s, f.file, sizeof f.file, NULL, NULL);
    EngModel m;
    const size_t before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_model_parse(&tiny, &f.scratch, f.file, f.file_size, NULL, &m));
    EXPECT_EQ(eng_arena_remaining(&tiny), before);
    EXPECT_TRUE(m.vertices == NULL);

    /* arène de travail trop petite */
    EngArena tiny_scratch;
    eng_arena_init_buffer(&tiny_scratch, "tiny-scratch", storage, 8);
    EXPECT_FALSE(eng_model_parse(&f.arena, &tiny_scratch, f.file, f.file_size, NULL, &m));
    fixture_close(&f);
}

UTEST(eng_model, the_scratch_arena_is_restored_after_success)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material = TEXTURED, .texture = true};
    const size_t before = eng_arena_remaining(&f.scratch);
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before);
    eng_model_free(&m);
    fixture_close(&f);
}

/* ── Fichiers externes ── */

static bool write_file(const char *relative, const void *data, size_t size)
{
    char path[512];
    return eng_path_resolve(path, sizeof path, relative) && SDL_SaveFile(path, data, size);
}

UTEST(eng_model, loads_a_gltf_with_external_buffer_and_percent_encoded_image_names)
{
    char dir[512];
    ASSERT_TRUE(eng_path_resolve(dir, sizeof dir, "model_test"));
    ASSERT_TRUE(SDL_CreateDirectory(dir));
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));

    const Spec s = {.material = TEXTURED,
                    .texture = true,
                    .buffer_uri = "tampon.bin",
                    .image_uri = "ma%20texture.png"};
    uint8_t blob[1024];
    size_t blob_size = 0;
    f.file_size = build(&s, f.file, sizeof f.file, blob, &blob_size);
    ASSERT_TRUE(write_file("model_test/m.gltf", f.file, f.file_size));
    ASSERT_TRUE(write_file("model_test/tampon.bin", blob, blob_size));
    ASSERT_TRUE(write_file("model_test/ma texture.png", PNG_2X2, sizeof PNG_2X2));

    EngModel m;
    ASSERT_TRUE(eng_model_load(&f.arena, &f.scratch, "model_test/m.gltf", &m));
    EXPECT_EQ(m.vertex_count, (uint32_t)3);
    EXPECT_NEAR(m.vertices[1].position[0], 1.0F, EPS);
    EXPECT_PNG_DECODED(m);
    eng_model_free(&m);

    /* tampon externe absent : échec */
    char gone[512];
    ASSERT_TRUE(eng_path_resolve(gone, sizeof gone, "model_test/tampon.bin"));
    ASSERT_TRUE(SDL_RemovePath(gone));
    EXPECT_FALSE(eng_model_load(&f.arena, &f.scratch, "model_test/m.gltf", &m));
    EXPECT_TRUE(m.vertices == NULL);

    /* fichier principal absent */
    EXPECT_FALSE(eng_model_load(&f.arena, &f.scratch, "model_test/absent.gltf", &m));
    fixture_close(&f);
}

/* ── Dessin : une commande par partie (sans GPU : la file n'est que des données) ── */

typedef struct DrawQueue {
    EngRenderQueue queue;
    EngDrawCmd commands[4];
    uint8_t uniforms[4096];
} DrawQueue;

static void init_draw_queue(DrawQueue *d, uint32_t capacity)
{
    memset(d, 0, sizeof *d);
    d->queue.commands = d->commands;
    d->queue.capacity = capacity;
    d->queue.uniforms = d->uniforms;
    d->queue.uniform_capacity = sizeof d->uniforms;
}

/* Les mêmes champs, dans le même ordre que ModelUniforms (model_gpu.c). */
typedef struct UniformCopy {
    float mvp[16];
    float model[16];
    float normal_matrix[16];
    float color[4];
} UniformCopy;

/* Les mêmes champs, dans le même ordre que LightUniforms (model_gpu.c) et le cbuffer de model.frag.
 */
typedef struct LightCopy {
    float sun_direction[4];
    float sun_radiance[4];
    float ambient[4];
    float camera_position[4];
    float material[4];
    float emissive[4];
    float environment[4];
    float shadow_matrix[16];
    float shadow_a[4];
    float shadow_b[4];
    float shadow_c[4];
} LightCopy;

UTEST(eng_model_gpu, draw_queues_one_command_per_part_with_its_material_and_uniforms)
{
    EngModelPart parts[2] = {{.first_index = 0, .index_count = 6, .material = 1},
                             {.first_index = 6, .index_count = 3, .material = 0}};
    EngModelGpuMaterial materials[2] = {
        {.base_color = {1, 1, 1, 1}, .texture = {.handle = {.index = 5, .generation = 1}}},
        {.base_color = {0.25F, 0.5F, 0.75F, 1.0F},
         .texture = {.handle = {.index = 9, .generation = 2}}}};
    EngModelGpu gpu = {.vertices = {.handle = {.index = 1, .generation = 1}},
                       .indices = {.handle = {.index = 2, .generation = 1}},
                       .sampler = {.handle = {.index = 3, .generation = 1}},
                       .parts = parts,
                       .part_count = 2,
                       .materials = materials,
                       .material_count = 2};
    const EngPipelineHandle pipeline = {.handle = {.index = 7, .generation = 3}};

    mat4 model;
    mat4 view_proj;
    glm_scale_make(model, (vec3){2.0F, 1.0F, 1.0F}); /* échelle non uniforme */
    glm_translate(model, (vec3){1.0F, 0.0F, 0.0F});
    glm_perspective(1.0F, 1.5F, 0.1F, 50.0F, view_proj);
    DrawQueue d;
    init_draw_queue(&d, 4);
    EXPECT_EQ(eng_model_gpu_draw(&gpu, &d.queue, pipeline, model, view_proj, NULL), (uint32_t)2);
    ASSERT_EQ(d.queue.count, (uint32_t)2);

    const EngDrawCmd *c0 = &d.queue.commands[0];
    EXPECT_EQ(memcmp(&c0->pipeline, &pipeline, sizeof pipeline), 0);
    EXPECT_EQ(memcmp(&c0->vertex_buffer, &gpu.vertices, sizeof gpu.vertices), 0);
    EXPECT_EQ(memcmp(&c0->index_buffer, &gpu.indices, sizeof gpu.indices), 0);
    EXPECT_EQ(memcmp(&c0->sampler, &gpu.sampler, sizeof gpu.sampler), 0);
    EXPECT_EQ(c0->index_type, ENG_INDEX_U32);
    EXPECT_EQ(c0->count, (uint32_t)6);
    EXPECT_EQ(c0->first, (uint32_t)0);
    EXPECT_EQ(memcmp(&c0->texture, &materials[1].texture, sizeof c0->texture), 0);
    EXPECT_EQ(c0->uniform_size, (uint32_t)sizeof(UniformCopy));

    const EngDrawCmd *c1 = &d.queue.commands[1];
    EXPECT_EQ(c1->count, (uint32_t)3);
    EXPECT_EQ(c1->first, (uint32_t)6);
    EXPECT_EQ(memcmp(&c1->texture, &materials[0].texture, sizeof c1->texture), 0);

    UniformCopy u0;
    UniformCopy u1;
    memcpy(&u0, &d.uniforms[c0->uniform_offset], sizeof u0);
    memcpy(&u1, &d.uniforms[c1->uniform_offset], sizeof u1);
    mat4 expected_mvp;
    glm_mat4_mul(view_proj, model, expected_mvp);
    for (int i = 0; i < 16; i++) {
        EXPECT_NEAR(u0.mvp[i], ((float *)expected_mvp)[i], 1e-5F);
        EXPECT_NEAR(u1.mvp[i], ((float *)expected_mvp)[i], 1e-5F);
    }
    /* matrice normale : inverse transposée du 3x3, dans un 4x4 (dernière ligne/colonne = identité)
     * ; pour l'échelle (2, 1, 1) : diag(1/2, 1, 1) */
    EXPECT_NEAR(u0.normal_matrix[0], 0.5F, EPS);
    EXPECT_NEAR(u0.normal_matrix[5], 1.0F, EPS);
    EXPECT_NEAR(u0.normal_matrix[10], 1.0F, EPS);
    EXPECT_NEAR(u0.normal_matrix[15], 1.0F, EPS);
    EXPECT_NEAR(u0.normal_matrix[12], 0.0F, EPS); /* la translation n'y est pas */
    /* la matrice du modèle est passée telle quelle (position monde pour le reflet) */
    for (int i = 0; i < 16; i++) {
        EXPECT_NEAR(u0.model[i], ((float *)model)[i], 1e-6F);
    }
    EXPECT_NEAR(u0.color[0], 0.25F, EPS); /* matériau 1 pour la partie 0 */
    EXPECT_NEAR(u0.color[2], 0.75F, EPS);
    EXPECT_NEAR(u1.color[0], 1.0F, EPS); /* matériau 0 pour la partie 1 */
}

UTEST(eng_model_gpu, draw_sends_the_lighting_as_fragment_uniforms_premultiplied_by_intensity)
{
    EngModelPart part = {.first_index = 0, .index_count = 3, .material = 0};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1}};
    EngModelGpu gpu = {
        .parts = &part, .part_count = 1, .materials = &material, .material_count = 1};
    mat4 identity;
    glm_mat4_identity(identity);
    EngLighting light = {.sun_direction = {0.0F, 0.6F, 0.8F},
                         .sun_color = {1.0F, 0.5F, 0.25F},
                         .sun_intensity = 2.0F,
                         .ambient_color = {0.2F, 0.4F, 0.8F},
                         .ambient_intensity = 0.5F,
                         .camera_position = {1.0F, 2.0F, 3.0F}};
    const EngLighting before = light;
    DrawQueue d;
    init_draw_queue(&d, 2);
    ASSERT_EQ(
        eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, &light),
        (uint32_t)1);
    EXPECT_TRUE(same_lighting(&light, &before)); /* l'éclairage de l'appelant est intact */
    const EngDrawCmd *c = &d.queue.commands[0];
    ASSERT_EQ(c->fragment_uniform_size, (uint32_t)sizeof(LightCopy));
    EXPECT_EQ(c->fragment_uniform_offset % ENG_UNIFORM_ALIGNMENT, (uint32_t)0);
    EXPECT_GE(c->fragment_uniform_offset,
              c->uniform_offset + c->uniform_size); /* pas de recouvrement */
    LightCopy u;
    memcpy(&u, &d.uniforms[c->fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.sun_direction[0], 0.0F, EPS);
    EXPECT_NEAR(u.sun_direction[1], 0.6F, EPS);
    EXPECT_NEAR(u.sun_direction[2], 0.8F, EPS);
    EXPECT_NEAR(u.sun_radiance[0], 2.0F, EPS); /* couleur * intensité */
    EXPECT_NEAR(u.sun_radiance[1], 1.0F, EPS);
    EXPECT_NEAR(u.sun_radiance[2], 0.5F, EPS);
    EXPECT_NEAR(u.ambient[0], 0.1F, EPS);
    EXPECT_NEAR(u.ambient[1], 0.2F, EPS);
    EXPECT_NEAR(u.ambient[2], 0.4F, EPS);
    EXPECT_NEAR(u.camera_position[0], 1.0F, EPS);
    EXPECT_NEAR(u.camera_position[1], 2.0F, EPS);
    EXPECT_NEAR(u.camera_position[2], 3.0F, EPS);
}

/* Ombre (M1-17) : la matrice, les biais et le filtre arrivent dans les uniforms, dans l'ordre du
 * cbuffer de model.frag ; la carte et son sampler sont liés au dessin ; sans ombre, c'est la
 * texture blanche (jamais lue : shadow_c.z = 0). */
UTEST(eng_model_gpu, draw_sends_the_shadow_matrix_biases_and_map)
{
    EngModelPart part = {.first_index = 0, .index_count = 3, .material = 0};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1}};
    const EngTextureHandle white = {.handle = {.index = 9, .generation = 1}};
    const EngSamplerHandle shadow_sampler = {.handle = {.index = 4, .generation = 1}};
    EngModelGpu gpu = {.parts = &part,
                       .part_count = 1,
                       .materials = &material,
                       .material_count = 1,
                       .white = white,
                       .shadow_sampler = shadow_sampler};
    mat4 identity;
    glm_mat4_identity(identity);
    EngLighting light;
    eng_lighting_default(&light);

    EngModelShadow shadow = {.map = {.handle = {.index = 7, .generation = 2}}};
    const float to_sun[3] = {0.0F, 1.0F, 0.0F};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    ASSERT_TRUE(eng_shadow_fit(&shadow.shadow, to_sun, center, 10.0F, 512U));
    shadow.params = (EngShadowParams){.constant_bias = 2.0F,
                                      .slope_bias = 0.5F,
                                      .normal_bias = 3.0F,
                                      .pcf_radius = 2U,
                                      .strength = 0.75F};
    DrawQueue d;
    init_draw_queue(&d, 2);
    ASSERT_EQ(eng_model_gpu_draw_shadowed(&gpu, &d.queue, (EngPipelineHandle){0}, identity,
                                          identity, &light, &shadow),
              1U);
    const EngDrawCmd *c = &d.queue.commands[0];
    EXPECT_EQ(c->shadow_map.handle.index, 7U);
    EXPECT_EQ(c->shadow_map.handle.generation, 2U);
    EXPECT_EQ(c->shadow_sampler.handle.index, 4U);
    LightCopy u;
    memcpy(&u, &d.uniforms[c->fragment_uniform_offset], sizeof u);
    for (int i = 0; i < 16; i++) {
        EXPECT_NEAR(u.shadow_matrix[i], ((float *)shadow.shadow.shadow_matrix)[i], 1e-6F);
    }
    EXPECT_NEAR(u.shadow_a[0], 0.0F, EPS);
    EXPECT_NEAR(u.shadow_a[1], 1.0F, EPS);
    EXPECT_NEAR(u.shadow_a[3], shadow.shadow.texel_world, 1e-7F);
    EXPECT_NEAR(u.shadow_b[0], 512.0F, EPS);
    EXPECT_NEAR(u.shadow_b[1], 2.0F, EPS);
    EXPECT_NEAR(u.shadow_b[2], 0.5F, EPS);
    EXPECT_NEAR(u.shadow_b[3], 3.0F, EPS);
    EXPECT_NEAR(u.shadow_c[0], 2.0F, EPS);
    EXPECT_NEAR(u.shadow_c[1], 0.75F, EPS);
    EXPECT_NEAR(u.shadow_c[2], 1.0F, EPS); /* ombre active */

    /* des réglages invalides sont corrigés dans la copie envoyée, pas chez l'appelant */
    shadow.params = (EngShadowParams){.constant_bias = -1.0F,
                                      .slope_bias = NAN,
                                      .normal_bias = 1.0F,
                                      .pcf_radius = 99U,
                                      .strength = 7.0F};
    init_draw_queue(&d, 2);
    ASSERT_EQ(eng_model_gpu_draw_shadowed(&gpu, &d.queue, (EngPipelineHandle){0}, identity,
                                          identity, &light, &shadow),
              1U);
    memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.shadow_b[1], 0.0F, EPS);
    EXPECT_NEAR(u.shadow_b[2], 0.0F, EPS);
    EXPECT_NEAR(u.shadow_c[0], (float)ENG_SHADOW_MAX_PCF_RADIUS, EPS);
    EXPECT_NEAR(u.shadow_c[1], 1.0F, EPS);
    EXPECT_EQ(shadow.params.pcf_radius, 99U);

    /* sans ombre (NULL, ou carte nulle) : texture blanche liée, ombre inactive, matrice neutre */
    EngModelShadow none = shadow;
    none.map = ENG_TEXTURE_NULL;
    const EngModelShadow *cases[2] = {NULL, &none};
    for (int i = 0; i < 2; i++) {
        init_draw_queue(&d, 2);
        ASSERT_EQ(eng_model_gpu_draw_shadowed(&gpu, &d.queue, (EngPipelineHandle){0}, identity,
                                              identity, &light, cases[i]),
                  1U);
        EXPECT_EQ(d.queue.commands[0].shadow_map.handle.index, 9U);
        EXPECT_EQ(d.queue.commands[0].shadow_sampler.handle.index, 4U);
        memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
        EXPECT_NEAR(u.shadow_c[2], 0.0F, EPS);
        EXPECT_NEAR(u.shadow_matrix[0], 1.0F, EPS);
        EXPECT_NEAR(u.shadow_matrix[5], 1.0F, EPS);
        EXPECT_NEAR(u.shadow_b[0], 0.0F, EPS);
    }
}

/* La passe d'ombre ne reçoit que la matrice monde -> découpage du soleil multipliée par celle du
 * modèle, une commande par partie, sans texture ni uniforms de fragment. */
UTEST(eng_model_gpu, draw_depth_queues_one_light_mvp_per_part)
{
    EngModelPart parts[2] = {{.first_index = 0, .index_count = 6, .material = 0},
                             {.first_index = 6, .index_count = 3, .material = 0}};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1},
                                    .texture = {.handle = {.index = 5, .generation = 1}}};
    EngModelGpu gpu = {
        .parts = parts, .part_count = 2, .materials = &material, .material_count = 1};
    mat4 model;
    glm_translate_make(model, (vec3){1.0F, 2.0F, 3.0F});
    mat4 light_vp;
    glm_mat4_identity(light_vp);
    light_vp[0][0] = 0.5F;
    light_vp[3][1] = 10.0F;
    DrawQueue d;
    init_draw_queue(&d, 4);
    ASSERT_EQ(eng_model_gpu_draw_depth(&gpu, &d.queue, (EngPipelineHandle){0}, model, light_vp),
              2U);
    for (uint32_t i = 0; i < 2; i++) {
        const EngDrawCmd *c = &d.queue.commands[i];
        EXPECT_EQ(c->count, parts[i].index_count);
        EXPECT_EQ(c->first, parts[i].first_index);
        EXPECT_EQ(c->fragment_uniform_size, 0U);
        EXPECT_EQ(c->texture.handle.generation, 0U);
        EXPECT_EQ(c->shadow_map.handle.generation, 0U);
        ASSERT_EQ(c->uniform_size, (uint32_t)sizeof(mat4));
        float mvp[16];
        memcpy(mvp, &d.uniforms[c->uniform_offset], sizeof mvp);
        mat4 expected;
        glm_mat4_mul(light_vp, model, expected);
        for (int k = 0; k < 16; k++) {
            EXPECT_NEAR(mvp[k], ((float *)expected)[k], 1e-6F);
        }
        EXPECT_NEAR(mvp[0], 0.5F, EPS);         /* l'échelle de la matrice du soleil... */
        EXPECT_NEAR(mvp[12], 0.5F + 0.0F, EPS); /* ...appliquée à la translation du modèle (x) */
        EXPECT_NEAR(mvp[13], 12.0F, EPS);       /* y : 2 + 10 */
    }
}

UTEST(eng_model_gpu, draw_sanitises_a_copy_of_the_lighting_and_defaults_when_it_is_null)
{
    EngModelPart part = {.first_index = 0, .index_count = 3, .material = 0};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1}};
    EngModelGpu gpu = {
        .parts = &part, .part_count = 1, .materials = &material, .material_count = 1};
    mat4 identity;
    glm_mat4_identity(identity);
    DrawQueue d;
    init_draw_queue(&d, 2);
    /* direction non unitaire, intensité négative, brillance nulle : corrigées dans la file */
    EngLighting bad = {.sun_direction = {0.0F, 0.0F, 5.0F},
                       .sun_color = {1.0F, 1.0F, 1.0F},
                       .sun_intensity = -1.0F,
                       .ambient_color = {1.0F, 1.0F, 1.0F},
                       .ambient_intensity = 0.3F,
                       .camera_position = {0.0F, 0.0F, 0.0F}};
    ASSERT_EQ(eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, &bad),
              (uint32_t)1);
    LightCopy u;
    memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.sun_direction[2], 1.0F, EPS);
    EXPECT_NEAR(u.sun_radiance[0], 0.0F, EPS);
    EXPECT_NEAR(bad.sun_intensity, -1.0F, EPS); /* la copie de l'appelant n'a pas été touchée */

    /* NULL : l'éclairage par défaut */
    init_draw_queue(&d, 2);
    ASSERT_EQ(eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, NULL),
              (uint32_t)1);
    EngLighting def;
    eng_lighting_default(&def);
    memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.sun_direction[1], def.sun_direction[1], EPS);
    EXPECT_NEAR(u.sun_radiance[0], def.sun_color[0] * def.sun_intensity, EPS);
    EXPECT_NEAR(u.ambient[0], def.ambient_color[0] * def.ambient_intensity, EPS);
}

UTEST(eng_model_gpu, a_full_queue_accepts_fewer_draws_and_reports_the_rest)
{
    EngModelPart parts[3] = {{0, 3, 0}, {3, 3, 0}, {6, 3, 0}};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1}};
    EngModelGpu gpu = {
        .parts = parts, .part_count = 3, .materials = &material, .material_count = 1};
    mat4 identity;
    glm_mat4_identity(identity);
    DrawQueue d;
    init_draw_queue(&d, 2);
    EXPECT_EQ(eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, NULL),
              (uint32_t)2);
    EXPECT_EQ(d.queue.count, (uint32_t)2);
    EXPECT_EQ(d.queue.dropped, (uint32_t)1);
}

UTEST(eng_model_gpu, pipeline_description_matches_the_vertex_layout)
{
    EngPipelineDesc desc;
    eng_model_pipeline_desc(&desc);
    EXPECT_STREQ(desc.vertex_shader, "model.vert");
    EXPECT_STREQ(desc.fragment_shader, "model.frag");
    EXPECT_EQ(desc.vertex_stride, (uint32_t)sizeof(EngModelVertex));
    EXPECT_EQ(desc.attribute_count, (uint32_t)4);
    EXPECT_EQ(desc.attributes[0].location, (uint32_t)0);
    EXPECT_EQ(desc.attributes[0].format, ENG_VERTEX_FLOAT3);
    EXPECT_EQ(desc.attributes[0].offset, (uint32_t)offsetof(EngModelVertex, position));
    EXPECT_EQ(desc.attributes[1].location, (uint32_t)1);
    EXPECT_EQ(desc.attributes[1].format, ENG_VERTEX_FLOAT3);
    EXPECT_EQ(desc.attributes[1].offset, (uint32_t)offsetof(EngModelVertex, normal));
    EXPECT_EQ(desc.attributes[2].location, (uint32_t)2);
    EXPECT_EQ(desc.attributes[2].format, ENG_VERTEX_FLOAT2);
    EXPECT_EQ(desc.attributes[2].offset, (uint32_t)offsetof(EngModelVertex, uv));
    EXPECT_EQ(desc.attributes[3].location, (uint32_t)3);
    EXPECT_EQ(desc.attributes[3].format, ENG_VERTEX_FLOAT4);
    EXPECT_EQ(desc.attributes[3].offset, (uint32_t)offsetof(EngModelVertex, tangent));
    EXPECT_TRUE(desc.depth_test);
    EXPECT_TRUE(desc.depth_write);
    EXPECT_EQ(desc.cull_mode, ENG_CULL_BACK);
}

UTEST(eng_model_gpu, draw_sends_the_material_factors_to_the_fragment_stage)
{
    EngModelPart part = {.first_index = 0, .index_count = 3, .material = 0};
    EngModelGpuMaterial material = {.base_color = {1, 1, 1, 1},
                                    .metallic = 0.3F,
                                    .roughness = 0.6F,
                                    .normal_scale = 0.5F,
                                    .occlusion_strength = 0.25F,
                                    .emissive = {0.1F, 0.2F, 0.3F},
                                    .has_normal_map = true};
    EngModelGpu gpu = {
        .parts = &part, .part_count = 1, .materials = &material, .material_count = 1};
    mat4 identity;
    glm_mat4_identity(identity);
    DrawQueue d;
    init_draw_queue(&d, 2);
    ASSERT_EQ(eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, NULL),
              (uint32_t)1);
    LightCopy u;
    memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.material[0], 0.3F, EPS);
    EXPECT_NEAR(u.material[1], 0.6F, EPS);
    EXPECT_NEAR(u.material[2], 0.5F, EPS);
    EXPECT_NEAR(u.material[3], 0.25F, EPS);
    EXPECT_NEAR(u.emissive[0], 0.1F, EPS);
    EXPECT_NEAR(u.emissive[1], 0.2F, EPS);
    EXPECT_NEAR(u.emissive[2], 0.3F, EPS);
    EXPECT_NEAR(u.emissive[3], 1.0F, EPS);
    material.has_normal_map = false;
    init_draw_queue(&d, 2);
    ASSERT_EQ(eng_model_gpu_draw(&gpu, &d.queue, (EngPipelineHandle){0}, identity, identity, NULL),
              (uint32_t)1);
    memcpy(&u, &d.uniforms[d.queue.commands[0].fragment_uniform_offset], sizeof u);
    EXPECT_NEAR(u.emissive[3], 0.0F, EPS);
}

/* ── Compléments (mutation) ── */

UTEST(eng_model, computed_normals_are_unit_length_whatever_the_triangle_size)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.no_normals = true, .node_json = "\"scale\":[2,2,2]"}; /* aire 2, pas 0,5 */
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (uint32_t v = 0; v < 3; v++) {
        EXPECT_VEC3(m.vertices[v].normal, 0.0F, 0.0F, 1.0F);
    }
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, a_vertex_no_triangle_uses_gets_the_up_normal)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.no_normals = true, .no_uvs = true, .fourth_vertex = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(m.vertex_count, (uint32_t)4);
    EXPECT_VEC3(m.vertices[3].position, 5.0F, 5.0F, 5.0F);
    EXPECT_VEC3(m.vertices[3].normal, 0.0F, 1.0F, 0.0F);
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 0.0F, 1.0F);
    EXPECT_VEC3(m.bounds_max, 5.0F, 5.0F, 5.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, computed_normals_do_not_depend_on_what_the_arena_held_before)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    /* l'arène a déjà servi : sa mémoire n'est plus à zéro */
    const EngArenaMark mark = eng_arena_mark(&f.arena);
    void *dirty = eng_arena_alloc(&f.arena, (size_t)1 << 16, 16);
    ASSERT_TRUE(dirty != NULL);
    memset(dirty, 0xAB, (size_t)1 << 16);
    eng_arena_reset_to(&f.arena, mark);
    EngModel m;
    const Spec s = {.no_normals = true, .no_uvs = true, .fourth_vertex = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC3(m.vertices[0].normal, 0.0F, 0.0F, 1.0F);
    EXPECT_VEC3(m.vertices[3].normal, 0.0F, 1.0F, 0.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, an_accessor_larger_than_its_buffer_is_refused_before_any_read)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* 3999 sommets annoncés pour 36 octets : la validation de cgltf doit refuser (sinon la lecture
     * dépasserait le tampon : ASan l'arrêterait) */
    const Spec s = {.big_count = true, .no_normals = true, .no_uvs = true, .no_indices = true};
    EXPECT_FALSE(load_spec(&f, &s, &m));
    fixture_close(&f);
}

UTEST(eng_model, inconsistent_attribute_counts_and_types_are_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec counts = {.bad_normal_count = true};
    EXPECT_FALSE(load_spec(&f, &counts, &m));
    const Spec uv_type = {.bad_uv_type = true};
    EXPECT_FALSE(load_spec(&f, &uv_type, &m));
    fixture_close(&f);
}

UTEST(eng_model, only_texcoord_zero_is_read)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.uv_set1 = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_NEAR(m.vertices[1].uv[0], 0.0F, EPS); /* TEXCOORD_1 ignorée : UV nulles */
    EXPECT_NEAR(m.vertices[2].uv[1], 0.0F, EPS);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, data_uris_with_any_base64_padding_decode)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    /* 69 octets (pas de « = »), 70 (« == ») et 71 (« = ») : le PNG tolère les octets en trop */
    for (int extra = 0; extra < 3; extra++) {
        EngModel m;
        const Spec s = {.material = TEXTURED, .texture = true, .png_extra = extra};
        ASSERT_TRUE(load_spec(&f, &s, &m));
        EXPECT_PNG_DECODED(m);
        eng_model_free(&m);
    }
    fixture_close(&f);
}

UTEST(eng_model, two_materials_sharing_an_image_decode_it_once)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.material = TEXTURED, .material2 = TEXTURED, .texture = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_EQ(m.material_count, (uint32_t)3);
    EXPECT_EQ(m.materials[1].image, (int32_t)0);
    EXPECT_EQ(m.materials[2].image, (int32_t)0);
    eng_model_free(&m); /* si l'image avait été décodée deux fois, ASan signalerait la fuite */
    fixture_close(&f);
}

/* ── Matériaux PBR et tangentes (M1-15) ── */

#define EXPECT_VEC4(v, ex, ey, ez, ew)                                                             \
    do {                                                                                           \
        EXPECT_VEC3(v, ex, ey, ez);                                                                \
        EXPECT_NEAR((v)[3], (ew), EPS);                                                            \
    } while (0)

UTEST(eng_model, material_defaults_follow_the_gltf_specification)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* matériau 0 (primitive sans matériau), matériau sans bloc PBR et bloc PBR vide : métal 1,
     * rugosité 1, rien d'émissif, échelles à 1, aucune texture (valeurs de la spécification) */
    const Spec none = {0};
    const Spec plain = {.material = "{\"name\":\"nu\"}"};
    const Spec empty = {.material = "{\"pbrMetallicRoughness\":{}}"};
    const Spec *specs[3] = {&none, &plain, &empty};
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(load_spec(&f, specs[i], &m));
        const EngModelMaterial *mat = &m.materials[m.parts[0].material];
        EXPECT_NEAR(mat->metallic, 1.0F, EPS);
        EXPECT_NEAR(mat->roughness, 1.0F, EPS);
        EXPECT_NEAR(mat->normal_scale, 1.0F, EPS);
        EXPECT_NEAR(mat->occlusion_strength, 1.0F, EPS);
        EXPECT_VEC3(mat->emissive, 0.0F, 0.0F, 0.0F);
        EXPECT_EQ(mat->image, (int32_t)-1);
        EXPECT_EQ(mat->metallic_roughness_image, (int32_t)-1);
        EXPECT_EQ(mat->normal_image, (int32_t)-1);
        EXPECT_EQ(mat->occlusion_image, (int32_t)-1);
        EXPECT_EQ(mat->emissive_image, (int32_t)-1);
        eng_model_free(&m);
    }
    fixture_close(&f);
}

UTEST(eng_model, pbr_factors_scales_and_emissive_strength_are_read)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const Spec s = {.extra_top = "\"extensionsUsed\":[\"KHR_materials_emissive_strength\"]",
                    .material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.2,0.4,0.6,0.5],"
                                "\"metallicFactor\":0.25,\"roughnessFactor\":0.75},"
                                "\"emissiveFactor\":[0.1,0.2,0.3],"
                                "\"extensions\":{\"KHR_materials_emissive_strength\":"
                                "{\"emissiveStrength\":4.0}},"
                                "\"normalTexture\":{\"index\":0,\"scale\":0.5},"
                                "\"occlusionTexture\":{\"index\":1,\"strength\":0.25}}",
                    .texture = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    const EngModelMaterial *mat = &m.materials[1];
    EXPECT_NEAR(mat->base_color[2], 0.6F, EPS);
    EXPECT_NEAR(mat->metallic, 0.25F, EPS);
    EXPECT_NEAR(mat->roughness, 0.75F, EPS);
    EXPECT_NEAR(mat->normal_scale, 0.5F, EPS);
    EXPECT_NEAR(mat->occlusion_strength, 0.25F, EPS);
    EXPECT_VEC3(mat->emissive, 0.4F, 0.8F,
                1.2F); /* facteur x force (KHR_materials_emissive_strength) */
    eng_model_free(&m);

    /* sans l'extension, le facteur est pris tel quel */
    const Spec plain = {.material = "{\"emissiveFactor\":[0.1,0.2,0.3]}"};
    ASSERT_TRUE(load_spec(&f, &plain, &m));
    EXPECT_VEC3(m.materials[1].emissive, 0.1F, 0.2F, 0.3F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, each_texture_role_gets_its_image_and_unused_images_stay_undecoded)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* texture 0 -> image 0, texture 1 -> image 1 */
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},"
                                "\"metallicRoughnessTexture\":{\"index\":1}},"
                                "\"normalTexture\":{\"index\":1},"
                                "\"occlusionTexture\":{\"index\":0},"
                                "\"emissiveTexture\":{\"index\":1}}",
                    .texture = true};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    const EngModelMaterial *mat = &m.materials[1];
    EXPECT_EQ(mat->image, (int32_t)0);
    EXPECT_EQ(mat->metallic_roughness_image, (int32_t)1);
    EXPECT_EQ(mat->normal_image, (int32_t)1);
    EXPECT_EQ(mat->occlusion_image, (int32_t)0);
    EXPECT_EQ(mat->emissive_image, (int32_t)1);
    EXPECT_TRUE(m.images[0].pixels != NULL);
    EXPECT_TRUE(m.images[1].pixels != NULL);
    eng_model_free(&m);

    /* seule une texture de normale : l'image 1 est décodée, l'image 0 non */
    const Spec only_normal = {.material = "{\"normalTexture\":{\"index\":1}}", .texture = true};
    ASSERT_TRUE(load_spec(&f, &only_normal, &m));
    EXPECT_EQ(m.materials[1].normal_image, (int32_t)1);
    EXPECT_EQ(m.materials[1].image, (int32_t)-1);
    EXPECT_TRUE(m.images[0].pixels == NULL);
    EXPECT_TRUE(m.images[1].pixels != NULL);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, computed_tangent_points_along_increasing_u_and_signs_the_bitangent)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* Triangle de référence : positions (0,0,0) (1,0,0) (0,1,0), UV (0,0) (1,0) (0,1), N = +Z.
     * e1 = (1,0,0), e2 = (0,1,0), (du1,dv1) = (1,0), (du2,dv2) = (0,1), det = 1 :
     *   T = dP/du = (1,0,0) ; dP/dv = (0,1,0), donc B = -dP/dv = (0,-1,0).
     * cross(N, T) = Z x X = +Y, opposé à B : w = -1. Ici v croît vers +Y, c'est-à-dire que le
     * « haut » de l'image (v décroissant) est -Y : l'image est inversée par rapport à la vue. */
    const Spec s = {0};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (int v = 0; v < 3; v++) {
        EXPECT_VEC4(m.vertices[v].tangent, 1.0F, 0.0F, 0.0F, -1.0F);
    }
    eng_model_free(&m);

    /* v décroît le long de +Y : UV (0,1) (1,1) (0,0). (du2,dv2) = (0,-1), det = -1 :
     *   T = (e1*dv2 - e2*dv1) / det = (-1,0,0) / -1 = (1,0,0) ;
     *   dP/dv = (e2*du1 - e1*du2) / det = (0,1,0) / -1 = (0,-1,0) donc B = (0,1,0) = cross(N,T) :
     *   w = +1 (l'image est dans le bon sens). */
    const float uvs[6] = {0, 1, 1, 1, 0, 0};
    const Spec upright = {.uv_values = uvs};
    ASSERT_TRUE(load_spec(&f, &upright, &m));
    for (int v = 0; v < 3; v++) {
        EXPECT_VEC4(m.vertices[v].tangent, 1.0F, 0.0F, 0.0F, 1.0F);
    }
    eng_model_free(&m);

    /* u inversé (UV (1,1) (0,1) (1,0)) : T pointe vers -X, B reste +Y, cross(N, T) = -Y : w = -1 */
    const float mirrored[6] = {1, 1, 0, 1, 1, 0};
    const Spec flipped_u = {.uv_values = mirrored};
    ASSERT_TRUE(load_spec(&f, &flipped_u, &m));
    EXPECT_VEC4(m.vertices[0].tangent, -1.0F, 0.0F, 0.0F, -1.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, computed_tangent_follows_sheared_uvs)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* UV (0,0) (1 ; 0,25) (0,5 ; 1) sur les positions (0,0,0) (1,0,0) (0,1,0) : u = x + 0,5 y et
     * v = 0,25 x + y. L'inverse de [[1 ; 0,5], [0,25 ; 1]] est [[1 ; -0,5], [-0,25 ; 1]] / 0,875,
     * donc dP/du = (1 ; -0,25 ; 0) / 0,875  ->  T = (0,97014 ; -0,24254 ; 0) normalisé, dP/dv =
     * (-0,5 ; 1 ; 0) / 0,875   ->  B = -dP/dv = (0,5714 ; -1,1429 ; 0). cross(N, T) = Z x T =
     * (0,24254 ; 0,97014 ; 0), dont le produit scalaire avec B vaut -0,97 : w = -1. Ici dv1 et du2
     * sont NON nuls : une erreur de signe dans T = (e1 dv2 - e2 dv1) / det ne passe pas inaperçue
     * (elle est invisible sur le triangle de référence, où dv1 = du2 = 0). */
    const float uvs[6] = {0.0F, 0.0F, 1.0F, 0.25F, 0.5F, 1.0F};
    const Spec s = {.uv_values = uvs};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (int v = 0; v < 3; v++) {
        EXPECT_VEC4(m.vertices[v].tangent, 0.9701425F, -0.2425356F, 0.0F, -1.0F);
    }
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, computed_tangent_is_orthogonalised_against_a_tilted_normal)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* N = (0,6 ; 0 ; 0,8), T brut (1,0,0) : T - N (N.T) = (1,0,0) - 0,6 (0,6 ; 0 ; 0,8) =
     * (0,64 ; 0 ; -0,48), de norme 0,8, soit (0,8 ; 0 ; -0,6). cross(N, T) = (0 ; 1 ; 0) = B (UV
     * à v décroissant) : w = +1. */
    const float uvs[6] = {0, 1, 1, 1, 0, 0};
    const Spec s = {.normal = {0.6F, 0.0F, 0.8F}, .uv_values = uvs};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (int v = 0; v < 3; v++) {
        const float *t = m.vertices[v].tangent;
        EXPECT_VEC4(t, 0.8F, 0.0F, -0.6F, 1.0F);
        const float *n = m.vertices[v].normal;
        EXPECT_NEAR((t[0] * n[0]) + (t[1] * n[1]) + (t[2] * n[2]), 0.0F, EPS);
        EXPECT_NEAR((t[0] * t[0]) + (t[1] * t[1]) + (t[2] * t[2]), 1.0F, EPS);
    }
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, without_usable_uvs_the_tangent_is_an_axis_orthogonal_to_the_normal)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* sans UV (tous nuls) : aucun triangle n'oriente la tangente. N = +Z (normale calculée du
     * triangle) : axe X ; N = +X : axe Y (X est trop aligné avec la normale) ; w = +1 */
    const Spec up = {.no_normals = true, .no_uvs = true, .fourth_vertex = true};
    ASSERT_TRUE(load_spec(&f, &up, &m));
    EXPECT_VEC4(m.vertices[0].tangent, 1.0F, 0.0F, 0.0F, 1.0F);
    /* le 4e sommet n'est utilisé par aucun triangle : sa normale est (0,1,0), donc l'axe X */
    EXPECT_VEC4(m.vertices[3].tangent, 1.0F, 0.0F, 0.0F, 1.0F);
    eng_model_free(&m);
    const Spec side = {.no_uvs = true, .normal = {1.0F, 0.0F, 0.0F}};
    ASSERT_TRUE(load_spec(&f, &side, &m));
    EXPECT_VEC4(m.vertices[0].tangent, 0.0F, 1.0F, 0.0F, 1.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, file_tangents_are_normalised_kept_and_transformed_with_the_node)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float tangents[12] = {2, 0, 0, 1, 0, 3, 0, -1, 0, 0, 4, 1};
    const Spec s = {.tangent_values = tangents};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    /* normalisées, w = signe de la 4e composante, UV du fichier ignorées pour le calcul */
    EXPECT_VEC4(m.vertices[0].tangent, 1.0F, 0.0F, 0.0F, 1.0F);
    EXPECT_VEC4(m.vertices[1].tangent, 0.0F, 1.0F, 0.0F, -1.0F);
    EXPECT_VEC4(m.vertices[2].tangent, 0.0F, 0.0F, 1.0F, 1.0F);
    eng_model_free(&m);

    /* rotation de 90° autour de Z (quaternion (0, 0, sin 45°, cos 45°)) : X -> Y */
    const Spec turned = {.tangent_values = tangents,
                         .node_json = "\"rotation\":[0,0,0.70710678,0.70710678]"};
    ASSERT_TRUE(load_spec(&f, &turned, &m));
    EXPECT_VEC4(m.vertices[0].tangent, 0.0F, 1.0F, 0.0F, 1.0F);
    eng_model_free(&m);

    /* échelle (-1, 1, 1) : la tangente (1,0,0) devient (-1,0,0), la normale +Z reste +Z, et
     * cross(N', T') = -Y : pour que la bitangente reste +Y, w passe de +1 à -1 */
    const Spec mirror = {.tangent_values = tangents, .node_json = "\"scale\":[-1,1,1]"};
    ASSERT_TRUE(load_spec(&f, &mirror, &m));
    EXPECT_VEC4(m.vertices[0].tangent, -1.0F, 0.0F, 0.0F, -1.0F);
    EXPECT_VEC4(m.vertices[1].tangent, 0.0F, 1.0F, 0.0F, 1.0F); /* w = -1 devient +1 */
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, unusable_file_tangents_fall_back_to_computed_ones_and_bad_counts_are_refused)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    const float zero[12] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}; /* tangentes nulles */
    const Spec s = {.tangent_values = zero};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC4(m.vertices[1].tangent, 1.0F, 0.0F, 0.0F, -1.0F); /* celles du calcul par les UV */
    eng_model_free(&m);

    const float ok[12] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    const Spec bad = {.tangent_values = ok, .bad_tangent_count = true};
    EXPECT_FALSE(load_spec(&f, &bad, &m));
    EXPECT_TRUE(m.vertices == NULL);
    fixture_close(&f);
}

UTEST(eng_model, computed_tangents_average_over_the_triangles_sharing_a_vertex)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    EngModel m;
    /* Même triangle, deux fois (deux nœuds) : deux copies, chacune avec son T. Puis un triangle
     * dont UV sont dégénérés (tous égaux) : aucun T, repli sur un axe, sans NaN. */
    const float same[6] = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
    const Spec s = {.uv_values = same};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    for (int v = 0; v < 3; v++) {
        const float *t = m.vertices[v].tangent;
        EXPECT_TRUE(isfinite(t[0]) && isfinite(t[1]) && isfinite(t[2]) && isfinite(t[3]));
        EXPECT_NEAR((t[0] * t[0]) + (t[1] * t[1]) + (t[2] * t[2]), 1.0F, EPS);
        EXPECT_NEAR(t[2], 0.0F, EPS); /* orthogonale à N = +Z */
    }
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model, computed_tangents_do_not_depend_on_what_the_arena_held_before)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const EngArenaMark mark = eng_arena_mark(&f.arena);
    void *dirty = eng_arena_alloc(&f.arena, (size_t)1 << 16, 16);
    ASSERT_TRUE(dirty != NULL);
    memset(dirty, 0xAB, (size_t)1 << 16);
    eng_arena_reset_to(&f.arena, mark);
    const EngArenaMark scratch_mark = eng_arena_mark(&f.scratch);
    void *dirty_scratch = eng_arena_alloc(&f.scratch, (size_t)1 << 16, 16);
    ASSERT_TRUE(dirty_scratch != NULL);
    memset(dirty_scratch, 0xCD, (size_t)1 << 16);
    eng_arena_reset_to(&f.scratch, scratch_mark);
    EngModel m;
    const Spec s = {0};
    ASSERT_TRUE(load_spec(&f, &s, &m));
    EXPECT_VEC4(m.vertices[0].tangent, 1.0F, 0.0F, 0.0F, -1.0F);
    EXPECT_VEC4(m.vertices[2].tangent, 1.0F, 0.0F, 0.0F, -1.0F);
    eng_model_free(&m);
    fixture_close(&f);
}

/* ── Boîte procédurale (M1-17) : le sol et la boîte des ombres ── */

UTEST(eng_model_box, has_24_vertices_outward_triangles_and_the_requested_bounds)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const float lo[3] = {-1.0F, 0.0F, -2.0F};
    const float hi[3] = {3.0F, 2.0F, 4.0F};
    EngModel m;
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    EXPECT_EQ(m.vertex_count, 24U);
    EXPECT_EQ(m.index_count, 36U);
    EXPECT_EQ(m.part_count, 1U);
    EXPECT_EQ(m.material_count, 2U);
    EXPECT_EQ(m.parts[0].material, 1U);
    EXPECT_EQ(m.parts[0].index_count, 36U);
    for (int a = 0; a < 3; a++) {
        EXPECT_NEAR(m.bounds_min[a], lo[a], EPS);
        EXPECT_NEAR(m.bounds_max[a], hi[a], EPS);
    }
    const float center[3] = {1.0F, 1.0F, 1.0F};
    float lo_seen[3] = {1e9F, 1e9F, 1e9F};
    float hi_seen[3] = {-1e9F, -1e9F, -1e9F};
    for (uint32_t v = 0; v < m.vertex_count; v++) {
        for (int a = 0; a < 3; a++) {
            lo_seen[a] = fminf(lo_seen[a], m.vertices[v].position[a]);
            hi_seen[a] = fmaxf(hi_seen[a], m.vertices[v].position[a]);
        }
        const float *n = m.vertices[v].normal;
        EXPECT_NEAR((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]), 1.0F, EPS);
        /* la normale d'un sommet pointe hors du centre de la boîte */
        float outward = 0.0F;
        for (int a = 0; a < 3; a++) {
            outward += n[a] * (m.vertices[v].position[a] - center[a]);
        }
        EXPECT_GT(outward, 0.0F);
        /* tangente unitaire et orthogonale à la normale */
        const float *t = m.vertices[v].tangent;
        EXPECT_NEAR((t[0] * n[0]) + (t[1] * n[1]) + (t[2] * n[2]), 0.0F, EPS);
        EXPECT_NEAR((t[0] * t[0]) + (t[1] * t[1]) + (t[2] * t[2]), 1.0F, EPS);
    }
    for (int a = 0; a < 3; a++) {
        EXPECT_NEAR(lo_seen[a], lo[a], EPS);
        EXPECT_NEAR(hi_seen[a], hi[a], EPS);
    }
    /* chaque triangle est dans le sens direct vu de l'extérieur : sa normale géométrique est celle
     * de ses sommets ; la surface totale est celle de la boîte (2 * (4*2 + 4*6 + 2*6) = 88) */
    float area = 0.0F;
    for (uint32_t i = 0; i < m.index_count; i += 3) {
        ASSERT_LT(m.indices[i], 24U);
        const EngModelVertex *a = &m.vertices[m.indices[i]];
        const EngModelVertex *b = &m.vertices[m.indices[i + 1]];
        const EngModelVertex *c = &m.vertices[m.indices[i + 2]];
        float e1[3];
        float e2[3];
        for (int k = 0; k < 3; k++) {
            e1[k] = b->position[k] - a->position[k];
            e2[k] = c->position[k] - a->position[k];
        }
        const float cross[3] = {(e1[1] * e2[2]) - (e1[2] * e2[1]),
                                (e1[2] * e2[0]) - (e1[0] * e2[2]),
                                (e1[0] * e2[1]) - (e1[1] * e2[0])};
        const float along =
            (cross[0] * a->normal[0]) + (cross[1] * a->normal[1]) + (cross[2] * a->normal[2]);
        EXPECT_GT(along, 0.0F);
        area += 0.5F * along;
    }
    EXPECT_NEAR(area, 88.0F, 1e-3F);
    eng_model_free(&m);
    fixture_close(&f);
}

UTEST(eng_model_box, uses_the_given_material_or_the_gltf_default)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const float lo[3] = {0.0F, 0.0F, 0.0F};
    const float hi[3] = {1.0F, 1.0F, 1.0F};
    EngModel m;
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, NULL, &m));
    EXPECT_NEAR(m.materials[1].metallic, 1.0F, EPS); /* défaut glTF */
    EXPECT_NEAR(m.materials[1].roughness, 1.0F, EPS);
    EXPECT_EQ(m.materials[1].image, -1);
    EngModelMaterial own = m.materials[1];
    own.metallic = 0.25F;
    own.roughness = 0.5F;
    own.base_color[0] = 0.1F;
    EngModel m2;
    ASSERT_TRUE(eng_model_make_box(&f.arena, lo, hi, &own, &m2));
    EXPECT_NEAR(m2.materials[1].metallic, 0.25F, EPS);
    EXPECT_NEAR(m2.materials[1].roughness, 0.5F, EPS);
    EXPECT_NEAR(m2.materials[1].base_color[0], 0.1F, EPS);
    EXPECT_NEAR(m2.materials[0].metallic, 1.0F, EPS); /* le matériau 0 reste le défaut */
    fixture_close(&f);
}

UTEST(eng_model_box, refuses_empty_inverted_or_non_finite_boxes_and_restores_the_arena)
{
    Fixture f;
    ASSERT_TRUE(fixture_open(&f));
    const EngArenaMark mark = eng_arena_mark(&f.arena);
    const float lo[3] = {0.0F, 0.0F, 0.0F};
    const float flat[3] = {1.0F, 0.0F, 1.0F};
    const float inverted[3] = {1.0F, -1.0F, 1.0F};
    const float nan[3] = {1.0F, NAN, 1.0F};
    const float inf[3] = {1.0F, INFINITY, 1.0F};
    const float *bad[4] = {flat, inverted, nan, inf};
    for (int i = 0; i < 4; i++) {
        EngModel m;
        EXPECT_FALSE(eng_model_make_box(&f.arena, lo, bad[i], NULL, &m));
        EXPECT_EQ(m.vertex_count, 0U);
    }
    /* un refus ne consomme rien dans l'arène ; une arène trop petite non plus */
    EXPECT_EQ(f.arena.used, mark.used);
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 256));
    const float hi[3] = {1.0F, 1.0F, 1.0F};
    EngModel m;
    EXPECT_FALSE(eng_model_make_box(&tiny, lo, hi, NULL, &m));
    EXPECT_EQ(tiny.used, 0U);
    eng_arena_shutdown(&tiny);
    fixture_close(&f);
}
