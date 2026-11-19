#include "engine/anim_machine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"

static bool clip_fits(const EngSkeleton *sk, const EngAnimClip *clip)
{
    if (clip == NULL || !isfinite(clip->duration) || clip->duration < 0.0F ||
        (clip->channel_count > 0 && clip->channels == NULL)) {
        return false;
    }
    for (uint32_t i = 0; i < clip->channel_count; i++) {
        if (clip->channels[i].joint >= sk->joint_count) {
            return false;
        }
    }
    return true;
}

static bool pose_copy(EngPose *dst, const EngPose *src)
{
    if (dst->joint_count != src->joint_count) {
        return false;
    }
    memcpy(dst->translation, src->translation, sizeof(float) * 3U * src->joint_count);
    memcpy(dst->rotation, src->rotation, sizeof(float) * 4U * src->joint_count);
    memcpy(dst->scale, src->scale, sizeof(float) * 3U * src->joint_count);
    return true;
}

bool eng_animmachine_init(EngAnimMachine *m, EngArena *arena, const EngSkeleton *skeleton)
{
    if (m == NULL) {
        return false;
    }
    memset(m, 0, sizeof *m);
    if (arena == NULL || skeleton == NULL) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    bool ok =
        eng_pose_init(arena, skeleton, &m->pose) && eng_pose_init(arena, skeleton, &m->snapshot);
    for (uint32_t i = 0; ok && i < 2; i++) {
        ok = eng_pose_init(arena, skeleton, &m->slots[i].pose) &&
             eng_blendspace_init(&m->slots[i].bs, arena, skeleton);
    }
    if (!ok) {
        eng_arena_reset_to(arena, mark);
        memset(m, 0, sizeof *m);
        return false;
    }
    m->skeleton = skeleton;
    m->root_joint = UINT32_MAX;
    return true;
}

/* Prépare `slot` pour jouer `state` ; `time` (état clip) et `phase` (état blend) donnent le point
 * de reprise (0 pour un départ). */
static void slot_enter(EngAnimMachine *m, EngAnimSlot *slot, uint32_t state, float time,
                       float phase)
{
    const EngAnimGraphState *st = &m->def.states[state];
    slot->state = state;
    slot->ended = false;
    memset(&slot->layer, 0, sizeof slot->layer);
    eng_blendspace_clear(&slot->bs);
    if (st->kind == ENG_ANIMSTATE_CLIP) {
        const EngAnimGraphClip *c = &m->def.clips[st->clip];
        const EngAnimClip *clip = m->clips[st->clip];
        slot->layer = (EngAnimLayer){.clip = clip, .speed = c->speed, .loop = c->loop};
        slot->layer.time = fminf(fmaxf(time, 0.0F), clip->duration);
        return;
    }
    for (uint32_t i = 0; i < st->sample_count; i++) {
        (void)eng_blendspace_add(&slot->bs, m->clips[st->sample_clip[i]], st->sample_position[i]);
    }
    (void)eng_blendspace_set_phase(&slot->bs, phase);
}

/* Avance le slot et rend la pose qu'il produit ; `motion` reçoit le déplacement de la racine. */
static const EngPose *slot_update(EngAnimMachine *m, EngAnimSlot *slot, float dt,
                                  EngRootMotion *motion)
{
    const EngAnimGraphState *st = &m->def.states[slot->state];
    memset(motion, 0, sizeof *motion);
    if (st->kind == ENG_ANIMSTATE_BLEND) {
        if (!eng_blendspace_update(&slot->bs, dt, m->params[st->param])) {
            return NULL;
        }
        *motion = slot->bs.motion;
        return &slot->bs.pose;
    }
    const float t0 = slot->layer.time;
    const float t1 = t0 + (dt * slot->layer.speed);
    if (eng_anim_layer_advance(&slot->layer, dt)) {
        slot->ended = true;
    }
    if (!eng_pose_sample(&slot->pose, m->skeleton, slot->layer.clip, slot->layer.time,
                         ENG_ANIM_WRAP_CLAMP)) {
        return NULL;
    }
    if (m->root_joint != UINT32_MAX) {
        (void)eng_rootmotion_delta(m->skeleton, slot->layer.clip, m->root_joint, t0, t1,
                                   slot->layer.loop, motion);
        (void)eng_rootmotion_strip(&slot->pose, m->skeleton, slot->layer.clip, m->root_joint);
    }
    return &slot->pose;
}

