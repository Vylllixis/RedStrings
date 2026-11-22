#include "engine/anim_ik.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define MIN_SLACK 0.001F /* m : marge avant la jambe droite / repliée */

static bool finite3(const float *v)
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

/* Rotation (quaternion) d'une matrice monde dont on retire l'échelle (colonnes normalisées). */
static bool world_rotation(mat4 *m, versor out)
{
    mat3 r;
    for (int c = 0; c < 3; c++) {
        vec3 axis = {(*m)[c][0], (*m)[c][1], (*m)[c][2]};
        const float len = glm_vec3_norm(axis);
        if (!(len > 1e-12F)) {
            return false;
        }
        for (int k = 0; k < 3; k++) {
            r[c][k] = axis[k] / len;
        }
    }
    glm_mat3_quat(r, out);
    glm_quat_normalize(out);
    return true;
}

/* Rotation la plus courte qui amène `from` sur `to` (vecteurs non nuls). */
static void shortest_arc(const vec3 from, const vec3 to, versor out)
{
    vec3 a;
    vec3 b;
    glm_vec3_normalize_to((float *)from, a);
    glm_vec3_normalize_to((float *)to, b);
    const float d = glm_vec3_dot(a, b);
    if (d < -0.999999F) { /* opposés : demi-tour autour d'un axe quelconque perpendiculaire */
        vec3 axis;
        glm_vec3_cross(a, (vec3){1.0F, 0.0F, 0.0F}, axis);
        if (glm_vec3_norm2(axis) < 1e-6F) {
            glm_vec3_cross(a, (vec3){0.0F, 1.0F, 0.0F}, axis);
        }
        glm_vec3_normalize(axis);
        glm_quatv(out, GLM_PIf, axis);
        return;
    }
    vec3 axis;
    glm_vec3_cross(a, b, axis);
    out[0] = axis[0];
    out[1] = axis[1];
    out[2] = axis[2];
    out[3] = 1.0F + d;
    glm_quat_normalize(out);
}

/* Applique la rotation monde `delta` à l'articulation `j` : locale' = parent^-1 * (delta * monde).
 */
static bool rotate_joint(EngPose *pose, const EngSkeleton *skeleton, mat4 *world, uint32_t j,
                         const versor delta)
{
    versor wq;
    if (!world_rotation(&world[j], wq)) {
        return false;
    }
    versor new_world;
    glm_quat_mul((float *)delta, wq, new_world);
    versor parent_q = {0.0F, 0.0F, 0.0F, 1.0F};
    const int32_t parent = skeleton->joints[j].parent;
    if (parent >= 0 && !world_rotation(&world[parent], parent_q)) {
        return false;
    }
    versor inv;
    glm_quat_inv(parent_q, inv);
    versor local;
    glm_quat_mul(inv, new_world, local);
    glm_quat_normalize(local);
    memcpy(&pose->rotation[4U * (size_t)j], local, 4U * sizeof(float));
    return true;
}

static void position_of(mat4 *world, uint32_t j, vec3 out)
{
    out[0] = world[j][3][0];
    out[1] = world[j][3][1];
    out[2] = world[j][3][2];
}

