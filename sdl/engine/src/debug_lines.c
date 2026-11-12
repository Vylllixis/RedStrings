#include "engine/debug_lines.h"

#include <math.h>
#include <stdalign.h>
#include <string.h>

bool eng_debug_lines_init(EngDebugLines *lines, EngArena *arena, uint32_t max_lines)
{
    *lines = (EngDebugLines){0};
    if (max_lines == 0 || max_lines > ENG_DEBUG_MAX_LINES) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    lines->depth = ENG_ARENA_NEW_ARRAY(arena, EngDebugVertex, (size_t)max_lines * 2U);
    lines->overlay = ENG_ARENA_NEW_ARRAY(arena, EngDebugVertex, (size_t)max_lines * 2U);
    if (lines->depth == NULL || lines->overlay == NULL) {
        eng_arena_reset_to(arena, mark);
        *lines = (EngDebugLines){0};
        return false;
    }
    lines->capacity = max_lines;
    return true;
}

void eng_debug_lines_clear(EngDebugLines *lines)
{
    lines->depth_count = 0;
    lines->overlay_count = 0;
    lines->dropped = 0;
    lines->rejected = 0;
}

void eng_debug_lines_set_overlay(EngDebugLines *lines, bool overlay)
{
    lines->overlay_mode = overlay;
}

uint32_t eng_debug_lines_depth_count(const EngDebugLines *lines)
{
    return lines->depth_count / 2U;
}

uint32_t eng_debug_lines_overlay_count(const EngDebugLines *lines)
{
    return lines->overlay_count / 2U;
}

uint32_t eng_debug_lines_count(const EngDebugLines *lines)
{
    return eng_debug_lines_depth_count(lines) + eng_debug_lines_overlay_count(lines);
}

/* ── Ajout ── */

static bool all_finite(const float *values, int count)
{
    for (int i = 0; i < count; i++) {
        if (!isfinite(values[i])) {
            return false;
        }
    }
    return true;
}

static bool vec3_finite(const vec3 v)
{
    return all_finite(v, 3);
}

static bool mat4_finite(mat4 m)
{
    return all_finite(&m[0][0], 16);
}

/* Réserve `n` lignes dans la liste courante : sinon la forme entière est perdue. */
static bool reserve(EngDebugLines *lines, uint32_t n)
{
    const uint32_t used = lines->overlay_mode ? lines->overlay_count : lines->depth_count;
    if (n > lines->capacity - used / 2U) {
        lines->dropped += n;
        return false;
    }
    return true;
}

/* Ajoute une ligne dont la place a été réservée. */
static void push(EngDebugLines *lines, const vec3 from, const vec3 to, EngDebugColor color)
{
    EngDebugVertex *list = lines->overlay_mode ? lines->overlay : lines->depth;
    uint32_t *count = lines->overlay_mode ? &lines->overlay_count : &lines->depth_count;
    EngDebugVertex *v = &list[*count];
    memcpy(v[0].position, from, sizeof v[0].position);
    memcpy(v[1].position, to, sizeof v[1].position);
    v[0].color = color;
    v[1].color = color;
    *count += 2U;
}

static bool reject(EngDebugLines *lines)
{
    lines->rejected++;
    return false;
}

bool eng_debug_line(EngDebugLines *lines, const vec3 from, const vec3 to, EngDebugColor color)
{
    if (!vec3_finite(from) || !vec3_finite(to)) {
        return reject(lines);
    }
    if (!reserve(lines, 1)) {
        return false;
    }
    push(lines, from, to, color);
    return true;
}

/* Les 12 arêtes d'un parallélépipède dont on donne les 8 coins : indice = x + 2y + 4z, où x, y, z
 * valent 0 (côté bas) ou 1 (côté haut). */
static bool push_box(EngDebugLines *lines, vec3 corners[8], EngDebugColor color)
{
    if (!reserve(lines, 12)) {
        return false;
    }
    for (uint32_t i = 0; i < 8; i++) {
        for (uint32_t bit = 1; bit <= 4; bit <<= 1) {
            if ((i & bit) == 0) { /* chaque arête une seule fois : depuis son coin « bas » */
                push(lines, corners[i], corners[i | bit], color);
            }
        }
    }
    return true;
}