bool eng_animmachine_set_def(EngAnimMachine *m, const EngAnimGraphDef *def,
                             EngAnimClipFinder finder, void *user)
{
    if (m == NULL || m->skeleton == NULL || def == NULL || finder == NULL ||
        def->state_count == 0 || def->start >= def->state_count) {
        return false;
    }
    const EngAnimClip *resolved[ENG_ANIMGRAPH_MAX_CLIPS];
    for (uint32_t i = 0; i < def->clip_count; i++) {
        resolved[i] = finder(user, def->clips[i].name, def->clips[i].path);
        if (resolved[i] == NULL || !clip_fits(m->skeleton, resolved[i])) {
            ENG_LOG_ERROR("machine d'animation : clip « %s » (%s) introuvable ou incompatible avec "
                          "le squelette",
                          def->clips[i].name, def->clips[i].path);
            return false;
        }
    }
    for (uint32_t s = 0; s < def->state_count; s++) {
        const EngAnimGraphState *st = &def->states[s];
        for (uint32_t k = 0; st->kind == ENG_ANIMSTATE_BLEND && k < st->sample_count; k++) {
            if (!(resolved[st->sample_clip[k]]->duration > 0.0F)) {
                ENG_LOG_ERROR("machine d'animation : le clip « %s » a une durée nulle (état %s)",
                              def->clips[st->sample_clip[k]].name, st->name);
                return false;
            }
        }
    }
    uint32_t root = UINT32_MAX;
    if (def->has_root_motion) {
        for (uint32_t j = 0; j < m->skeleton->joint_count; j++) {
            const char *name = m->skeleton->joints[j].name;
            if (name != NULL && strcmp(name, def->root_joint) == 0) {
                root = j;
                break;
            }
        }
        if (root == UINT32_MAX || m->skeleton->joints[root].parent != -1) {
            ENG_LOG_ERROR("machine d'animation : root_motion « %s » : articulation introuvable ou "
                          "qui n'est pas une racine",
                          def->root_joint);
            return false;
        }
    }
    /* de quoi reprendre : l'état courant par son nom, les paramètres par leur nom */
    char old_state[ENG_ANIMGRAPH_MAX_NAME];
    old_state[0] = '\0';
    float resume_time = 0.0F;
    float resume_phase = 0.0F;
    if (m->ready) {
        const EngAnimSlot *cur = &m->slots[m->cur];
        (void)snprintf(old_state, sizeof old_state, "%s", m->def.states[cur->state].name);
        resume_time = cur->layer.time;
        resume_phase = cur->bs.phase;
    }
    float old_params[ENG_ANIMGRAPH_MAX_PARAMS];
    EngAnimGraphDef old_def = m->def;
    memcpy(old_params, m->params, sizeof old_params);
    const bool was_ready = m->ready;

    m->def = *def;
    m->root_joint = root;
    for (uint32_t i = 0; i < 2; i++) {
        (void)eng_blendspace_set_root_joint(&m->slots[i].bs, root);
    }
    for (uint32_t i = 0; i < ENG_ANIMGRAPH_MAX_CLIPS; i++) {
        m->clips[i] = i < def->clip_count ? resolved[i] : NULL;
    }
    for (uint32_t i = 0; i < ENG_ANIMGRAPH_MAX_PARAMS; i++) {
        m->params[i] = i < def->param_count ? def->params[i].initial : 0.0F;
    }
    for (uint32_t i = 0; was_ready && i < def->param_count; i++) {
        const uint32_t old = eng_animgraph_find_param(&old_def, def->params[i].name);
        if (old != UINT32_MAX) {
            m->params[i] = old_params[old];
        }
    }
    uint32_t state = def->start;
    bool resume = false;
    if (was_ready) {
        const uint32_t same = eng_animgraph_find_state(def, old_state);
        if (same != UINT32_MAX) {
            state = same;
            resume = true;
        }
    }
    m->has_from = false;
    m->from_frozen = false;
    m->elapsed = 0.0F;
    m->fade = 0.0F;
    slot_enter(m, &m->slots[m->cur], state, resume ? resume_time : 0.0F,
               resume ? resume_phase : 0.0F);
    m->ready = true;
    EngRootMotion first;
    const EngPose *out = slot_update(m, &m->slots[m->cur], 0.0F, &first);
    memset(&m->motion, 0, sizeof m->motion);
    return out != NULL && pose_copy(&m->pose, out);
}

