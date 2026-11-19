#include <SDL3/SDL.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/anim_clip.h"
#include "engine/engine.h"
#include "engine/files.h"
#include "engine/heightmap.h"
#include "engine/image.h"
#include "engine/mesh_file.h"
#include "engine/model.h"
#include "engine/skeleton.h"
#include "engine/skin_file.h"
#include "engine/tex_file.h"

/* assetc : le cuiseur d'assets (M2-02). Transforme les sources (glTF, images) en fichiers binaires
 * que le moteur lit sans analyse (docs/asset-formats.md).
 *
 *   assetc mesh <entrée.gltf|.glb> <sortie.mesh> [--bench N] [--raw] [--skin N]
 *               [--depfile <fichier.d>]
 *       Écrit les textures <sortie>.<k>.tex (une par texture utilisée, BC7 avec ses mips, RGBA8
 *       avec --raw) PUIS la sortie : un .mesh présent est donc toujours complet. Les textures
 *       périmées d'une cuisson précédente sont supprimées. --bench N mesure ensuite N chargements
 *       du glTF et N du fichier cuit. --skin N (M4-21) cuit un personnage animé : seuls les
 *       maillages du skin N sont gardés (en pose de liaison) et leurs poids sont écrits dans
 *       <sortie>.skin, AVANT le .mesh ; les indices d'articulation sont ceux du .skel du même skin
 *       (assetc skel --skin N). Incompatible avec --bench.
 *   assetc tex <entrée.png|.jpg|...> <sortie.tex> [--linear] [--raw] [--depfile <fichier.d>]
 *   assetc skel <entrée.gltf|.glb> <sortie.skel> [--skin N] [--depfile <fichier.d>]
 *   assetc anim <entrée.gltf|.glb> <sortie.anim> [--anim N | --anim-name NOM] [--skin N]
 *               [--skeleton chemin.skel] [--depfile <fichier.d>]
 *       Une image en .tex BC7 avec ses mips (RGBA8 avec --raw). sRGB par défaut ; --linear pour
 *       des données (normales, rugosité...).
 *   assetc heightmap <sortie.hmap> [--size N] [--spacing M] [--alt-min A] [--alt-max B] [--seed S]
 *                    [--wavelength W] [--octaves K] [--lacunarity L] [--gain G] [--ridged]
 *                    [--png <sortie.png>]
 *       Génère un champ de hauteurs par bruit fractal (M3-01, docs/terrain.md). Défauts : 1025,
 *       1 m, 0 à 120 m, graine 1337, 512 m, 6 octaves. --png écrit aussi une image 16 bits.
 *   assetc heightmap-import <entrée.png> <sortie.hmap> [--spacing M] [--alt-min A] [--alt-max B]
 *       Importe une image carrée (PNG 16 bits de préférence) comme champ de hauteurs.
 *   assetc info <fichier.mesh|.tex|.skel|.anim|.skin|.hmap>
 *       Affiche l'en-tête, les sections et le contenu vérifié.
 *
 * --depfile (M2-04) écrit, au format Makefile que lisent Ninja et make, la liste des fichiers lus
 * (« <sortie>: <entrée> <tampons et images externes>... », la sortie telle qu'elle est écrite sur
 * la ligne de commande) : le système de build recuit si l'un d'eux change.
 *
 * Les chemins sont relatifs au dossier COURANT (pas à celui de l'exécutable, contrairement au
 * moteur). Sortie déterministe : mêmes entrées, mêmes octets. Code de retour : 0 réussite,
 * 1 échec (message sur stderr), 2 mauvais usage. */

#define ARENA_BYTES                                                                                \
    ((size_t)512 << 20) /* réservés d'avance ; seule la partie utilisée est touchée */

typedef struct Tool {
    EngArena model;   /* tableaux du modèle */
    EngArena scratch; /* fichiers lus, calculs */
    EngArena out;     /* fichiers produits */
} Tool;

static bool tool_open(Tool *t)
{
    if (!eng_arena_init(&t->model, "assetc-model", ARENA_BYTES)) {
        return false;
    }
    if (!eng_arena_init(&t->scratch, "assetc-scratch", ARENA_BYTES)) {
        eng_arena_shutdown(&t->model);
        return false;
    }
    if (!eng_arena_init(&t->out, "assetc-out", ARENA_BYTES)) {
        eng_arena_shutdown(&t->scratch);
        eng_arena_shutdown(&t->model);
        return false;
    }
    return true;
}

static void tool_close(Tool *t)
{
    eng_arena_shutdown(&t->out);
    eng_arena_shutdown(&t->scratch);
    eng_arena_shutdown(&t->model);
}

/* Chemin absolu : un chemin déjà absolu tel quel, sinon relatif au dossier courant. */
static bool absolute_path(const char *arg, char *out, size_t size)
{
    if (eng_path_is_absolute(arg)) {
        return SDL_snprintf(out, size, "%s", arg) < (int)size;
    }
    char *cwd = SDL_GetCurrentDirectory();
    if (cwd == NULL) {
        fprintf(stderr, "assetc : dossier courant inconnu : %s\n", SDL_GetError());
        return false;
    }
    const bool fits = SDL_snprintf(out, size, "%s%s", cwd, arg) < (int)size;
    SDL_free(cwd); /* SDL_GetCurrentDirectory se termine par un séparateur */
    if (!fits) {
        fprintf(stderr, "assetc : chemin trop long : %s\n", arg);
    }
    return fits;
}

static bool has_suffix(const char *text, const char *suffix)
{
    const size_t a = strlen(text);
    const size_t b = strlen(suffix);
    return a >= b && SDL_strcasecmp(text + a - b, suffix) == 0;
}