bool eng_debug_aabb(EngDebugLines *lines, const vec3 min, const vec3 max, EngDebugColor color)
{
    if (!vec3_finite(min) || !vec3_finite(max)) {
        return reject(lines);
    }
    vec3 corners[8];
    for (uint32_t i = 0; i < 8; i++) {
        corners[i][0] = (i & 1U) != 0 ? max[0] : min[0];
        corners[i][1] = (i & 2U) != 0 ? max[1] : min[1];
        corners[i][2] = (i & 4U) != 0 ? max[2] : min[2];
    }
    return push_box(lines, corners, color);
}

bool eng_debug_box(EngDebugLines *lines, mat4 transform, const vec3 half, EngDebugColor color)
{
    if (!mat4_finite(transform) || !vec3_finite(half) || half[0] < 0.0F || half[1] < 0.0F ||
        half[2] < 0.0F) {
        return reject(lines);
    }
    vec3 corners[8];
    for (uint32_t i = 0; i < 8; i++) {
        vec3 local = {(i & 1U) != 0 ? half[0] : -half[0], (i & 2U) != 0 ? half[1] : -half[1],
                      (i & 4U) != 0 ? half[2] : -half[2]};
        glm_mat4_mulv3(transform, local, 1.0F, corners[i]);
    }
    return push_box(lines, corners, color);
}

static uint32_t clamp_segments(uint32_t segments)
{
    if (segments == 0) {
        return ENG_DEBUG_CIRCLE_SEGMENTS;
    }
    if (segments < ENG_DEBUG_MIN_SEGMENTS) {
        return ENG_DEBUG_MIN_SEGMENTS;
    }
    return segments > ENG_DEBUG_MAX_SEGMENTS ? ENG_DEBUG_MAX_SEGMENTS : segments;
}

/* Ajoute le cercle ; la place a été réservée. */
static void push_circle(EngDebugLines *lines, const vec3 center, const vec3 u, const vec3 v,
                        EngDebugColor color, uint32_t segments)
{
    vec3 previous = {center[0] + u[0], center[1] + u[1], center[2] + u[2]};
    for (uint32_t k = 1; k <= segments; k++) {
        vec3 point;
        if (k == segments) {
            /* ferme exactement la boucle, sans erreur d'arrondi cumulée */
            for (int axis = 0; axis < 3; axis++) {
                point[axis] = center[axis] + u[axis];
            }
        } else {
            const float angle = GLM_PIf * 2.0F * (float)k / (float)segments;
            const float c = cosf(angle);
            const float s = sinf(angle);
            for (int axis = 0; axis < 3; axis++) {
                point[axis] = center[axis] + u[axis] * c + v[axis] * s;
            }
        }
        push(lines, previous, point, color);
        glm_vec3_copy(point, previous);
    }
}

bool eng_debug_circle(EngDebugLines *lines, const vec3 center, const vec3 axis_u, const vec3 axis_v,
                      EngDebugColor color, uint32_t segments)
{
    if (!vec3_finite(center) || !vec3_finite(axis_u) || !vec3_finite(axis_v)) {
        return reject(lines);
    }
    const uint32_t n = clamp_segments(segments);
    if (!reserve(lines, n)) {
        return false;
    }
    push_circle(lines, center, axis_u, axis_v, color, n);
    return true;
}

bool eng_debug_sphere(EngDebugLines *lines, const vec3 center, float radius, EngDebugColor color,
                      uint32_t segments)
{
    if (!vec3_finite(center) || !isfinite(radius) || radius < 0.0F) {
        return reject(lines);
    }
    const uint32_t n = clamp_segments(segments);
    if (!reserve(lines, 3U * n)) {
        return false;
    }
    const vec3 x = {radius, 0.0F, 0.0F};
    const vec3 y = {0.0F, radius, 0.0F};
    const vec3 z = {0.0F, 0.0F, radius};
    push_circle(lines, center, x, y, color, n);
    push_circle(lines, center, y, z, color, n);
    push_circle(lines, center, x, z, color, n);
    return true;
}

