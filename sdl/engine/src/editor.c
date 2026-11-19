#include "engine/editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"

/* ── Rayons ── */

static bool all_finite(const float *values, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (!isfinite(values[i])) {
            return false;
        }
    }
    return true;
}

bool eng_ray_from_screen(const EngCamera *camera, float aspect, float x, float y, float width,
                         float height, EngRay *out)
{
    const float inputs[] = {aspect, x, y, width, height};
    if (!all_finite(inputs, 5) || !all_finite(camera->position, 3) || !(aspect > 0.0F) ||
        !(width > 0.0F) || !(height > 0.0F) || x < 0.0F || y < 0.0F || x > width || y > height ||
        !(camera->fov_y > 0.0F) || !isfinite(camera->fov_y)) {
        return false;
    }
    vec3 forward;
    vec3 right;
    vec3 up;
    eng_camera_forward(camera, forward);
    eng_camera_right(camera, right);
    glm_vec3_cross(right, forward, up);
    glm_vec3_normalize(up);
    const float ndc_x = ((2.0F * x) / width) - 1.0F;
    const float ndc_y = 1.0F - ((2.0F * y) / height);
    const float half_height = tanf(0.5F * camera->fov_y);
    vec3 dir;
    for (int i = 0; i < 3; i++) {
        dir[i] =
            forward[i] + (right[i] * ndc_x * half_height * aspect) + (up[i] * ndc_y * half_height);
    }
    glm_vec3_normalize(dir);
    for (int i = 0; i < 3; i++) {
        out->origin[i] = camera->position[i];
        out->dir[i] = dir[i];
    }
    return true;
}

bool eng_ray_vs_box(const EngRay *ray, const float box_min[3], const float box_max[3], mat4 model,
                    float *t)
{
    if (fabsf(glm_mat4_det(model)) < 1.0e-12F) {
        return false;
    }
    mat4 inverse;
    glm_mat4_inv(model, inverse);
    vec3 origin;
    vec3 dir;
    glm_mat4_mulv3(inverse, (float *)ray->origin, 1.0F, origin);
    glm_mat4_mulv3(inverse, (float *)ray->dir, 0.0F, dir);
    /* La transformation est affine : un point origin + s·dir du repère local est le point
     * origin_monde + s·dir_monde du monde, `s` est donc aussi la distance monde. */
    float enter = 0.0F;
    float leave = INFINITY;
    for (int axis = 0; axis < 3; axis++) {
        if (box_min[axis] > box_max[axis]) {
            return false;
        }
        if (fabsf(dir[axis]) < 1.0e-12F) {
            if (origin[axis] < box_min[axis] || origin[axis] > box_max[axis]) {
                return false;
            }
            continue;
        }
        float near_t = (box_min[axis] - origin[axis]) / dir[axis];
        float far_t = (box_max[axis] - origin[axis]) / dir[axis];
        if (near_t > far_t) {
            const float swap = near_t;
            near_t = far_t;
            far_t = swap;
        }
        enter = near_t > enter ? near_t : enter;
        leave = far_t < leave ? far_t : leave;
        if (enter > leave) {
            return false;
        }
    }
    if (t != NULL) {
        *t = enter;
    }
    return true;
}

bool eng_ray_closest_to_line(const EngRay *ray, const float point[3], const float dir[3],
                             float *line_s, float *ray_t, float *distance)
{
    vec3 w;
    for (int i = 0; i < 3; i++) {
        w[i] = ray->origin[i] - point[i];
    }
    const float b = glm_vec3_dot((float *)ray->dir, (float *)dir);
    const float d = glm_vec3_dot((float *)ray->dir, w);
    const float e = glm_vec3_dot((float *)dir, w);
    const float denominator =
        1.0F - (b * b); /* = sin² de l'angle (les deux directions sont unitaires) */
    if (!(denominator > ENG_RAY_PARALLEL_SIN * ENG_RAY_PARALLEL_SIN)) {
        return false;
    }
    const float t = ((b * e) - d) / denominator;
    const float s = (e - (b * d)) / denominator;
    vec3 on_ray;
    vec3 on_line;
    for (int i = 0; i < 3; i++) {
        on_ray[i] = ray->origin[i] + (t * ray->dir[i]);
        on_line[i] = point[i] + (s * dir[i]);
    }
    *line_s = s;
    *ray_t = t;
    *distance = glm_vec3_distance(on_ray, on_line);
    return true;
}

