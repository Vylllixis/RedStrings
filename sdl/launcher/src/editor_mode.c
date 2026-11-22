#include "editor_mode.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"
#include "engine/scene.h"

static bool file_exists(const char *path)
{
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        return false;
    }
    FILE *file = fopen(resolved, "rb");
    if (file == NULL) {
        return false;
    }
    (void)fclose(file);
    return true;
}

static void build_default_world(EngWorld *world)
{
    const EngEntity entity = eng_world_create(world, "modèle central");
    (void)eng_world_add(world, entity, ENG_COMPONENT_TRANSFORM);
    EngMeshRenderer *renderer = eng_world_add(world, entity, ENG_COMPONENT_MESH_RENDERER);
    if (renderer != NULL) {
        (void)snprintf(renderer->model, sizeof renderer->model, "assets/cooked/models/demo.mesh");
    }
}

bool editor_mode_init(EditorMode *mode, const char *scene_path)
{
    *mode = (EditorMode){0};
    (void)snprintf(mode->scene_path, sizeof mode->scene_path, "%s",
                   scene_path != NULL ? scene_path : EDITOR_DEFAULT_SCENE);
    if (!eng_world_init(&mode->world, EDITOR_WORLD_CAPACITY)) {
        return false;
    }
    eng_editor_init(&mode->editor);
    mode->ready = true;
    mode->save_allowed = true;
    if (!file_exists(mode->scene_path)) {
        ENG_LOG_INFO("éditeur : %s n'existe pas, monde par défaut (créé à l'enregistrement)",
                     mode->scene_path);
        build_default_world(&mode->world);
        return true;
    }
    EngSceneError error;
    if (eng_scene_load(&mode->world, mode->scene_path, &error)) {
        ENG_LOG_INFO("éditeur : %s chargée (%u entité(s))", mode->scene_path,
                     eng_world_entity_count(&mode->world));
        return true;
    }
    ENG_LOG_ERROR("éditeur : %s illisible (ligne %u : %s) : enregistrement désactivé pour ne pas "
                  "l'écraser",
                  mode->scene_path, (unsigned)error.line, error.message);
    mode->save_allowed = false;
    build_default_world(&mode->world);
    return true;
}

void editor_mode_shutdown(EditorMode *mode)
{
    if (mode->ready) {
        eng_world_shutdown(&mode->world);
    }
    *mode = (EditorMode){0};
}

bool editor_mode_save(const EditorMode *mode)
{
    if (!mode->ready || !mode->save_allowed) {
        ENG_LOG_ERROR("éditeur : enregistrement refusé (%s n'a pas pu être relue au démarrage)",
                      mode->scene_path);
        return false;
    }
    if (!eng_scene_save(&mode->world, mode->scene_path)) {
        ENG_LOG_ERROR("éditeur : échec de l'enregistrement dans %s", mode->scene_path);
        return false;
    }
    ENG_LOG_INFO("éditeur : scène enregistrée dans %s (%u entité(s))", mode->scene_path,
                 eng_world_entity_count(&mode->world));
    return true;
}

/* ── Autotest ── */

typedef struct Projector {
    mat4 view_proj;
    float width;
    float height;
} Projector;

/* Pixel (x, y) où le point monde `p` apparaît ; false s'il est derrière la caméra. */
static bool project(Projector *projector, const float p[3], float *x, float *y)
{
    vec4 clip;
    glm_mat4_mulv(projector->view_proj, (vec4){p[0], p[1], p[2], 1.0F}, clip);
    if (!(clip[3] > 0.0F)) {
        return false;
    }
    *x = ((clip[0] / clip[3]) * 0.5F + 0.5F) * projector->width;
    *y = (1.0F - (((clip[1] / clip[3]) * 0.5F) + 0.5F)) * projector->height;
    return true;
}

/* Égalité bit à bit de deux floats (l'écriture « %.9g » doit les restituer exactement). */
static bool same_bits(float a, float b)
{
    uint32_t ia;
    uint32_t ib;
    memcpy(&ia, &a, sizeof ia);
    memcpy(&ib, &b, sizeof ib);
    return ia == ib;
}

#define FAIL(...)                                                                                  \
    do {                                                                                           \
        ENG_LOG_ERROR("AUTOTEST ÉDITEUR : ÉCHEC : " __VA_ARGS__);                                  \
        return false;                                                                              \
    } while (0)

/* Une image de souris : pixel (x, y), bouton. */
static bool mouse(EditorMode *mode, DemoScene *scene, const EngCamera *camera, float aspect,
                  Projector *projector, const float world_point[3], bool down, bool *modified)
{
    float x;
    float y;
    EngRay ray;
    if (!project(projector, world_point, &x, &y) ||
        !eng_ray_from_screen(camera, aspect, x, y, projector->width, projector->height, &ray)) {
        return false;
    }
    const bool changed = eng_editor_pointer(&mode->editor, &mode->world, demo_scene_entity_bounds,
                                            scene, &ray, camera->position, down, false);
    if (modified != NULL) {
        *modified = changed;
    }
    return true;
}

