#include "engine/socket.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

#define DEG_TO_RAD 0.017453292519943295F

void eng_socketset_init(EngSocketSet *set)
{
    if (set != NULL) {
        memset(set, 0, sizeof *set);
    }
}

static bool finite_n(const float *v, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) {
            return false;
        }
    }
    return true;
}

static bool valid_name(const char *name)
{
    if (name == NULL || name[0] == '\0' || strlen(name) >= ENG_SOCKET_NAME_MAX) {
        return false;
    }
    for (const char *p = name; *p != '\0'; p++) {
        const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                        (*p >= '0' && *p <= '9') || *p == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

/* q = qz * qy * qx (X d'abord, axes fixes). */
static void euler_to_quat(const float deg[3], float q[4])
{
    versor qx;
    versor qy;
    versor qz;
    versor tmp;
    glm_quatv(qx, deg[0] * DEG_TO_RAD, (vec3){1.0F, 0.0F, 0.0F});
    glm_quatv(qy, deg[1] * DEG_TO_RAD, (vec3){0.0F, 1.0F, 0.0F});
    glm_quatv(qz, deg[2] * DEG_TO_RAD, (vec3){0.0F, 0.0F, 1.0F});
    glm_quat_mul(qz, qy, tmp);
    glm_quat_mul(tmp, qx, tmp);
    glm_quat_normalize(tmp);
    memcpy(q, tmp, 4U * sizeof(float));
}

static uint32_t find_joint(const EngSkeleton *skeleton, const char *name)
{
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        const char *joint_name = skeleton->joints[i].name;
        if (joint_name != NULL && strcmp(joint_name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool eng_socketset_add(EngSocketSet *set, const EngSkeleton *skeleton, const char *name,
                       const char *joint_name, const float translation[3],
                       const float euler_degrees[3], float scale)
{
    if (set == NULL || skeleton == NULL || skeleton->joints == NULL || joint_name == NULL) {
        ENG_LOG_ERROR("socket : argument NULL");
        return false;
    }
    if (!valid_name(name)) {
        ENG_LOG_ERROR("socket : nom invalide (1 à %u caractères parmi A-Z a-z 0-9 _)",
                      (unsigned)(ENG_SOCKET_NAME_MAX - 1U));
        return false;
    }
    if (set->count >= ENG_SOCKET_MAX) {
        ENG_LOG_ERROR("socket %s : ensemble plein (%u)", name, (unsigned)ENG_SOCKET_MAX);
        return false;
    }
    if (eng_socketset_find(set, name) != UINT32_MAX) {
        ENG_LOG_ERROR("socket %s : nom déjà pris", name);
        return false;
    }
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float *t = translation != NULL ? translation : zero;
    const float *e = euler_degrees != NULL ? euler_degrees : zero;
    if (!finite_n(t, 3) || !finite_n(e, 3) || !isfinite(scale) || scale <= 0.0F) {
        ENG_LOG_ERROR("socket %s : valeur non finie ou échelle <= 0", name);
        return false;
    }
    const uint32_t joint = find_joint(skeleton, joint_name);
    if (joint == UINT32_MAX) {
        ENG_LOG_ERROR("socket %s : articulation inconnue : %s", name, joint_name);
        return false;
    }
    EngSocket *s = &set->sockets[set->count];
    memset(s, 0, sizeof *s);
    memcpy(s->name, name, strlen(name) + 1U);
    s->joint = joint;
    memcpy(s->translation, t, sizeof s->translation);
    euler_to_quat(e, s->rotation);
    s->scale = scale;
    set->count++;
    return true;
}

uint32_t eng_socketset_find(const EngSocketSet *set, const char *name)
{
    if (set == NULL || name == NULL) {
        return UINT32_MAX;
    }
    for (uint32_t i = 0; i < set->count; i++) {
        if (strcmp(set->sockets[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool eng_socket_world(const EngSocket *socket, mat4 *joint_world, uint32_t joint_count, mat4 model,
                      mat4 out)
{
    if (socket == NULL || joint_world == NULL || model == NULL || out == NULL ||
        socket->joint >= joint_count) {
        return false;
    }
    const float *jw = &joint_world[socket->joint][0][0];
    if (!finite_n(jw, 16) || !finite_n(&model[0][0], 16)) {
        return false;
    }
    /* cadre de l'articulation sans échelle */
    mat4 frame;
    glm_mat4_identity(frame);
    for (int c = 0; c < 3; c++) {
        vec3 axis = {joint_world[socket->joint][c][0], joint_world[socket->joint][c][1],
                     joint_world[socket->joint][c][2]};
        const float len = glm_vec3_norm(axis);
        if (!(len > 1e-12F)) {
            return false;
        }
        glm_vec3_scale(axis, 1.0F / len, axis);
        frame[c][0] = axis[0];
        frame[c][1] = axis[1];
        frame[c][2] = axis[2];
    }
    frame[3][0] = joint_world[socket->joint][3][0];
    frame[3][1] = joint_world[socket->joint][3][1];
    frame[3][2] = joint_world[socket->joint][3][2];
    /* décalage local : T x R x S */
    mat4 local;
    glm_mat4_identity(local);
    versor rotation; /* copie alignée : cglm lit les quaternions en SSE (16 octets) */
    memcpy(rotation, socket->rotation, sizeof rotation);
    glm_quat_mat4(rotation, local);
    for (int c = 0; c < 3; c++) {
        local[c][0] *= socket->scale;
        local[c][1] *= socket->scale;
        local[c][2] *= socket->scale;
    }
    local[3][0] = socket->translation[0];
    local[3][1] = socket->translation[1];
    local[3][2] = socket->translation[2];
    mat4 joint_local;
    mat4 result;
    glm_mat4_mul(frame, local, joint_local);
    glm_mat4_mul(model, joint_local, result);
    glm_mat4_copy(result, out);
    return true;
}

uint32_t eng_socketset_debug_draw(const EngSocketSet *set, mat4 *joint_world, uint32_t joint_count,
                                  mat4 model, EngDebugLines *lines, float size)
{
    if (set == NULL || joint_world == NULL || model == NULL || lines == NULL) {
        return 0;
    }
    const bool was_overlay = lines->overlay_mode;
    eng_debug_lines_set_overlay(lines, true);
    uint32_t drawn = 0;
    for (uint32_t i = 0; i < set->count; i++) {
        mat4 world;
        if (eng_socket_world(&set->sockets[i], joint_world, joint_count, model, world) &&
            eng_debug_axes(lines, world, size)) {
            drawn++;
        }
    }
    eng_debug_lines_set_overlay(lines, was_overlay);
    return drawn;
}

/* ── Fichier texte ── */

static void set_error(char *error, size_t size, unsigned line, const char *fmt, const char *arg)
{
    if (error == NULL || size == 0) {
        return;
    }
    int n = snprintf(error, size, "ligne %u : ", line);
    if (n > 0 && (size_t)n < size) {
        (void)snprintf(error + n, size - (size_t)n, fmt, arg != NULL ? arg : "");
    }
}

/* Découpe `line` (modifiée) en jetons séparés par des espaces / tabulations. */
static uint32_t tokenize(char *line, char **tokens, uint32_t max)
{
    uint32_t n = 0;
    char *p = line;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        if (n == max) {
            return max + 1U;
        }
        tokens[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r') {
            p++;
        }
    }
    return n;
}

static bool parse_number(const char *tok, float *out)
{
    char *end = NULL;
    const float v = strtof(tok, &end);
    if (end == tok || *end != '\0' || !isfinite(v)) {
        return false;
    }
    *out = v;
    return true;
}

bool eng_socketset_parse(const char *text, const EngSkeleton *skeleton, EngSocketSet *set,
                         char *error, size_t error_size)
{
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    if (text == NULL || skeleton == NULL || set == NULL) {
        set_error(error, error_size, 0, "argument NULL%s", "");
        return false;
    }
    EngSocketSet candidate;
    eng_socketset_init(&candidate);
    unsigned line_no = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        line_no++;
        const char *eol = strchr(cursor, '\n');
        const size_t len = eol != NULL ? (size_t)(eol - cursor) : strlen(cursor);
        char line[256];
        if (len >= sizeof line) {
            set_error(error, error_size, line_no, "ligne trop longue%s", "");
            return false;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';
        cursor += len + (eol != NULL ? 1U : 0U);
        char *hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }
        char *tok[12];
        const uint32_t n = tokenize(line, tok, 11);
        if (n == 0) {
            continue;
        }
        if (strcmp(tok[0], "socket") != 0) {
            set_error(error, error_size, line_no, "instruction inconnue : %s", tok[0]);
            return false;
        }
        if (n < 6U || n > 10U || n == 7U || n == 8U) {
            set_error(error, error_size, line_no,
                      "socket NOM ARTICULATION tx ty tz [rx ry rz [échelle]]%s", "");
            return false;
        }
        float t[3];
        float e[3] = {0.0F, 0.0F, 0.0F};
        float scale = 1.0F;
        bool ok = true;
        for (uint32_t i = 0; i < 3U; i++) {
            ok = ok && parse_number(tok[3U + i], &t[i]);
        }
        if (n >= 9U) {
            for (uint32_t i = 0; i < 3U; i++) {
                ok = ok && parse_number(tok[6U + i], &e[i]);
            }
        }
        if (n == 10U) {
            ok = ok && parse_number(tok[9], &scale);
        }
        if (!ok) {
            set_error(error, error_size, line_no, "nombre invalide%s", "");
            return false;
        }
        if (!eng_socketset_add(&candidate, skeleton, tok[1], tok[2], t, e, scale)) {
            set_error(error, error_size, line_no, "socket refusé : %s (voir le journal)", tok[1]);
            return false;
        }
    }
    *set = candidate;
    return true;
}