/* ── Sélection ── */

void eng_editor_init(EngEditor *editor)
{
    *editor = (EngEditor){0};
}

bool eng_editor_select(EngEditor *editor, const EngWorld *world, EngEntity entity)
{
    if (entity.generation != 0 && !eng_world_alive(world, entity)) {
        return false;
    }
    editor->selected = entity;
    editor->drag_axis = ENG_GIZMO_NONE;
    editor->hover_axis = ENG_GIZMO_NONE;
    return true;
}

EngEntity eng_editor_selected(EngEditor *editor, const EngWorld *world)
{
    if (editor->selected.generation != 0 && !eng_world_alive(world, editor->selected)) {
        editor->selected = ENG_ENTITY_NULL;
        editor->drag_axis = ENG_GIZMO_NONE;
        editor->hover_axis = ENG_GIZMO_NONE;
    }
    return editor->selected;
}

EngEntity eng_editor_pick(const EngWorld *world, EngEditorBoundsFn bounds, void *user,
                          const EngRay *ray, float *t)
{
    EngEntity best = ENG_ENTITY_NULL;
    float best_t = INFINITY;
    const uint32_t count = eng_world_table_count(world, ENG_COMPONENT_MESH_RENDERER);
    for (uint32_t i = 0; i < count; i++) {
        const EngEntity entity = eng_world_table_entity(world, ENG_COMPONENT_MESH_RENDERER, i);
        const EngMeshRenderer *renderer =
            eng_world_table_data(world, ENG_COMPONENT_MESH_RENDERER, i);
        float box_min[3];
        float box_max[3];
        mat4 model;
        float hit;
        if (!renderer->visible || !bounds(user, entity, box_min, box_max) ||
            !eng_world_world_matrix(world, entity, model) ||
            !eng_ray_vs_box(ray, box_min, box_max, model, &hit)) {
            continue;
        }
        if (hit < best_t) {
            best_t = hit;
            best = entity;
        }
    }
    if (best.generation != 0 && t != NULL) {
        *t = best_t;
    }
    return best;
}

/* ── Position monde ── */

bool eng_editor_world_position(const EngWorld *world, EngEntity entity, float out[3])
{
    mat4 model;
    if (!eng_world_world_matrix(world, entity, model)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = model[3][i];
    }
    return true;
}

bool eng_editor_set_world_position(EngWorld *world, EngEntity entity, const float position[3])
{
    if (!eng_world_alive(world, entity) || !all_finite(position, 3)) {
        return false;
    }
    mat4 parent_world;
    const EngEntity parent = eng_world_parent(world, entity);
    if (parent.generation != 0) {
        (void)eng_world_world_matrix(world, parent, parent_world);
    } else {
        glm_mat4_identity(parent_world);
    }
    if (fabsf(glm_mat4_det(parent_world)) < 1.0e-12F) {
        return false;
    }
    mat4 inverse;
    glm_mat4_inv(parent_world, inverse);
    vec3 local;
    glm_mat4_mulv3(inverse, (float *)position, 1.0F, local);
    if (!all_finite(local, 3)) {
        return false;
    }
    EngTransform *transform = eng_world_add(world, entity, ENG_COMPONENT_TRANSFORM);
    if (transform == NULL) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        transform->position[i] = local[i];
    }
    return true;
}

/* ── Poignées ── */