bool eng_ik_two_bone(EngPose *pose, const EngSkeleton *skeleton, uint32_t upper, uint32_t lower,
                     uint32_t end, const float target[3], const float pole[3], float weight)
{
    if (pose == NULL || skeleton == NULL || skeleton->joints == NULL || target == NULL ||
        pose->translation == NULL || pose->rotation == NULL || pose->scale == NULL) {
        ENG_LOG_ERROR("ik : argument NULL");
        return false;
    }
    const uint32_t n = skeleton->joint_count;
    if (pose->joint_count != n || n > ENG_SKELETON_MAX_JOINTS) {
        ENG_LOG_ERROR("ik : la pose et le squelette n'ont pas le même nombre d'articulations");
        return false;
    }
    if (upper >= n || lower >= n || end >= n || skeleton->joints[lower].parent != (int32_t)upper ||
        skeleton->joints[end].parent != (int32_t)lower) {
        ENG_LOG_ERROR(
            "ik : la chaîne doit être cuisse -> genou -> pied (parent -> enfant -> enfant)");
        return false;
    }
    if (!finite3(target) || (pole != NULL && !finite3(pole)) || !isfinite(weight) ||
        weight < 0.0F || weight > 1.0F) {
        ENG_LOG_ERROR("ik : cible, pôle ou poids invalide (poids dans [0 ; 1])");
        return false;
    }
    if (weight == 0.0F) {
        return true;
    }
    mat4 world[ENG_SKELETON_MAX_JOINTS];
    if (!eng_pose_world(pose, skeleton, world)) {
        return false;
    }
    vec3 a;
    vec3 b;
    vec3 c;
    position_of(world, upper, a);
    position_of(world, lower, b);
    position_of(world, end, c);
    vec3 ab;
    vec3 bc;
    glm_vec3_sub(b, a, ab);
    glm_vec3_sub(c, b, bc);
    const float l1 = glm_vec3_norm(ab);
    const float l2 = glm_vec3_norm(bc);
    if (!(l1 > 1e-6F) || !(l2 > 1e-6F)) {
        ENG_LOG_ERROR("ik : os de longueur nulle");
        return false;
    }
    vec3 to_target;
    glm_vec3_sub((float *)target, a, to_target);
    float dist = glm_vec3_norm(to_target);
    vec3 dir;
    if (dist > 1e-9F) {
        glm_vec3_scale(to_target, 1.0F / dist, dir);
    } else {
        glm_vec3_copy((vec3){0.0F, -1.0F, 0.0F}, dir); /* cible sur la cuisse : arbitraire */
    }
    const float reach_max = l1 + l2 - MIN_SLACK;
    float reach_min = fabsf(l1 - l2);
    if (reach_min < MIN_SLACK) {
        reach_min = MIN_SLACK;
    }
    if (reach_max <= reach_min) { /* os minuscules : plus de marge à ménager */
        reach_min = fabsf(l1 - l2);
    }
    dist = dist > reach_max ? reach_max : dist;
    dist = dist < reach_min ? reach_min : dist;

    /* sens de pliage : composante de la direction voulue perpendiculaire à `dir` */
    vec3 hint;
    if (pole != NULL) {
        glm_vec3_sub((float *)pole, a, hint);
    } else {
        /* côté actuel du genou : son écart à la droite (A, pied) ; nul si la jambe est droite */
        vec3 ac;
        glm_vec3_sub(c, a, ac);
        const float ac_len = glm_vec3_norm(ac);
        glm_vec3_zero(hint);
        if (ac_len > 1e-9F) {
            glm_vec3_scale(ac, 1.0F / ac_len, ac);
            vec3 along;
            glm_vec3_scale(ac, glm_vec3_dot(ab, ac), along);
            glm_vec3_sub(ab, along, hint);
        }
        if (glm_vec3_norm(hint) < 1e-4F) { /* jambe droite à 0,1 mm près : pas de côté */
            glm_vec3_zero(hint);
        }
    }
    vec3 bend;
    glm_vec3_scale(dir, glm_vec3_dot(hint, dir), bend);
    glm_vec3_sub(hint, bend, bend);
    if (glm_vec3_norm(bend) < 1e-6F) { /* pas de pôle exploitable : l'avant, sinon +X */
        glm_vec3_copy((vec3){0.0F, 0.0F, -1.0F}, hint);
        glm_vec3_scale(dir, glm_vec3_dot(hint, dir), bend);
        glm_vec3_sub(hint, bend, bend);
        if (glm_vec3_norm(bend) < 1e-6F) {
            glm_vec3_copy((vec3){1.0F, 0.0F, 0.0F}, hint);
            glm_vec3_scale(dir, glm_vec3_dot(hint, dir), bend);
            glm_vec3_sub(hint, bend, bend);
        }
    }
    glm_vec3_normalize(bend);

    /* triangle (a, genou, pied) : angle en `a` entre la direction de la cible et la cuisse */
    float cos_a = ((l1 * l1) + (dist * dist) - (l2 * l2)) / (2.0F * l1 * dist);
    cos_a = cos_a > 1.0F ? 1.0F : (cos_a < -1.0F ? -1.0F : cos_a);
    const float sin_a = sqrtf(1.0F - (cos_a * cos_a));
    vec3 b_new;
    vec3 c_new;
    for (int k = 0; k < 3; k++) {
        b_new[k] = a[k] + (l1 * ((cos_a * dir[k]) + (sin_a * bend[k])));
        c_new[k] = a[k] + (dist * dir[k]);
    }

    /* sauvegarde pour le mélange et pour restituer la pose si un calcul échoue */
    float saved_upper[4];
    float saved_lower[4];
    memcpy(saved_upper, &pose->rotation[4U * (size_t)upper], sizeof saved_upper);
    memcpy(saved_lower, &pose->rotation[4U * (size_t)lower], sizeof saved_lower);

    vec3 ab_new;
    glm_vec3_sub(b_new, a, ab_new);
    versor q1;
    shortest_arc(ab, ab_new, q1);
    bool ok = rotate_joint(pose, skeleton, world, upper, q1);
    if (ok) {
        ok = eng_pose_world(pose, skeleton, world);
    }
    vec3 b_now;
    vec3 c_now;
    if (ok) {
        position_of(world, lower, b_now);
        position_of(world, end, c_now);
        vec3 from;
        vec3 to;
        glm_vec3_sub(c_now, b_now, from);
        glm_vec3_sub(c_new, b_now, to);
        if (glm_vec3_norm(from) > 1e-9F && glm_vec3_norm(to) > 1e-9F) {
            versor q2;
            shortest_arc(from, to, q2);
            ok = rotate_joint(pose, skeleton, world, lower, q2);
        }
    }
    if (!ok) {
        memcpy(&pose->rotation[4U * (size_t)upper], saved_upper, sizeof saved_upper);
        memcpy(&pose->rotation[4U * (size_t)lower], saved_lower, sizeof saved_lower);
        ENG_LOG_ERROR("ik : matrice d'articulation dégénérée");
        return false;
    }
    if (weight < 1.0F) {
        const float *old[2] = {saved_upper, saved_lower};
        const uint32_t joints[2] = {upper, lower};
        for (int i = 0; i < 2; i++) {
            float *q = &pose->rotation[4U * (size_t)joints[i]];
            const float dot =
                (old[i][0] * q[0]) + (old[i][1] * q[1]) + (old[i][2] * q[2]) + (old[i][3] * q[3]);
            const float sign = dot < 0.0F ? -1.0F : 1.0F;
            float m[4];
            float len2 = 0.0F;
            for (int k = 0; k < 4; k++) {
                m[k] = (old[i][k] * (1.0F - weight)) + (sign * q[k] * weight);
                len2 += m[k] * m[k];
            }
            const float inv = 1.0F / sqrtf(len2);
            for (int k = 0; k < 4; k++) {
                q[k] = m[k] * inv;
            }
        }
    }
    return true;
}
