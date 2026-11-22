#include "engine/hurtbox.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"
#include "engine/socket.h"

static const char *const ZONE_NAMES[ENG_ZONE_COUNT] = {"head", "torso", "limb"};

const char *eng_zone_name(EngHitZone z)
{
    return ((unsigned)z < (unsigned)ENG_ZONE_COUNT) ? ZONE_NAMES[z] : "?";
}

EngDebugColor eng_zone_color(EngHitZone z)
{
    switch (z) {
    case ENG_ZONE_HEAD:
        return ENG_DEBUG_MAGENTA;
    case ENG_ZONE_TORSO:
        return ENG_DEBUG_YELLOW;
    default:
        return ENG_DEBUG_CYAN;
    }
}

void eng_hurtboxset_init(EngHurtboxSet *set)
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
    if (name == NULL || name[0] == '\0' || strlen(name) >= ENG_HURTBOX_NAME_MAX) {
        return false;
    }
    for (const char *p = name; *p != '\0'; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return false;
        }
    }
    return true;
}

static uint32_t find_joint(const EngSkeleton *skeleton, const char *name)
{
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        const char *jn = skeleton->joints[i].name;
        if (jn != NULL && strcmp(jn, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t eng_hurtboxset_find(const EngHurtboxSet *set, const char *name)
{
    for (uint32_t i = 0; set != NULL && name != NULL && i < set->count; i++) {
        if (strcmp(set->boxes[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool eng_hurtboxset_add(EngHurtboxSet *set, const EngSkeleton *skeleton, const char *name,
                        EngHitZone zone, const char *joint_name, const float a[3], const float b[3],
                        float radius)
{
    if (set == NULL || skeleton == NULL || skeleton->joints == NULL || joint_name == NULL ||
        a == NULL || b == NULL) {
        ENG_LOG_ERROR("hurtbox : argument NULL");
        return false;
    }
    if (!valid_name(name)) {
        ENG_LOG_ERROR("hurtbox : nom invalide (1 à %u caractères parmi A-Z a-z 0-9 _)",
                      ENG_HURTBOX_NAME_MAX - 1U);
        return false;
    }
    if (set->count >= ENG_HURTBOX_MAX) {
        ENG_LOG_ERROR("hurtbox : ensemble plein (%u)", ENG_HURTBOX_MAX);
        return false;
    }
    if (eng_hurtboxset_find(set, name) != UINT32_MAX) {
        ENG_LOG_ERROR("hurtbox : nom déjà pris « %s »", name);
        return false;
    }
    if ((unsigned)zone >= (unsigned)ENG_ZONE_COUNT) {
        ENG_LOG_ERROR("hurtbox : zone invalide");
        return false;
    }
    const uint32_t joint = find_joint(skeleton, joint_name);
    if (joint == UINT32_MAX) {
        ENG_LOG_ERROR("hurtbox : articulation inconnue « %s »", joint_name);
        return false;
    }
    if (!finite_n(a, 3) || !finite_n(b, 3) || !isfinite(radius) || !(radius > 0.0F) ||
        radius > 10.0F) {
        ENG_LOG_ERROR("hurtbox : valeur invalide (rayon dans ]0 ; 10] m)");
        return false;
    }
    EngHurtbox *h = &set->boxes[set->count++];
    memset(h, 0, sizeof *h);
    snprintf(h->name, sizeof h->name, "%s", name);
    h->zone = zone;
    h->joint = joint;
    memcpy(h->a, a, sizeof h->a);
    memcpy(h->b, b, sizeof h->b);
    h->radius = radius;
    return true;
}

static void set_error(char *error, size_t size, unsigned line, const char *msg, const char *arg)
{
    if (error == NULL || size == 0) {
        return;
    }
    const int n = snprintf(error, size, "ligne %u : ", line);
    if (n > 0 && (size_t)n < size) {
        (void)snprintf(error + n, size - (size_t)n, "%s%s%s", msg, arg != NULL ? " " : "",
                       arg != NULL ? arg : "");
    }
}

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

bool eng_hurtboxset_parse(const char *text, const EngSkeleton *skeleton, EngHurtboxSet *set,
                          char *error, size_t error_size)
{
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    if (text == NULL || skeleton == NULL || set == NULL) {
        set_error(error, error_size, 0, "argument NULL", NULL);
        return false;
    }
    EngHurtboxSet candidate;
    eng_hurtboxset_init(&candidate);
    unsigned line_no = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        line_no++;
        const char *eol = strchr(cursor, '\n');
        const size_t len = eol != NULL ? (size_t)(eol - cursor) : strlen(cursor);
        char line[256];
        if (len >= sizeof line) {
            set_error(error, error_size, line_no, "ligne trop longue", NULL);
            return false;
        }
        memcpy(line, cursor, len);
        line[len] = '\0';
        cursor += len + (eol != NULL ? 1U : 0U);
        char *hash = strchr(line, '#');
        if (hash != NULL) {
            *hash = '\0';
        }
        char *tok[13];
        const uint32_t n = tokenize(line, tok, 12);
        if (n == 0) {
            continue;
        }
        if (strcmp(tok[0], "hurtbox") != 0) {
            set_error(error, error_size, line_no, "instruction inconnue :", tok[0]);
            return false;
        }
        if (n != 11U) {
            set_error(error, error_size, line_no,
                      "hurtbox NOM ZONE ARTICULATION ax ay az bx by bz RAYON attendu", NULL);
            return false;
        }
        int zone = -1;
        for (int z = 0; z < ENG_ZONE_COUNT; z++) {
            if (strcmp(tok[2], ZONE_NAMES[z]) == 0) {
                zone = z;
            }
        }
        if (zone < 0) {
            set_error(error, error_size, line_no, "zone inconnue (head, torso ou limb) :", tok[2]);
            return false;
        }
        float v[7];
        bool ok = true;
        for (uint32_t i = 0; i < 7U; i++) {
            ok = ok && parse_number(tok[4U + i], &v[i]);
        }
        if (!ok) {
            set_error(error, error_size, line_no, "nombre invalide", NULL);
            return false;
        }
        if (!eng_hurtboxset_add(&candidate, skeleton, tok[1], (EngHitZone)zone, tok[3], v, v + 3,
                                v[6])) {
            set_error(error, error_size, line_no, "hurtbox refusée (voir le journal) :", tok[1]);
            return false;
        }
    }
    *set = candidate;
    return true;
}

bool eng_capsule_on_joint(uint32_t joint, const float a[3], const float b[3], float radius,
                          mat4 *joint_world, uint32_t joint_count, mat4 model, EngCapsule *out)
{
    if (a == NULL || b == NULL || joint_world == NULL || model == NULL || out == NULL ||
        joint >= joint_count || !finite_n(a, 3) || !finite_n(b, 3) || !isfinite(radius)) {
        return false;
    }
    const EngSocket sa = {.joint = joint,
                          .translation = {a[0], a[1], a[2]},
                          .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                          .scale = 1.0F};
    EngSocket sb = sa;
    memcpy(sb.translation, b, sizeof sb.translation);
    mat4 wa;
    mat4 wb;
    if (!eng_socket_world(&sa, joint_world, joint_count, model, wa) ||
        !eng_socket_world(&sb, joint_world, joint_count, model, wb)) {
        return false;
    }
    const float s =
        sqrtf(model[0][0] * model[0][0] + model[0][1] * model[0][1] + model[0][2] * model[0][2]);
    EngCapsule c;
    for (int i = 0; i < 3; i++) {
        c.a[i] = wa[3][i];
        c.b[i] = wb[3][i];
    }
    c.radius = radius * s;
    *out = c;
    return true;
}

bool eng_hurtboxset_world(const EngHurtboxSet *set, mat4 *joint_world, uint32_t joint_count,
                          mat4 model, EngCapsule *out)
{
    if (set == NULL || out == NULL) {
        return false;
    }
    bool ok = true;
    for (uint32_t i = 0; i < set->count; i++) {
        const EngHurtbox *h = &set->boxes[i];
        if (!eng_capsule_on_joint(h->joint, h->a, h->b, h->radius, joint_world, joint_count, model,
                                  &out[i])) {
            memset(&out[i], 0, sizeof out[i]);
            out[i].radius = -1.0F; /* invalide : eng_capsule_overlap la refuse */
            ok = false;
        }
    }
    return ok;
}

bool eng_capsule_debug_draw(EngDebugLines *lines, const EngCapsule *c, EngDebugColor color)
{
    if (lines == NULL || c == NULL || !(c->radius >= 0.0F)) {
        return false;
    }
    bool ok = eng_debug_sphere(lines, c->a, c->radius, color, 12) &&
              eng_debug_sphere(lines, c->b, c->radius, color, 12);
    /* 4 génératrices dans les plans X et Z de l'axe monde (approximation lisible) */
    const float off[4][3] = {
        {c->radius, 0, 0}, {-c->radius, 0, 0}, {0, 0, c->radius}, {0, 0, -c->radius}};
    for (int i = 0; i < 4; i++) {
        vec3 p = {c->a[0] + off[i][0], c->a[1] + off[i][1], c->a[2] + off[i][2]};
        vec3 q = {c->b[0] + off[i][0], c->b[1] + off[i][1], c->b[2] + off[i][2]};
        ok = eng_debug_line(lines, p, q, color) && ok;
    }
    return ok;
}