bool editor_mode_selftest(EditorMode *mode, DemoScene *scene, const EngCamera *camera, float aspect,
                          float width, float height, const char *output_path)
{
    const EngEntity target = eng_world_find_by_name(&mode->world, "modèle central");
    float start[3];
    float min[3];
    float max[3];
    if (target.generation == 0 || !eng_editor_world_position(&mode->world, target, start)) {
        FAIL("entité « modèle central » absente");
    }
    if (!demo_scene_entity_bounds(scene, target, min, max)) {
        FAIL("le modèle central n'est pas chargé (bornes indisponibles)");
    }
    Projector projector = {.width = width, .height = height};
    mat4 view;
    mat4 proj;
    eng_camera_view(camera, view);
    eng_camera_projection(camera, aspect, proj);
    glm_mat4_mul(proj, view, projector.view_proj);

    /* 1. sélection : le centre du modèle (en repère monde : l'entité est à l'identité) */
    const float center[3] = {start[0] + (0.5F * (min[0] + max[0])),
                             start[1] + (0.5F * (min[1] + max[1])),
                             start[2] + (0.5F * (min[2] + max[2]))};
    bool modified = false;
    (void)eng_editor_select(&mode->editor, &mode->world, ENG_ENTITY_NULL);
    if (!mouse(mode, scene, camera, aspect, &projector, center, false, NULL) ||
        !mouse(mode, scene, camera, aspect, &projector, center, true, &modified)) {
        FAIL("le centre du modèle n'est pas visible à l'écran");
    }
    const EngEntity picked = eng_editor_selected(&mode->editor, &mode->world);
    if (picked.index != target.index || picked.generation != target.generation) {
        FAIL("le clic au centre du modèle n'a pas sélectionné « modèle central »");
    }
    (void)mouse(mode, scene, camera, aspect, &projector, center, false, NULL);

    /* 2. poignée X : à mi-chemin de la flèche ; puis tirée de 3 m le long de X */
    const float length = eng_editor_gizmo_length(camera->position, start);
    const float on_handle[3] = {start[0] + (0.5F * length), start[1], start[2]};
    if (!mouse(mode, scene, camera, aspect, &projector, on_handle, false, NULL) ||
        mode->editor.hover_axis != ENG_GIZMO_X) {
        FAIL("la poignée X n'est pas survolée (axe %u)", (unsigned)mode->editor.hover_axis);
    }
    (void)mouse(mode, scene, camera, aspect, &projector, on_handle, true, &modified);
    if (mode->editor.drag_axis != ENG_GIZMO_X) {
        FAIL("l'appui sur la poignée X n'a pas démarré de glissement");
    }
    const float dragged[3] = {on_handle[0] + 3.0F, on_handle[1], on_handle[2]};
    if (!mouse(mode, scene, camera, aspect, &projector, dragged, true, &modified) || !modified) {
        FAIL("le glissement n'a rien déplacé");
    }
    (void)mouse(mode, scene, camera, aspect, &projector, dragged, false, NULL);
    float moved[3];
    (void)eng_editor_world_position(&mode->world, target, moved);
    if (fabsf(moved[0] - (start[0] + 3.0F)) > 0.02F || fabsf(moved[1] - start[1]) > 1e-4F ||
        fabsf(moved[2] - start[2]) > 1e-4F) {
        FAIL("position après glissement (%.4f, %.4f, %.4f) au lieu de (%.4f, %.4f, %.4f)",
             (double)moved[0], (double)moved[1], (double)moved[2], (double)(start[0] + 3.0F),
             (double)start[1], (double)start[2]);
    }

    /* 3. enregistrement puis relecture dans un monde neuf */
    if (!eng_scene_save(&mode->world, output_path)) {
        FAIL("enregistrement dans %s impossible", output_path);
    }
    EngWorld reread;
    if (!eng_world_init(&reread, EDITOR_WORLD_CAPACITY)) {
        FAIL("monde de relecture impossible");
    }
    EngSceneError error;
    bool ok = eng_scene_load(&reread, output_path, &error);
    if (!ok) {
        ENG_LOG_ERROR("AUTOTEST ÉDITEUR : ÉCHEC : relecture (ligne %u : %s)", (unsigned)error.line,
                      error.message);
    } else {
        const EngEntity again = eng_world_find_by_name(&reread, "modèle central");
        float read_back[3] = {0};
        ok = again.generation != 0 && eng_editor_world_position(&reread, again, read_back) &&
             same_bits(read_back[0], moved[0]) && same_bits(read_back[1], moved[1]) &&
             same_bits(read_back[2], moved[2]) &&
             eng_world_entity_count(&reread) == eng_world_entity_count(&mode->world);
        if (!ok) {
            ENG_LOG_ERROR("AUTOTEST ÉDITEUR : ÉCHEC : la scène relue diffère (%.6f, %.6f, %.6f)",
                          (double)read_back[0], (double)read_back[1], (double)read_back[2]);
        }
    }
    eng_world_shutdown(&reread);
    if (ok) {
        ENG_LOG_INFO("AUTOTEST ÉDITEUR : OK (sélection, glissement de %.3f m, enregistrement, "
                     "relecture)",
                     (double)(moved[0] - start[0]));
    }
    return ok;
}