bool eng_animmachine_set_param_index(EngAnimMachine *m, uint32_t index, float value)
{
    if (m == NULL || !m->ready || index >= m->def.param_count || !isfinite(value)) {
        return false;
    }
    m->params[index] = value;
    return true;
}

bool eng_animmachine_set_param(EngAnimMachine *m, const char *name, float value)
{
    if (m == NULL || !m->ready) {
        return false;
    }
    const uint32_t index = eng_animgraph_find_param(&m->def, name);
    return index != UINT32_MAX && eng_animmachine_set_param_index(m, index, value);
}

static bool condition_true(const EngAnimMachine *m, const EngAnimGraphTransition *t)
{
    if (t->cond == ENG_ANIMCOND_ENDED) {
        const EngAnimSlot *cur = &m->slots[m->cur];
        return m->def.states[cur->state].kind == ENG_ANIMSTATE_CLIP && !cur->layer.loop &&
               cur->ended;
    }
    const float v = m->params[t->param];
    switch (t->cond) {
    case ENG_ANIMCOND_GT:
        return v > t->threshold;
    case ENG_ANIMCOND_LT:
        return v < t->threshold;
    case ENG_ANIMCOND_GE:
        return v >= t->threshold;
    case ENG_ANIMCOND_LE:
        return v <= t->threshold;
    case ENG_ANIMCOND_ENDED:
    default:
        return false;
    }
}

static void enter_state(EngAnimMachine *m, uint32_t to, float fade)
{
    const bool fading = m->has_from && m->elapsed < m->fade;
    if (fade <= 0.0F) {
        m->has_from = false;
    } else if (fading) {
        (void)pose_copy(&m->snapshot, &m->pose); /* continuité : on fige ce qui est affiché */
        m->from_frozen = true;
        m->has_from = true;
    } else {
        m->from_frozen = false;
        m->has_from = true;
        m->cur = 1U - m->cur; /* l'ancien état courant devient la source */
    }
    slot_enter(m, &m->slots[m->cur], to, 0.0F, 0.0F);
    m->fade = fade;
    m->elapsed = 0.0F;
    m->transitions_taken++;
}

bool eng_animmachine_update(EngAnimMachine *m, float dt)
{
    if (m == NULL || !m->ready) {
        return false;
    }
    if (!isfinite(dt) || dt < 0.0F) {
        dt = 0.0F;
    }
    const uint32_t current = m->slots[m->cur].state;
    for (uint32_t i = 0; i < m->def.transition_count; i++) {
        const EngAnimGraphTransition *t = &m->def.transitions[i];
        if ((t->from == current || (t->from == ENG_ANIMGRAPH_ANY && t->to != current)) &&
            condition_true(m, t)) {
            enter_state(m, t->to, t->fade);
            break;
        }
    }
    EngRootMotion cur_motion;
    const EngPose *cur = slot_update(m, &m->slots[m->cur], dt, &cur_motion);
    if (cur == NULL) {
        return false;
    }
    if (!m->has_from) {
        m->motion = cur_motion;
        return pose_copy(&m->pose, cur);
    }
    const EngPose *from = &m->snapshot;
    EngRootMotion from_motion;
    memset(&from_motion, 0, sizeof from_motion); /* source figée : plus de déplacement */
    if (!m->from_frozen) {
        from = slot_update(m, &m->slots[1U - m->cur], dt, &from_motion);
        if (from == NULL) {
            return false;
        }
    }
    m->elapsed += dt;
    if (m->elapsed >= m->fade) {
        m->has_from = false;
        m->motion = cur_motion;
        return pose_copy(&m->pose, cur);
    }
    const float w = eng_crossfade_curve(m->elapsed / m->fade);
    m->motion = eng_rootmotion_lerp(&from_motion, &cur_motion, w);
    return eng_pose_blend(&m->pose, from, cur, w);
}

const char *eng_animmachine_state_name(const EngAnimMachine *m)
{
    if (m == NULL || !m->ready) {
        return "";
    }
    return m->def.states[m->slots[m->cur].state].name;
}

bool eng_animmachine_in_transition(const EngAnimMachine *m)
{
    return m != NULL && m->ready && m->has_from;
}