/* Écrit `size` octets dans `path` après avoir créé son dossier. */
static bool write_output(const char *path, const void *data, size_t size)
{
    char dir[1024];
    SDL_snprintf(dir, sizeof dir, "%s", path);
    size_t keep = 0;
    for (size_t i = 0; dir[i] != '\0'; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            keep = i;
        }
    }
    if (keep > 0) {
        dir[keep] = '\0';
        if (!SDL_CreateDirectory(dir)) {
            fprintf(stderr, "assetc : dossier %s impossible : %s\n", dir, SDL_GetError());
            return false;
        }
    }
    if (!SDL_SaveFile(path, data, size)) {
        fprintf(stderr, "assetc : écriture de %s impossible : %s\n", path, SDL_GetError());
        return false;
    }
    return true;
}

/* Texte d'un fichier de dépendances au format Makefile : « cible: dép1 dép2 ... ». Les espaces,
 * « # » et « $ » sont protégés comme l'attendent Ninja et make. */
static size_t escape_dependency(char *out, const char *text)
{
    size_t n = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p == ' ' || *p == '#') {
            out[n++] = '\\';
        } else if (*p == '$') {
            out[n++] = '$';
        }
        out[n++] = *p;
    }
    return n;
}

static bool write_depfile(Tool *t, const char *depfile_arg, const char *target,
                          const char *const *deps, uint32_t count)
{
    char depfile[1024];
    if (!absolute_path(depfile_arg, depfile, sizeof depfile)) {
        return false;
    }
    size_t room = (strlen(target) * 2U) + 8U;
    for (uint32_t i = 0; i < count; i++) {
        room += (strlen(deps[i]) * 2U) + 1U;
    }
    const EngArenaMark mark = eng_arena_mark(&t->out);
    char *text = eng_arena_alloc(&t->out, room, 1);
    bool ok = false;
    if (text != NULL) {
        size_t n = escape_dependency(text, target);
        text[n++] = ':';
        for (uint32_t i = 0; i < count; i++) {
            text[n++] = ' ';
            n += escape_dependency(text + n, deps[i]);
        }
        text[n++] = '\n';
        ok = write_output(depfile, text, n);
    } else {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
    }
    eng_arena_reset_to(&t->out, mark);
    return ok;
}

/* ── mesh ── */

/* Nom de fichier sans dossier ni extension « .mesh ». */
static void output_stem(const char *path, char *stem, size_t size)
{
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }
    SDL_snprintf(stem, size, "%s", name);
    if (has_suffix(stem, ".mesh")) {
        stem[strlen(stem) - 5U] = '\0';
    }
}