bool eng_debug_axes(EngDebugLines *lines, mat4 transform, float size)
{
    if (!mat4_finite(transform) || !isfinite(size)) {
        return reject(lines);
    }
    if (!reserve(lines, 3)) {
        return false;
    }
    const EngDebugColor colors[3] = {ENG_DEBUG_RED, ENG_DEBUG_GREEN, ENG_DEBUG_BLUE};
    for (int axis = 0; axis < 3; axis++) {
        vec3 tip;
        glm_vec3_scale(transform[axis], size, tip);
        glm_vec3_add(tip, transform[3], tip);
        push(lines, transform[3], tip, colors[axis]);
    }
    return true;
}

bool eng_debug_grid(EngDebugLines *lines, const vec3 center, uint32_t cells, float cell_size,
                    EngDebugColor color, EngDebugColor major_color)
{
    if (!vec3_finite(center) || !isfinite(cell_size) || cell_size <= 0.0F ||
        cells > ENG_DEBUG_MAX_GRID_CELLS) {
        return reject(lines);
    }
    const uint32_t per_direction = 2U * cells + 1U;
    if (!reserve(lines, 2U * per_direction)) {
        return false;
    }
    const float extent = (float)cells * cell_size;
    for (int32_t i = -(int32_t)cells; i <= (int32_t)cells; i++) {
        const float offset = (float)i * cell_size;
        const EngDebugColor c = i % ENG_DEBUG_GRID_MAJOR_EVERY == 0 ? major_color : color;
        const vec3 x0 = {center[0] - extent, center[1], center[2] + offset};
        const vec3 x1 = {center[0] + extent, center[1], center[2] + offset};
        push(lines, x0, x1, c);
        const vec3 z0 = {center[0] + offset, center[1], center[2] - extent};
        const vec3 z1 = {center[0] + offset, center[1], center[2] + extent};
        push(lines, z0, z1, c);
    }
    return true;
}

bool eng_debug_arrow(EngDebugLines *lines, const vec3 from, const vec3 to, EngDebugColor color)
{
    if (!vec3_finite(from) || !vec3_finite(to)) {
        return reject(lines);
    }
    vec3 d = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const float length = glm_vec3_norm(d);
    if (!isfinite(length)) {
        return reject(lines); /* différence qui déborde */
    }
    if (length < 1e-6F) {
        return eng_debug_line(lines, from, to, color);
    }
    if (!reserve(lines, 5)) {
        return false;
    }
    push(lines, from, to, color);
    vec3 dir;
    glm_vec3_scale(d, 1.0F / length, dir);
    /* deux directions perpendiculaires à la flèche */
    vec3 helper = {0.0F, 1.0F, 0.0F};
    if (fabsf(dir[1]) > 0.99F) {
        helper[0] = 1.0F;
        helper[1] = 0.0F;
    }
    vec3 side;
    vec3 up;
    glm_vec3_cross(dir, helper, side);
    glm_vec3_normalize(side);
    glm_vec3_cross(dir, side, up);
    const float head = length * 0.15F;
    for (int i = 0; i < 4; i++) {
        const float sign = (i & 1) != 0 ? -1.0F : 1.0F;
        const float *perp = i < 2 ? side : up;
        vec3 tip;
        for (int axis = 0; axis < 3; axis++) {
            tip[axis] = to[axis] - dir[axis] * head + perp[axis] * sign * head * 0.5F;
        }
        push(lines, to, tip, color);
    }
    return true;
}

bool eng_debug_frustum(EngDebugLines *lines, mat4 view_proj, EngDebugColor color)
{
    if (!mat4_finite(view_proj)) {
        return reject(lines);
    }
    mat4 inverse;
    glm_mat4_inv(view_proj, inverse);
    vec3 corners[8];
    for (uint32_t i = 0; i < 8; i++) {
        /* x, y dans [-1, 1], profondeur z dans [0, 1] (SDL_GPU) */
        vec4 ndc = {(i & 1U) != 0 ? 1.0F : -1.0F, (i & 2U) != 0 ? 1.0F : -1.0F,
                    (i & 4U) != 0 ? 1.0F : 0.0F, 1.0F};
        vec4 world;
        glm_mat4_mulv(inverse, ndc, world);
        if (!isfinite(world[3]) || fabsf(world[3]) < 1e-12F) {
            return reject(lines);
        }
        for (int axis = 0; axis < 3; axis++) {
            corners[i][axis] = world[axis] / world[3];
        }
        if (!vec3_finite(corners[i])) {
            return reject(lines);
        }
    }
    return push_box(lines, corners, color);
}