static const float AXIS_DIRECTIONS[4][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

float eng_editor_gizmo_length(const float camera_position[3], const float origin[3])
{
    const float length =
        ENG_GIZMO_SCREEN_FRACTION * glm_vec3_distance((float *)camera_position, (float *)origin);
    return isfinite(length) && length > ENG_GIZMO_MIN_LENGTH ? length : ENG_GIZMO_MIN_LENGTH;
}

uint32_t eng_editor_gizmo_hit(const EngEditor *editor, const EngWorld *world, const EngRay *ray,
                              const float camera_position[3])
{
    float origin[3];
    if (editor->selected.generation == 0 ||
        !eng_editor_world_position(world, editor->selected, origin)) {
        return ENG_GIZMO_NONE;
    }
    const float length = eng_editor_gizmo_length(camera_position, origin);
    const float radius = ENG_GIZMO_PICK_RADIUS * length;
    uint32_t best = ENG_GIZMO_NONE;
    float best_distance = INFINITY;
    for (uint32_t axis = ENG_GIZMO_X; axis <= ENG_GIZMO_Z; axis++) {
        float s;
        float t;
        float distance;
        if (eng_ray_closest_to_line(ray, origin, AXIS_DIRECTIONS[axis], &s, &t, &distance) &&
            t > 0.0F && s >= 0.0F && s <= length && distance <= radius &&
            distance < best_distance) {
            best = axis;
            best_distance = distance;
        }
    }
    return best;
}

bool eng_editor_drag_begin(EngEditor *editor, const EngWorld *world, uint32_t axis,
                           const EngRay *ray)
{
    float origin[3];
    float s;
    float t;
    float distance;
    if (axis < ENG_GIZMO_X || axis > ENG_GIZMO_Z || editor->selected.generation == 0 ||
        !eng_editor_world_position(world, editor->selected, origin) ||
        !eng_ray_closest_to_line(ray, origin, AXIS_DIRECTIONS[axis], &s, &t, &distance)) {
        return false;
    }
    memcpy(editor->drag_origin, origin, sizeof origin);
    editor->drag_start = s;
    editor->drag_axis = axis;
    return true;
}

bool eng_editor_drag_update(EngEditor *editor, EngWorld *world, const EngRay *ray)
{
    float s;
    float t;
    float distance;
    if (editor->drag_axis == ENG_GIZMO_NONE ||
        !eng_ray_closest_to_line(ray, editor->drag_origin, AXIS_DIRECTIONS[editor->drag_axis], &s,
                                 &t, &distance)) {
        return false;
    }
    float position[3];
    for (int i = 0; i < 3; i++) {
        position[i] = editor->drag_origin[i] +
                      ((s - editor->drag_start) * AXIS_DIRECTIONS[editor->drag_axis][i]);
    }
    return eng_editor_set_world_position(world, editor->selected, position);
}

void eng_editor_drag_end(EngEditor *editor)
{
    editor->drag_axis = ENG_GIZMO_NONE;
}

/* ── Dessin ── */

void eng_editor_draw(const EngEditor *editor, const EngWorld *world, EngEditorBoundsFn bounds,
                     void *user, EngDebugLines *lines, const float camera_position[3])
{
    float origin[3];
    if (editor->selected.generation == 0 ||
        !eng_editor_world_position(world, editor->selected, origin)) {
        return;
    }
    const bool previous_mode = lines->overlay_mode;
    eng_debug_lines_set_overlay(lines, true);
    float box_min[3];
    float box_max[3];
    mat4 model;
    if (bounds(user, editor->selected, box_min, box_max) &&
        eng_world_world_matrix(world, editor->selected, model)) {
        vec3 center;
        vec3 half;
        for (int i = 0; i < 3; i++) {
            center[i] = 0.5F * (box_min[i] + box_max[i]);
            half[i] = 0.5F * (box_max[i] - box_min[i]);
        }
        mat4 placed;
        glm_translate_to(model, center, placed);
        (void)eng_debug_box(lines, placed, half, ENG_DEBUG_CYAN);
    }
    static const EngDebugColor colors[4] = {
        {0, 0, 0, 255}, {255, 40, 40, 255}, {40, 255, 40, 255}, {60, 90, 255, 255}};
    const float length = eng_editor_gizmo_length(camera_position, origin);
    for (uint32_t axis = ENG_GIZMO_X; axis <= ENG_GIZMO_Z; axis++) {
        const bool active = axis == editor->hover_axis || axis == editor->drag_axis;
        vec3 tip;
        for (int i = 0; i < 3; i++) {
            tip[i] = origin[i] + (length * AXIS_DIRECTIONS[axis][i]);
        }
        (void)eng_debug_arrow(lines, origin, tip, active ? ENG_DEBUG_YELLOW : colors[axis]);
    }
    eng_debug_lines_set_overlay(lines, previous_mode);
}

/* ── Édition de la scène ── */

EngEntity eng_editor_new_entity(EngEditor *editor, EngWorld *world, bool as_child)
{
    const EngEntity parent = as_child ? eng_editor_selected(editor, world) : ENG_ENTITY_NULL;
    char name[ENG_NAME_MAX];
    for (uint32_t n = 1;; n++) {
        (void)snprintf(name, sizeof name, "entité %u", (unsigned)n);
        if (eng_world_find_by_name(world, name).generation == 0) {
            break;
        }
    }
    const EngEntity entity = eng_world_create(world, name);
    if (entity.generation == 0) {
        return ENG_ENTITY_NULL;
    }
    if (eng_world_add(world, entity, ENG_COMPONENT_TRANSFORM) == NULL) {
        (void)eng_world_destroy(world, entity);
        return ENG_ENTITY_NULL;
    }
    if (parent.generation != 0 && !eng_world_set_parent(world, entity, parent)) {
        ENG_LOG_WARN("éditeur : « %s » reste à la racine (rattachement refusé)", name);
    }
    (void)eng_editor_select(editor, world, entity);
    return entity;
}

bool eng_editor_delete_selected(EngEditor *editor, EngWorld *world)
{
    const EngEntity entity = eng_editor_selected(editor, world);
    if (entity.generation == 0) {
        return false;
    }
    const bool ok = eng_world_destroy(world, entity);
    (void)eng_editor_select(editor, world, ENG_ENTITY_NULL);
    return ok;
}

void eng_editor_quat_to_euler(const float quat[4], float degrees[3])
{
    versor q = {quat[0], quat[1], quat[2], quat[3]};
    glm_quat_normalize(q);
    mat4 m;
    glm_quat_mat4(q, m);
    vec3 radians;
    glm_euler_angles(m, radians);
    for (int i = 0; i < 3; i++) {
        degrees[i] = glm_deg(radians[i]);
    }
}

void eng_editor_euler_to_quat(const float degrees[3], float quat[4])
{
    vec3 radians = {glm_rad(degrees[0]), glm_rad(degrees[1]), glm_rad(degrees[2])};
    versor q;
    glm_euler_xyz_quat(radians, q);
    glm_quat_normalize(q);
    for (int i = 0; i < 4; i++) {
        quat[i] = q[i];
    }
}

/* ── Souris ── */

bool eng_editor_pointer(EngEditor *editor, EngWorld *world, EngEditorBoundsFn bounds, void *user,
                        const EngRay *ray, const float camera_position[3], bool button_down,
                        bool over_ui)
{
    const bool pressed = button_down && !editor->button_was_down;
    bool modified = false;
    editor->button_was_down = button_down;
    (void)eng_editor_selected(editor, world); /* purge une sélection périmée */
    if (over_ui && editor->drag_axis == ENG_GIZMO_NONE) {
        editor->hover_axis = ENG_GIZMO_NONE;
        return false;
    }
    if (over_ui) { /* le pointeur est passé sur l'interface pendant un glissement : on l'abandonne
                    */
        eng_editor_drag_end(editor);
        return false;
    }
    if (pressed) {
        const uint32_t axis = eng_editor_gizmo_hit(editor, world, ray, camera_position);
        if (axis != ENG_GIZMO_NONE) {
            (void)eng_editor_drag_begin(editor, world, axis, ray);
        } else {
            (void)eng_editor_select(editor, world, eng_editor_pick(world, bounds, user, ray, NULL));
        }
    } else if (button_down) {
        if (editor->drag_axis != ENG_GIZMO_NONE) {
            modified = eng_editor_drag_update(editor, world, ray);
        }
    } else {
        eng_editor_drag_end(editor);
        editor->hover_axis = eng_editor_gizmo_hit(editor, world, ray, camera_position);
    }
    return modified;
}