static bool parse_gltf(Tool *t, const char *path, EngModel *model)
{
    const EngArenaMark mark = eng_arena_mark(&t->scratch);
    EngFileData file;
    bool ok = false;
    if (!eng_file_read_all(&t->scratch, path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", path);
    } else {
        ok = eng_model_parse(&t->model, &t->scratch, file.data, file.size, path, model);
    }
    eng_arena_reset_to(&t->scratch, mark);
    return ok;
}

static double ms_since(uint64_t start)
{
    return (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
           (double)SDL_GetPerformanceFrequency();
}

/* N chargements du glTF et N du fichier cuit : durée moyenne de chacun. */
static bool bench_loads(Tool *t, const char *gltf_path, const char *mesh_path, int runs)
{
    double gltf_total = 0.0;
    double mesh_total = 0.0;
    for (int i = 0; i < runs; i++) {
        EngModel model;
        eng_arena_reset(&t->model);
        uint64_t start = SDL_GetPerformanceCounter();
        if (!parse_gltf(t, gltf_path, &model)) {
            return false;
        }
        gltf_total += ms_since(start);
        eng_model_free(&model);

        eng_arena_reset(&t->model);
        start = SDL_GetPerformanceCounter();
        if (!eng_mesh_load(&t->model, &t->scratch, mesh_path, &model)) {
            return false;
        }
        mesh_total += ms_since(start);
        eng_model_free(&model);
    }
    const double gltf_ms = gltf_total / runs;
    const double mesh_ms = mesh_total / runs;
    printf("chargement (moyenne de %d) : glTF %.1f ms, cuit %.1f ms, soit %.1f fois plus rapide\n",
           runs, gltf_ms, mesh_ms, mesh_ms > 0.0 ? gltf_ms / mesh_ms : 0.0);
    return true;
}

/* Supprime <dossier><stem>.<k>.tex pour k >= `first` : textures d'une cuisson précédente qui
 * avait plus de textures que celle-ci. */
static void remove_stale_textures(const char *dir, const char *stem, uint32_t first)
{
    for (uint32_t k = first;; k++) {
        char path[1400];
        SDL_snprintf(path, sizeof path, "%s%s.%u.tex", dir, stem, (unsigned)k);
        if (!eng_file_exists(path) || !SDL_RemovePath(path)) {
            return;
        }
    }
}

/* Dépendances d'un glTF : le fichier lui-même, ses tampons et images externes. */
static bool mesh_depfile(Tool *t, const char *depfile_arg, const char *target, const char *in_path)
{
    const EngArenaMark scratch_mark = eng_arena_mark(&t->scratch);
    const EngArenaMark out_mark = eng_arena_mark(&t->out);
    EngFileData file;
    const char **external = NULL;
    uint32_t external_count = 0;
    bool ok = false;
    if (!eng_file_read_all(&t->scratch, in_path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", in_path);
    } else if (!eng_model_dependencies(&t->out, file.data, file.size, in_path, &external,
                                       &external_count)) {
        fprintf(stderr, "assetc : dépendances du glTF illisibles (voir le journal)\n");
    } else {
        const char **all = (const char **)eng_arena_alloc(
            &t->out, ((size_t)external_count + 1U) * sizeof *all, alignof(const char *));
        if (all == NULL) {
            fprintf(stderr, "assetc : mémoire insuffisante\n");
        } else {
            all[0] = in_path;
            for (uint32_t i = 0; i < external_count; i++) {
                all[i + 1U] = external[i];
            }
            ok = write_depfile(t, depfile_arg, target, all, external_count + 1U);
        }
    }
    eng_arena_reset_to(&t->out, out_mark);
    eng_arena_reset_to(&t->scratch, scratch_mark);
    return ok;
}

/* Maillage d'un personnage : seuls les nœuds du skin, poids recodés avec la permutation du
 * squelette du MÊME skin (comme assetc skel). `*skin` et `*joint_count` : voir eng_skin_encode. */
static bool parse_gltf_skinned(Tool *t, const char *path, uint32_t skin_index, EngModel *model,
                               EngSkinVertex **skin, uint32_t *joint_count)
{
    const EngArenaMark mark = eng_arena_mark(&t->scratch);
    EngFileData file;
    bool ok = false;
    if (!eng_file_read_all(&t->scratch, path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", path);
    } else {
        const uint32_t skins = eng_skeleton_gltf_skin_count(file.data, file.size);
        EngSkeleton skeleton = {0};
        uint32_t *remap = NULL;
        if (skins == 0) {
            fprintf(stderr, "assetc : %s ne contient aucun skin (squelette) glTF\n", path);
        } else if (skin_index >= skins) {
            fprintf(stderr, "assetc : skin %u inexistant : %s en contient %u (0 à %u)\n",
                    (unsigned)skin_index, path, (unsigned)skins, (unsigned)(skins - 1U));
        } else if (eng_skeleton_from_gltf(&t->model, &t->scratch, file.data, file.size, path,
                                          skin_index, &skeleton, &remap) &&
                   eng_model_parse_skinned(&t->model, &t->scratch, file.data, file.size, path,
                                           skin_index, remap, skeleton.joint_count, model, skin)) {
            *joint_count = skeleton.joint_count;
            ok = true;
        }
    }
    eng_arena_reset_to(&t->scratch, mark);
    return ok;
}

static int cmd_mesh(const char *in_arg, const char *out_arg, int bench_runs, bool raw,
                    bool has_skin, uint32_t skin_index, const char *depfile)
{
    char in_path[1024];
    char out_path[1024];
    if (!absolute_path(in_arg, in_path, sizeof in_path) ||
        !absolute_path(out_arg, out_path, sizeof out_path)) {
        return 1;
    }
    if (!has_suffix(out_path, ".mesh")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .mesh : %s\n", out_arg);
        return 2;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngModel model = {0};
    EngSkinVertex *skin = NULL;
    uint32_t joint_count = 0;
    const uint64_t start = SDL_GetPerformanceCounter();
    if (has_skin ? !parse_gltf_skinned(&t, in_path, skin_index, &model, &skin, &joint_count)
                 : !parse_gltf(&t, in_path, &model)) {
        goto done;
    }

    /* textures utilisées : une image par rôle (sRGB ou linéaire), nommées <sortie>.<k>.tex */
    char stem[256];
    output_stem(out_path, stem, sizeof stem);
    EngMeshPlan plan;
    if (!eng_mesh_plan(&t.model, &model, stem, &plan)) {
        fprintf(stderr, "assetc : le modèle ne peut pas être cuit (voir le journal)\n");
        goto done;
    }
    const uint32_t use_count = plan.texture_count;
    const void *file = NULL;
    size_t size = 0;
    if (!eng_mesh_encode(&t.out, &t.scratch, &plan.model, plan.textures, use_count, &file, &size)) {
        fprintf(stderr, "assetc : le modèle ne peut pas être cuit (voir le journal)\n");
        goto done;
    }
    /* le .mesh est écrit en DERNIER (après les textures) : un .mesh présent est toujours complet,
     * et une cuisson interrompue ne laisse pas un fichier qui paraîtrait à jour */
    printf("%s : %u sommets, %u index, %u parties, %u matériaux, %u textures, %zu octets\n",
           out_arg, (unsigned)model.vertex_count, (unsigned)model.index_count,
           (unsigned)model.part_count, (unsigned)model.material_count, (unsigned)use_count, size);

    /* chaque texture : l'image du glTF, dans le bon espace de couleur */
    char dir[1024];
    SDL_snprintf(dir, sizeof dir, "%s", out_path);
    size_t keep = 0;
    for (size_t i = 0; dir[i] != '\0'; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            keep = i + 1U;
        }
    }
    dir[keep] = '\0';
    size_t tex_bytes = 0;
    size_t raw_bytes = 0; /* ce que les mêmes textures coûteraient en RGBA8 */
    for (uint32_t i = 0; i < use_count; i++) {
        const EngArenaMark mark = eng_arena_mark(&t.out);
        const EngImage *image = &model.images[plan.source_images[i]];
        const void *tex = NULL;
        size_t tex_size = 0;
        raw_bytes += eng_image_mip_chain_size(image->width, image->height,
                                              eng_image_mip_count(image->width, image->height));
        const bool encoded =
            raw ? eng_tex_encode(&t.out, &t.scratch, image, plan.textures[i].srgb, &tex, &tex_size)
                : eng_tex_encode_bc7(&t.out, &t.scratch, image, plan.textures[i].srgb, &tex,
                                     &tex_size);
        if (!encoded) {
            fprintf(stderr, "assetc : texture %u impossible à cuire\n", (unsigned)i);
            goto done;
        }
        char tex_path[1400];
        SDL_snprintf(tex_path, sizeof tex_path, "%s%s", dir, plan.textures[i].path);
        if (!write_output(tex_path, tex, tex_size)) {
            goto done;
        }
        tex_bytes += tex_size;
        eng_arena_reset_to(&t.out, mark);
    }
    printf("%u textures (%s) : %zu octets, contre %zu en RGBA8 avec mips, soit %.2f fois moins "
           "(%.1f ms au total)\n",
           (unsigned)use_count, raw ? "RGBA8" : "BC7", tex_bytes, raw_bytes,
           tex_bytes > 0 ? (double)raw_bytes / (double)tex_bytes : 0.0, ms_since(start));
    /* poids : AVANT le .mesh (même règle : un .mesh présent est complet) ; sans --skin, un .skin
     * d'une cuisson précédente est supprimé (il ne correspondrait plus au maillage) */
    char skin_path[1400];
    SDL_snprintf(skin_path, sizeof skin_path, "%s%s.skin", dir, stem);
    if (has_skin) {
        const void *skin_file = NULL;
        size_t skin_size = 0;
        if (!eng_skin_encode(&t.out, skin, model.vertex_count, joint_count, &skin_file,
                             &skin_size) ||
            !write_output(skin_path, skin_file, skin_size)) {
            fprintf(stderr, "assetc : poids de skinning impossibles à cuire (voir le journal)\n");
            goto done;
        }
        printf("%s.skin : %u sommets, %u articulations, %zu octets\n", stem,
               (unsigned)model.vertex_count, (unsigned)joint_count, skin_size);
    } else if (eng_file_exists(skin_path)) {
        (void)SDL_RemovePath(skin_path);
    }
    if (!write_output(out_path, file, size)) {
        goto done;
    }
    remove_stale_textures(dir, stem, use_count);
    if (depfile != NULL && !mesh_depfile(&t, depfile, out_arg, in_path)) {
        goto done;
    }
    status = 0;
    if (bench_runs > 0) {
        eng_model_free(&model);
        model = (EngModel){0};
        if (!bench_loads(&t, in_path, out_path, bench_runs)) {
            status = 1;
        }
    }
done:
    eng_model_free(&model);
    tool_close(&t);
    return status;
}

/* ── tex ── */

static int cmd_tex(const char *in_arg, const char *out_arg, bool srgb, bool raw,
                   const char *depfile)
{
    char in_path[1024];
    char out_path[1024];
    if (!absolute_path(in_arg, in_path, sizeof in_path) ||
        !absolute_path(out_arg, out_path, sizeof out_path)) {
        return 1;
    }
    if (!has_suffix(out_path, ".tex")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .tex : %s\n", out_arg);
        return 2;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngImage image = {0};
    EngFileData file;
    if (!eng_file_read_all(&t.scratch, in_path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", in_arg);
    } else if (eng_image_decode(file.data, file.size, &image)) {
        const void *tex = NULL;
        size_t size = 0;
        const char *const in_deps[1] = {in_path};
        const bool encoded =
            raw ? eng_tex_encode(&t.out, &t.scratch, &image, srgb, &tex, &size)
                : eng_tex_encode_bc7(&t.out, &t.scratch, &image, srgb, &tex, &size);
        if (encoded && write_output(out_path, tex, size) &&
            (depfile == NULL || write_depfile(&t, depfile, out_arg, in_deps, 1))) {
            printf("%s : %u x %u, %u niveaux, %s, %zu octets\n", out_arg, (unsigned)image.width,
                   (unsigned)image.height, (unsigned)eng_image_mip_count(image.width, image.height),
                   srgb ? "sRGB" : "linéaire", size);
            status = 0;
        }
    }
    eng_image_free(&image);
    tool_close(&t);
    return status;
}

/* ── skel ── */

static int cmd_skel(const char *in_arg, const char *out_arg, uint32_t skin, const char *depfile)
{
    char in_path[1024];
    char out_path[1024];
    if (!absolute_path(in_arg, in_path, sizeof in_path) ||
        !absolute_path(out_arg, out_path, sizeof out_path)) {
        return 1;
    }
    if (!has_suffix(out_path, ".skel")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .skel : %s\n", out_arg);
        return 2;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngFileData file;
    EngSkeleton skeleton = {0};
    if (!eng_file_read_all(&t.scratch, in_path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", in_arg);
    } else {
        const uint32_t skins = eng_skeleton_gltf_skin_count(file.data, file.size);
        if (skins == 0) {
            fprintf(stderr, "assetc : %s ne contient aucun skin (squelette) glTF\n", in_arg);
        } else if (skin >= skins) {
            fprintf(stderr, "assetc : skin %u inexistant : %s en contient %u (0 à %u)\n",
                    (unsigned)skin, in_arg, (unsigned)skins, (unsigned)(skins - 1U));
        } else if (eng_skeleton_from_gltf(&t.out, &t.scratch, file.data, file.size, in_path, skin,
                                          &skeleton, NULL)) {
            const void *data = NULL;
            size_t size = 0;
            if (eng_skeleton_encode(&t.out, &skeleton, &data, &size) &&
                write_output(out_path, data, size) &&
                (depfile == NULL || mesh_depfile(&t, depfile, out_arg, in_path))) {
                uint32_t roots = 0;
                uint32_t depth_max = 0;
                uint32_t depth[ENG_SKELETON_MAX_JOINTS];
                for (uint32_t i = 0; i < skeleton.joint_count; i++) {
                    const int32_t parent = skeleton.joints[i].parent;
                    depth[i] = parent < 0 ? 0U : depth[parent] + 1U;
                    roots += parent < 0 ? 1U : 0U;
                    depth_max = depth[i] > depth_max ? depth[i] : depth_max;
                }
                const float mismatch = eng_skeleton_bind_mismatch(&skeleton);
                printf("%s : skin %u, %u articulations, %u racine(s), profondeur %u, %zu octets\n",
                       out_arg, (unsigned)skin, (unsigned)skeleton.joint_count, (unsigned)roots,
                       (unsigned)depth_max, size);
                if (!(mismatch <= 1e-3F)) {
                    fprintf(
                        stderr,
                        "assetc : ATTENTION : la pose de liaison (matrices inverses) diffère de "
                        "la pose repos des nœuds de %.4f m au pire : le squelette affiché "
                        "pourrait ne pas coller au maillage\n",
                        (double)mismatch);
                }
                status = 0;
            }
        }
    }
    tool_close(&t);
    return status;
}

/* ── anim ── */

static int cmd_anim(const char *in_arg, const char *out_arg, uint32_t anim, const char *anim_name,
                    uint32_t skin, const char *skeleton_path, const char *depfile)
{
    char in_path[1024];
    char out_path[1024];
    if (!absolute_path(in_arg, in_path, sizeof in_path) ||
        !absolute_path(out_arg, out_path, sizeof out_path)) {
        return 1;
    }
    if (!has_suffix(out_path, ".anim")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .anim : %s\n", out_arg);
        return 2;
    }
    /* chemin du squelette par défaut : <nom de l'entrée>.skel (celui de `assetc skel` appelé sur le
     * même fichier) */
    char default_skeleton[512];
    if (skeleton_path == NULL) {
        const char *base = in_arg;
        for (const char *p = in_arg; *p != '\0'; p++) {
            if (*p == '/' || *p == '\\') {
                base = p + 1;
            }
        }
        SDL_snprintf(default_skeleton, sizeof default_skeleton, "%s", base);
        char *dot = strrchr(default_skeleton, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        const size_t len = strlen(default_skeleton);
        SDL_snprintf(default_skeleton + len, sizeof default_skeleton - len, ".skel");
        skeleton_path = default_skeleton;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngFileData file;
    if (!eng_file_read_all(&t.scratch, in_path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", in_arg);
    } else {
        const uint32_t clips = eng_anim_gltf_clip_count(file.data, file.size);
        const uint32_t skins = eng_skeleton_gltf_skin_count(file.data, file.size);
        if (anim_name != NULL) {
            anim = eng_anim_gltf_find(file.data, file.size, anim_name);
            if (anim == UINT32_MAX) {
                fprintf(stderr, "assetc : aucune animation nommée « %s » dans %s\n", anim_name,
                        in_arg);
                anim = clips; /* refusé plus bas */
            }
        }
        EngAnimClip clip = {0};
        EngAnimImportStats stats;
        if (clips == 0) {
            fprintf(stderr, "assetc : %s ne contient aucune animation glTF\n", in_arg);
        } else if (anim >= clips) {
            fprintf(stderr, "assetc : animation %u inexistante : %s en contient %u (0 à %u)\n",
                    (unsigned)anim, in_arg, (unsigned)clips, (unsigned)(clips - 1U));
        } else if (skin >= skins) {
            fprintf(stderr, "assetc : skin %u inexistant : %s en contient %u\n", (unsigned)skin,
                    in_arg, (unsigned)skins);
        } else if (eng_anim_from_gltf(&t.out, &t.scratch, file.data, file.size, in_path, anim, skin,
                                      skeleton_path, &clip, &stats)) {
            const void *data = NULL;
            size_t size = 0;
            if (eng_anim_encode(&t.out, &clip, &data, &size) &&
                write_output(out_path, data, size) &&
                (depfile == NULL || mesh_depfile(&t, depfile, out_arg, in_path))) {
                printf("%s : animation %u, durée %.3f s, %u canaux, %u clés, squelette %s, %zu "
                       "octets\n",
                       out_arg, (unsigned)anim, (double)clip.duration, (unsigned)clip.channel_count,
                       (unsigned)clip.key_total, skeleton_path, size);
                if (stats.rebaked_joints > 0 || stats.ignored_channels > 0) {
                    printf("  %u articulation(s) recalculée(s) (ancêtres repliés, ex. Armature), "
                           "%u canal(aux) ignoré(s) sur %u\n",
                           (unsigned)stats.rebaked_joints, (unsigned)stats.ignored_channels,
                           (unsigned)stats.gltf_channels);
                }
                status = 0;
            }
        }
    }
    tool_close(&t);
    return status;
}

/* ── info ── */

static void print_tag(uint32_t tag)
{
    printf("%c%c%c%c", (int)(tag & 0xFFU), (int)((tag >> 8) & 0xFFU), (int)((tag >> 16) & 0xFFU),
           (int)((tag >> 24) & 0xFFU));
}

static int cmd_info(const char *arg)
{
    char path[1024];
    if (!absolute_path(arg, path, sizeof path)) {
        return 1;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngFileData file;
    if (!eng_file_read_all(&t.scratch, path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", arg);
        goto done;
    }
    uint32_t magic = 0;
    uint16_t version = 0;
    if (file.size >= 4U) {
        memcpy(&magic, file.data, sizeof magic);
    }
    static const struct {
        uint32_t magic;
        uint16_t version;
    } kinds[] = {{ENG_ASSET_MAGIC_MESH, ENG_ASSET_VERSION_MESH},
                 {ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX},
                 {ENG_ASSET_MAGIC_SKEL, ENG_ASSET_VERSION_SKEL},
                 {ENG_ASSET_MAGIC_ANIM, ENG_ASSET_VERSION_ANIM},
                 {ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP},
                 {ENG_ASSET_MAGIC_SKIN, ENG_ASSET_VERSION_SKIN}};
    for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        if (magic == kinds[k].magic) {
            version = kinds[k].version;
        }
    }
    if (version == 0) {
        fprintf(stderr, "assetc : %s n'est pas un fichier d'asset (magic inconnu)\n", arg);
        goto done;
    }
    EngAssetView view;
    const EngAssetStatus st = eng_asset_open(file.data, file.size, magic, version, arg, &view);
    if (st != ENG_ASSET_OK) {
        fprintf(stderr, "assetc : %s refusé : %s\n", arg, eng_asset_status_name(st));
        goto done;
    }
    printf("%s : ", arg);
    print_tag(magic);
    printf(" version %u, %zu octets, %u sections\n", (unsigned)view.version, view.size,
           (unsigned)view.section_count);
    for (uint32_t i = 0; i < view.section_count; i++) {
        EngAssetSection s;
        if (eng_asset_section_at(&view, i, &s)) {
            printf("  ");
            print_tag(s.tag);
            printf("  offset %8u  taille %9u  éléments %9u\n", (unsigned)(s.data - file.data),
                   (unsigned)s.size, (unsigned)s.count);
        }
    }
    if (magic == ENG_ASSET_MAGIC_MESH) {
        EngMeshFile mesh;
        if (!eng_mesh_decode(&t.model, file.data, file.size, arg, &mesh)) {
            fprintf(stderr, "assetc : contenu du .mesh refusé\n");
            goto done;
        }
        printf("  contenu valide : %u sommets, %u index, %u parties, %u matériaux\n",
               (unsigned)mesh.model.vertex_count, (unsigned)mesh.model.index_count,
               (unsigned)mesh.model.part_count, (unsigned)mesh.model.material_count);
        for (uint32_t i = 0; i < mesh.texture_count; i++) {
            printf("  texture %u : %s (%s)\n", (unsigned)i, mesh.textures[i].path,
                   mesh.textures[i].srgb ? "sRGB" : "linéaire");
        }
    } else if (magic == ENG_ASSET_MAGIC_TEX) {
        EngTexView tex;
        if (!eng_tex_open(file.data, file.size, arg, &tex)) {
            fprintf(stderr, "assetc : contenu du .tex refusé\n");
            goto done;
        }
        printf("  contenu valide : format %u, %u x %u, %u niveaux, %u couches\n",
               (unsigned)tex.info.format, (unsigned)tex.info.width, (unsigned)tex.info.height,
               (unsigned)tex.info.mip_count, (unsigned)tex.info.layer_count);
    } else if (magic == ENG_ASSET_MAGIC_SKIN) {
        EngSkinVertex *skin = NULL;
        uint32_t joints = 0;
        uint32_t vertices = 0;
        if (view.section_count > 0) {
            EngAssetSection w;
            if (eng_asset_find_section(&view, ENG_ASSET_TAG_WGHT, &w)) {
                vertices = w.count;
            }
        }
        if (!eng_skin_decode(&t.model, file.data, file.size, arg, vertices, &skin, &joints)) {
            fprintf(stderr, "assetc : contenu du .skin refusé\n");
            goto done;
        }
        printf("  contenu valide : %u sommets, %u articulations\n", (unsigned)vertices,
               (unsigned)joints);
    } else if (magic == ENG_ASSET_MAGIC_HMAP) {
        EngHeightmap map;
        if (!eng_heightmap_open(file.data, file.size, arg, &map)) {
            fprintf(stderr, "assetc : contenu du .hmap refusé\n");
            goto done;
        }
        printf("  contenu valide : %u x %u échantillons, espacement %g m, étendue %g m, "
               "altitudes %g à %g m\n",
               (unsigned)map.size, (unsigned)map.size, (double)map.spacing,
               (double)eng_heightmap_extent(&map), (double)map.altitude_min,
               (double)map.altitude_max);
    }
    status = 0;
done:
    tool_close(&t);
    return status;
}

/* ── heightmap ── */

static bool parse_float(const char *text, float *out)
{
    char *end = NULL;
    const float v = strtof(text, &end);
    if (end == text || *end != '\0') {
        fprintf(stderr, "assetc : nombre attendu : %s\n", text);
        return false;
    }
    *out = v;
    return true;
}

static bool parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    const unsigned long v = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || text[0] == '-' || v > UINT32_MAX) {
        fprintf(stderr, "assetc : entier positif attendu : %s\n", text);
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static bool parse_i32(const char *text, int32_t *out)
{
    char *end = NULL;
    const long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < INT32_MIN || v > INT32_MAX) {
        fprintf(stderr, "assetc : entier attendu : %s\n", text);
        return false;
    }
    *out = (int32_t)v;
    return true;
}

typedef enum OptionKind { OPTION_U32, OPTION_I32, OPTION_FLOAT } OptionKind;

typedef struct HeightmapOption {
    const char *name;
    OptionKind kind;
    void *target;
} HeightmapOption;

/* Option « --nom valeur » en argv[i] : écrit la valeur et rend 2 ; -1 (message sur stderr) si
 * l'option est inconnue, sans valeur ou mal écrite. */
static int parse_option(const HeightmapOption *options, size_t count, int argc, char **argv, int i)
{
    for (size_t k = 0; k < count; k++) {
        if (strcmp(argv[i], options[k].name) != 0) {
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "assetc : %s demande une valeur\n", options[k].name);
            return -1;
        }
        const char *text = argv[i + 1];
        bool ok = false;
        if (options[k].kind == OPTION_U32) {
            ok = parse_u32(text, options[k].target);
        } else if (options[k].kind == OPTION_I32) {
            ok = parse_i32(text, options[k].target);
        } else {
            ok = parse_float(text, options[k].target);
        }
        return ok ? 2 : -1;
    }
    fprintf(stderr, "assetc : option inconnue : %s\n", argv[i]);
    return -1;
}

static int cmd_heightmap(const char *out_arg, const EngHeightmapParams *params, const char *png_arg)
{
    char out_path[1024];
    char png_path[1024];
    if (!absolute_path(out_arg, out_path, sizeof out_path) ||
        (png_arg != NULL && !absolute_path(png_arg, png_path, sizeof png_path))) {
        return 1;
    }
    if (!has_suffix(out_path, ".hmap")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .hmap : %s\n", out_arg);
        return 2;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngHeightmap map;
    if (eng_heightmap_generate(&t.scratch, params, &map)) {
        const size_t bytes = eng_heightmap_file_size(&map);
        void *file = eng_arena_alloc(&t.out, bytes, ENG_ASSET_ALIGN);
        if (file != NULL && eng_heightmap_encode(&map, file, bytes, NULL) &&
            write_output(out_path, file, bytes)) {
            status = 0;
            printf(
                "%s : %u x %u échantillons, %g m (étendue %g m), altitudes %g à %g m, graine %d, "
                "%zu octets\n",
                out_arg, (unsigned)map.size, (unsigned)map.size, (double)map.spacing,
                (double)eng_heightmap_extent(&map), (double)map.altitude_min,
                (double)map.altitude_max, (int)params->seed, bytes);
            if (png_arg != NULL) {
                const void *png = NULL;
                size_t png_size = 0;
                if (eng_heightmap_encode_png(&t.out, &map, &png, &png_size) &&
                    write_output(png_path, png, png_size)) {
                    printf("%s : PNG 16 bits, %zu octets\n", png_arg, png_size);
                } else {
                    status = 1;
                }
            }
        }
    }
    tool_close(&t);
    return status;
}

static int cmd_heightmap_import(const char *in_arg, const char *out_arg, float spacing, float low,
                                float high)
{
    char in_path[1024];
    char out_path[1024];
    if (!absolute_path(in_arg, in_path, sizeof in_path) ||
        !absolute_path(out_arg, out_path, sizeof out_path)) {
        return 1;
    }
    if (!has_suffix(out_path, ".hmap")) {
        fprintf(stderr, "assetc : la sortie doit se terminer par .hmap : %s\n", out_arg);
        return 2;
    }
    Tool t;
    if (!tool_open(&t)) {
        fprintf(stderr, "assetc : mémoire insuffisante\n");
        return 1;
    }
    int status = 1;
    EngFileData file;
    EngHeightmap map;
    if (!eng_file_read_all(&t.scratch, in_path, &file)) {
        fprintf(stderr, "assetc : lecture de %s impossible\n", in_arg);
    } else if (eng_heightmap_import_image(&t.model, file.data, file.size, spacing, low, high,
                                          &map)) {
        const size_t bytes = eng_heightmap_file_size(&map);
        void *out = eng_arena_alloc(&t.out, bytes, ENG_ASSET_ALIGN);
        if (out != NULL && eng_heightmap_encode(&map, out, bytes, NULL) &&
            write_output(out_path, out, bytes)) {
            printf("%s : %u x %u échantillons importés de %s, %zu octets\n", out_arg,
                   (unsigned)map.size, (unsigned)map.size, in_arg, bytes);
            status = 0;
        }
    }
    tool_close(&t);
    return status;
}

static int usage(void)
{
    fprintf(
        stderr,
        "assetc (moteur %s)\n"
        "usage : assetc mesh <entrée.gltf|.glb> <sortie.mesh> [--bench N] [--raw] [--skin N] "
        "[--depfile f.d]\n"
        "        assetc tex <entrée.png|.jpg|...> <sortie.tex> [--linear] [--raw] [--depfile f.d]\n"
        "        assetc skel <entrée.gltf|.glb> <sortie.skel> [--skin N] [--depfile f.d]\n"
        "        assetc anim <entrée.gltf|.glb> <sortie.anim> [--anim N | --anim-name NOM] [--skin "
        "N]\n"
        "                    [--skeleton chemin.skel]\n"
        "                    [--depfile f.d]\n"
        "        assetc heightmap <sortie.hmap> [--size N] [--spacing M] [--alt-min A] [--alt-max "
        "B]\n"
        "                         [--seed S] [--wavelength W] [--octaves K] [--lacunarity L]\n"
        "                         [--gain G] [--ridged] [--png <sortie.png>]\n"
        "        assetc heightmap-import <entrée.png> <sortie.hmap> [--spacing M] [--alt-min A]\n"
        "                         [--alt-max B]\n"
        "        assetc info <fichier.mesh|.tex|.skel|.anim|.skin|.hmap>\n",
        eng_version());
    return 2;
}

int main(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "mesh") == 0) {
        int runs = 0;
        bool raw = false;
        bool has_skin = false;
        uint32_t skin_index = 0;
        const char *depfile = NULL;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--raw") == 0) {
                raw = true;
            } else if (strcmp(argv[i], "--skin") == 0 && i + 1 < argc) {
                char *end = NULL;
                const long v = strtol(argv[++i], &end, 10);
                if (end == argv[i] || *end != '\0' || v < 0 || v > 1000) {
                    return usage();
                }
                skin_index = (uint32_t)v;
                has_skin = true;
            } else if (strcmp(argv[i], "--depfile") == 0 && i + 1 < argc) {
                depfile = argv[++i];
            } else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
                runs = atoi(argv[++i]);
                if (runs < 1 || runs > 1000) {
                    return usage();
                }
            } else {
                return usage();
            }
        }
        if (has_skin && runs > 0) {
            fprintf(stderr, "assetc : --skin et --bench sont incompatibles\n");
            return 2;
        }
        return cmd_mesh(argv[2], argv[3], runs, raw, has_skin, skin_index, depfile);
    }
    if (argc >= 4 && strcmp(argv[1], "tex") == 0) {
        bool srgb = true;
        bool raw = false;
        const char *depfile = NULL;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--linear") == 0) {
                srgb = false;
            } else if (strcmp(argv[i], "--raw") == 0) {
                raw = true;
            } else if (strcmp(argv[i], "--depfile") == 0 && i + 1 < argc) {
                depfile = argv[++i];
            } else {
                return usage();
            }
        }
        return cmd_tex(argv[2], argv[3], srgb, raw, depfile);
    }
    if (argc >= 4 && strcmp(argv[1], "skel") == 0) {
        uint32_t skin = 0;
        const char *depfile = NULL;
        for (int i = 4; i < argc; i++) {
            char *end = NULL;
            if (strcmp(argv[i], "--depfile") == 0 && i + 1 < argc) {
                depfile = argv[++i];
            } else if (strcmp(argv[i], "--skin") == 0 && i + 1 < argc) {
                const long v = strtol(argv[++i], &end, 10);
                if (end == argv[i] || *end != '\0' || v < 0 || v > 1000000) {
                    return usage();
                }
                skin = (uint32_t)v;
            } else {
                return usage();
            }
        }
        return cmd_skel(argv[2], argv[3], skin, depfile);
    }
    if (argc >= 4 && strcmp(argv[1], "anim") == 0) {
        uint32_t anim = 0;
        uint32_t skin = 0;
        const char *anim_name = NULL;
        const char *skeleton_path = NULL;
        const char *depfile = NULL;
        for (int i = 4; i < argc; i++) {
            char *end = NULL;
            const bool has_value = i + 1 < argc;
            if (strcmp(argv[i], "--depfile") == 0 && has_value) {
                depfile = argv[++i];
            } else if (strcmp(argv[i], "--anim-name") == 0 && has_value) {
                anim_name = argv[++i];
            } else if (strcmp(argv[i], "--skeleton") == 0 && has_value) {
                skeleton_path = argv[++i];
            } else if ((strcmp(argv[i], "--anim") == 0 || strcmp(argv[i], "--skin") == 0) &&
                       has_value) {
                const bool is_anim = strcmp(argv[i], "--anim") == 0;
                const long v = strtol(argv[++i], &end, 10);
                if (end == argv[i] || *end != '\0' || v < 0 || v > 1000000) {
                    return usage();
                }
                if (is_anim) {
                    anim = (uint32_t)v;
                } else {
                    skin = (uint32_t)v;
                }
            } else {
                return usage();
            }
        }
        return cmd_anim(argv[2], argv[3], anim, anim_name, skin, skeleton_path, depfile);
    }
    if (argc >= 3 && strcmp(argv[1], "heightmap") == 0) {
        EngHeightmapParams params = eng_heightmap_default_params();
        const char *png = NULL;
        const HeightmapOption options[] = {{"--size", OPTION_U32, &params.size},
                                           {"--octaves", OPTION_U32, &params.octaves},
                                           {"--seed", OPTION_I32, &params.seed},
                                           {"--spacing", OPTION_FLOAT, &params.spacing},
                                           {"--alt-min", OPTION_FLOAT, &params.altitude_min},
                                           {"--alt-max", OPTION_FLOAT, &params.altitude_max},
                                           {"--wavelength", OPTION_FLOAT, &params.wavelength},
                                           {"--lacunarity", OPTION_FLOAT, &params.lacunarity},
                                           {"--gain", OPTION_FLOAT, &params.gain}};
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--ridged") == 0) {
                params.fractal = ENG_NOISE_RIDGED;
            } else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc) {
                png = argv[++i];
            } else {
                const int used =
                    parse_option(options, sizeof options / sizeof options[0], argc, argv, i);
                if (used < 0) {
                    return 2;
                }
                i += used - 1;
            }
        }
        return cmd_heightmap(argv[2], &params, png);
    }
    if (argc >= 4 && strcmp(argv[1], "heightmap-import") == 0) {
        float spacing = 1.0F;
        float low = 0.0F;
        float high = 120.0F;
        const HeightmapOption options[] = {{"--spacing", OPTION_FLOAT, &spacing},
                                           {"--alt-min", OPTION_FLOAT, &low},
                                           {"--alt-max", OPTION_FLOAT, &high}};
        for (int i = 4; i < argc; i++) {
            const int used =
                parse_option(options, sizeof options / sizeof options[0], argc, argv, i);
            if (used < 0) {
                return 2;
            }
            i += used - 1;
        }
        return cmd_heightmap_import(argv[2], argv[3], spacing, low, high);
    }
    if (argc == 3 && strcmp(argv[1], "info") == 0) {
        return cmd_info(argv[2]);
    }
    return usage();
}
