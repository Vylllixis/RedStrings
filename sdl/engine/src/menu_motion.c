/* Animation des menus : voir menu_motion.h. Aucune dépendance au rendu. */
#include "engine/menu_motion.h"

#include <math.h>
#include <string.h>

#define TWO_PI 6.2831853F
#define MAX_DT 0.05F
#define FAR_TIME 100.0F /* « il y a longtemps » : animation terminée */

static float clamp01(float t)
{
    return t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
}

float eng_ease_out_cubic(float t)
{
    const float u = 1.0F - clamp01(t);
    return 1.0F - (u * u * u);
}

float eng_ease_in_out_cubic(float t)
{
    t = clamp01(t);
    if (t < 0.5F) {
        return 4.0F * t * t * t;
    }
    const float u = (-2.0F * t) + 2.0F;
    return 1.0F - (u * u * u * 0.5F);
}

float eng_ease_out_back(float t)
{
    const float c1 = 1.70158F;
    const float c3 = c1 + 1.0F;
    const float u = clamp01(t) - 1.0F;
    return 1.0F + (c3 * u * u * u) + (c1 * u * u);
}

void eng_spring_step(EngSpring *s, float target, float hz, float damping, float dt)
{
    if (s == NULL || !(dt > 0.0F) || !(hz > 0.0F) || !isfinite(target)) {
        return;
    }
    const float w = TWO_PI * hz;
    const float k = w * w;
    const float c = 2.0F * damping * w;
    int steps = (int)ceilf(dt * 240.0F);
    steps = steps < 1 ? 1 : (steps > 48 ? 48 : steps);
    const float h = dt / (float)steps;
    for (int i = 0; i < steps; i++) {
        const float a = (k * (target - s->x)) - (c * s->v);
        s->v += a * h;
        s->x += s->v * h;
    }
}

float eng_stagger(float time, uint32_t index, float delay, float duration)
{
    if (!(duration > 0.0F)) {
        return time >= (float)index * delay ? 1.0F : 0.0F;
    }
    return clamp01((time - ((float)index * delay)) / duration);
}

EngMenuFamily eng_menu_family(EngMenuScreen screen)
{
    switch (screen) {
    case ENG_MENU_MAIN:
        return ENG_MENU_FAMILY_RAIL;
    case ENG_MENU_SAVES:
    case ENG_MENU_SLOT:
    case ENG_MENU_NAME:
    case ENG_MENU_DELETE:
        return ENG_MENU_FAMILY_CARDS;
    case ENG_MENU_PAUSE:
    case ENG_MENU_OPTIONS:
    case ENG_MENU_CONTROLS:
        return ENG_MENU_FAMILY_LIST;
    default:
        return ENG_MENU_FAMILY_NONE;
    }
}

static float cards_fit(float aspect)
{
    const float row = (3.0F * ENG_MOTION_CARD_W) + (2.0F * ENG_MOTION_CARD_GAP);
    const float fit = (aspect - 0.10F) / row;
    return fit < 0.35F ? 0.35F : (fit > 1.0F ? 1.0F : fit);
}

static int32_t focus_slot(const EngMenu *m)
{
    return m->slot < 0
               ? 0
               : (m->slot >= (int32_t)ENG_MENU_SLOTS ? (int32_t)ENG_MENU_SLOTS - 1 : m->slot);
}

void eng_menu_motion_card_target(const EngMenu *m, uint32_t index, float aspect, float fit,
                                 float out[2], float *scale, float *alpha)
{
    const float row_x = ((float)index - 1.0F) * (ENG_MOTION_CARD_W + ENG_MOTION_CARD_GAP) * fit;
    const float row_y = ENG_MOTION_THREAD_Y + ((0.04F + (ENG_MOTION_CARD_H * 0.5F)) * fit);
    out[0] = row_x;
    out[1] = row_y;
    *scale = 1.0F;
    *alpha = 1.0F;
    switch (m->screen) {
    case ENG_MENU_SAVES:
        if (m->selected == (int32_t)index) {
            *scale = 1.06F;
            out[1] += 0.008F; /* la carte choisie pèse sur le fil */
        } else {
            *alpha = 0.86F;
        }
        break;
    case ENG_MENU_SLOT:
    case ENG_MENU_NAME:
    case ENG_MENU_DELETE: {
        const int32_t focus = focus_slot(m);
        if ((int32_t)index == focus) { /* la carte visée vient à gauche, agrandie */
            out[0] = -0.29F * fit;
            out[1] = row_y + 0.012F;
            *scale = 1.14F;
        } else { /* les autres s'écartent hors de l'écran */
            out[0] = ((int32_t)index < focus ? -1.0F : 1.0F) * ((aspect * 0.5F) + 0.3F);
            *scale = 0.9F;
            *alpha = 0.0F;
        }
        break;
    }
    default: /* pas d'écran de cartes : elles sont remontées au-dessus de l'écran */
        out[1] = -0.95F;
        *alpha = 0.0F;
        break;
    }
}

float eng_menu_motion_scroll_target(float current, int32_t selected, uint32_t count,
                                    uint32_t visible)
{
    if (visible == 0U || count <= visible) {
        return 0.0F;
    }
    const float max_first = (float)(count - visible);
    float first = current;
    const float margin = visible >= 4U ? 1.0F : 0.0F;
    const float sel = (float)(selected < 0 ? 0 : selected);
    if (sel < first + margin) {
        first = sel - margin;
    } else if (sel > first + (float)visible - 1.0F - margin) {
        first = sel - (float)visible + 1.0F + margin;
    }
    return first < 0.0F ? 0.0F : (first > max_first ? max_first : first);
}

/* mise en page du titre et cadrage de la scène pour l'écran `m` */
static void scene_targets(const EngMenu *m, float aspect, float *title_y, float *title_w,
                          float cam[3], float *dim)
{
    const float wide = fminf(0.56F, aspect * 0.4F);
    cam[0] = 0.0F;
    cam[1] = 0.0F;
    cam[2] = 1.0F;
    *dim = 0.0F;
    *title_y = 0.27F;
    *title_w = wide;
    switch (eng_menu_family(m->screen)) {
    case ENG_MENU_FAMILY_CARDS:
        *title_y = 0.392F;
        *title_w = fminf(0.3F, aspect * 0.3F);
        cam[1] = -0.012F;
        cam[2] = m->screen == ENG_MENU_SAVES ? 1.05F : 1.08F;
        cam[0] = m->screen == ENG_MENU_SAVES ? 0.0F : -0.03F;
        *dim = m->screen == ENG_MENU_SAVES ? 0.78F : 0.9F;
        break;
    case ENG_MENU_FAMILY_LIST:
        *title_y = 0.405F;
        *title_w = fminf(0.26F, aspect * 0.28F);
        cam[0] = 0.035F;
        cam[2] = 1.03F;
        *dim = 0.92F;
        break;
    default:
        break;
    }
}

void eng_menu_motion_init(EngMenuMotion *a)
{
    if (a == NULL) {
        return;
    }
    memset(a, 0, sizeof *a);
    a->ghost_time = -1.0F;
    a->leave = FAR_TIME;
    a->pluck = FAR_TIME;
    a->since_select = FAR_TIME;
    a->fit = 1.0F;
    a->scroll_snap = true;
    a->cam_zoom.x = 1.0F;
    a->title_y.x = 0.255F; /* la place du titre sur l'écran de chargement */
    a->title_w.x = 0.56F;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        a->cards[i].born = FAR_TIME;
        a->cards[i].scale.x = 1.0F;
        a->cards[i].y.x = -0.95F;
    }
}

static void remember(EngMenuMotion *a, const EngMenu *m)
{
    a->seen_confirm = m->confirm_serial;
    a->seen_deny = m->deny_serial;
    a->seen_adjust = m->adjust_serial;
    a->seen_text = m->text_serial;
    a->seen_status = m->status_serial;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        a->seen_used[i] = m->slots[i].used;
        a->seen_slots[i] = m->slots[i];
    }
}

void eng_menu_motion_settle(EngMenuMotion *a, const EngMenu *m, float aspect)
{
    if (a == NULL || m == NULL || !(aspect > 0.0F)) {
        return;
    }
    a->started = true;
    a->screen = m->screen;
    a->from = ENG_MENU_CLOSED;
    a->leave = FAR_TIME;
    a->enter = FAR_TIME;
    a->selected = m->selected;
    a->since_select = FAR_TIME;
    a->pluck = FAR_TIME;
    a->cursor = (EngSpring){(float)m->selected, 0.0F};
    for (uint32_t i = 0; i < ENG_MENU_MAX_ITEMS; i++) {
        a->emphasis[i] = (int32_t)i == m->selected ? 1.0F : 0.0F;
    }
    a->open = eng_menu_is_open(m) ? 1.0F : 0.0F;
    a->confirm = 0.0F;
    a->deny = 0.0F;
    a->adjust = 0.0F;
    a->typed = 0.0F;
    a->status = m->status[0] != '\0' ? 1.0F : 0.0F;
    memcpy(a->status_text, m->status, sizeof a->status_text);
    a->status_text[sizeof a->status_text - 1U] = '\0';
    a->thread = eng_menu_family(m->screen) == ENG_MENU_FAMILY_CARDS ? 1.0F : 0.0F;
    a->fit = cards_fit(aspect);
    a->ghost_time = -1.0F;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        float at[2];
        float scale = 1.0F;
        float alpha = 1.0F;
        eng_menu_motion_card_target(m, i, aspect, a->fit, at, &scale, &alpha);
        a->cards[i].x = (EngSpring){at[0], 0.0F};
        a->cards[i].y = (EngSpring){at[1], 0.0F};
        a->cards[i].scale = (EngSpring){scale, 0.0F};
        a->cards[i].alpha = (EngSpring){alpha, 0.0F};
        a->cards[i].angle = (EngSpring){0.0F, 0.0F};
        a->cards[i].born = FAR_TIME;
    }
    float title_y = 0.0F;
    float title_w = 0.0F;
    float cam[3];
    float dim = 0.0F;
    scene_targets(m, aspect, &title_y, &title_w, cam, &dim);
    a->title_menu = eng_menu_is_open(m) ? 1.0F : a->title_menu;
    a->title_y = (EngSpring){title_y, 0.0F};
    a->title_w = (EngSpring){title_w, 0.0F};
    a->cam_x = (EngSpring){cam[0], 0.0F};
    a->cam_y = (EngSpring){cam[1], 0.0F};
    a->cam_zoom = (EngSpring){cam[2], 0.0F};
    a->dim = (EngSpring){dim, 0.0F};
    remember(a, m);
}

static float approach(float value, float target, float rate, float dt)
{
    return value + ((target - value) * (1.0F - expf(-rate * dt)));
}

static float decay(float value, float rate, float dt)
{
    const float v = value * expf(-rate * dt);
    return v < 0.001F ? 0.0F : v;
}

/* un écran vient de remplacer l'autre */
static void change_screen(EngMenuMotion *a, const EngMenu *m)
{
    a->from = a->screen;
    a->from_selected = a->selected;
    a->leave = 0.0F;
    a->screen = m->screen;
    a->enter = 0.0F;
    a->selected = m->selected;
    a->since_select = FAR_TIME;
    a->cursor = (EngSpring){(float)m->selected, 0.0F};
    a->scroll = (EngSpring){0.0F, 0.0F};
    a->scroll_first = 0.0F;
    a->scroll_snap = true;
    a->list_count = 0;
    a->list_visible = 0;
    memset(a->emphasis, 0, sizeof a->emphasis);
}

static void update_cards(EngMenuMotion *a, const EngMenu *m, float dt, float aspect)
{
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        EngMotionCard *c = &a->cards[i];
        if (m->slots[i].used != a->seen_used[i]) {
            if (!m->slots[i].used) { /* supprimée : son image tombe, la carte vide redescend */
                a->ghost = a->seen_slots[i];
                a->ghost_time = 0.0F;
                a->ghost_x = c->x.x;
                a->ghost_y = c->y.x;
                a->ghost_scale = c->scale.x;
                a->ghost_index = (int32_t)i;
            }
            c->born = 0.0F;
            c->y.x = -0.8F; /* la nouvelle carte descend le long du fil */
            c->y.v = 0.0F;
            c->alpha.x = 0.0F;
            c->angle.v += i == 1U ? 1.6F : -1.6F;
        }
        float at[2];
        float scale = 1.0F;
        float alpha = 1.0F;
        eng_menu_motion_card_target(m, i, aspect, a->fit, at, &scale, &alpha);
        const float before = c->x.x;
        eng_spring_step(&c->x, at[0], 2.6F, 0.82F, dt);
        eng_spring_step(&c->y, at[1], 2.4F + (0.25F * (float)i), 0.7F, dt);
        eng_spring_step(&c->scale, scale, 3.4F, 0.72F, dt);
        eng_spring_step(&c->alpha, alpha, 4.5F, 1.0F, dt);
        /* la carte est suspendue : elle part en arrière quand on la déplace, puis se balance */
        c->angle.v -= (c->x.x - before) * 4.5F;
        eng_spring_step(&c->angle, 0.0F, 1.15F, 0.3F, dt);
        c->angle.x = c->angle.x < -0.5F ? -0.5F : (c->angle.x > 0.5F ? 0.5F : c->angle.x);
        c->born += dt;
    }
    if (a->ghost_time >= 0.0F) {
        a->ghost_time += dt;
        if (a->ghost_time > ENG_MOTION_GHOST_TIME) {
            a->ghost_time = -1.0F;
        }
    }
}

void eng_menu_motion_update(EngMenuMotion *a, const EngMenu *m, float dt, float aspect)
{
    if (a == NULL || m == NULL || !(aspect > 0.0F)) {
        return;
    }
    if (!a->started) { /* première image : l'état de repos, mais l'entrée de l'écran se joue */
        const float loading_y = a->title_y.x;
        const float loading_w = a->title_w.x;
        const float title_menu = a->title_menu;
        eng_menu_motion_settle(a, m, aspect);
        a->enter = 0.0F;
        a->open = 0.0F;
        a->title_menu = title_menu;
        if (title_menu <
            0.5F) { /* on arrive de l'écran de chargement : le titre part de sa place */
            a->title_y.x = loading_y;
            a->title_w.x = fminf(loading_w, fminf(0.56F, aspect * 0.4F));
            a->cam_x.x = 0.0F;
            a->cam_y.x = 0.0F;
            a->cam_zoom.x = 1.0F;
            a->dim.x = 0.0F;
        }
        a->thread = 0.0F;
        memset(a->emphasis, 0, sizeof a->emphasis);
        for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
            a->cards[i].y.x = -0.95F;
            a->cards[i].alpha.x = 0.0F;
        }
    }
    dt = !(dt > 0.0F) ? 0.0F : (dt > MAX_DT ? MAX_DT : dt);
    a->clock += dt;
    a->enter += dt;
    a->leave += dt;
    a->pluck += dt;
    a->since_select += dt;
    a->fit = cards_fit(aspect);
    if (m->screen != a->screen) {
        change_screen(a, m);
    } else if (m->selected != a->selected) {
        if (m->screen == ENG_MENU_SAVES && m->selected >= 0 &&
            m->selected < (int32_t)ENG_MENU_SLOTS) { /* la carte choisie se balance */
            a->cards[m->selected].angle.v += m->selected > a->selected ? -1.1F : 1.1F;
        }
        a->selected = m->selected;
        a->since_select = 0.0F;
        a->pluck = 0.0F;
    }
    if (m->confirm_serial != a->seen_confirm) {
        a->confirm = 1.0F;
    }
    if (m->deny_serial != a->seen_deny) {
        a->deny = 1.0F;
    }
    if (m->adjust_serial != a->seen_adjust) {
        a->adjust = 1.0F;
        a->adjust_dir = m->adjust_dir;
    }
    if (m->text_serial != a->seen_text) {
        a->typed = 1.0F;
    }
    a->confirm = decay(a->confirm, 5.0F, dt);
    a->deny = decay(a->deny, 6.0F, dt);
    a->adjust = decay(a->adjust, 7.0F, dt);
    a->typed = decay(a->typed, 9.0F, dt);
    a->open = approach(a->open, eng_menu_is_open(m) ? 1.0F : 0.0F, 11.0F, dt);
    a->status = approach(a->status, m->status[0] != '\0' ? 1.0F : 0.0F, 9.0F, dt);
    if (m->status[0] != '\0') {
        memcpy(a->status_text, m->status, sizeof a->status_text);
        a->status_text[sizeof a->status_text - 1U] = '\0';
    }
    eng_spring_step(&a->cursor, (float)m->selected, 5.0F, 0.62F, dt);
    for (uint32_t i = 0; i < ENG_MENU_MAX_ITEMS; i++) {
        a->emphasis[i] =
            approach(a->emphasis[i], (int32_t)i == m->selected ? 1.0F : 0.0F, 14.0F, dt);
    }
    if (a->list_visible > 0U) { /* la liste défile pour garder la ligne choisie à l'écran */
        a->scroll_first = eng_menu_motion_scroll_target(a->scroll_first, m->selected, a->list_count,
                                                        a->list_visible);
        if (a->scroll_snap) {
            a->scroll = (EngSpring){a->scroll_first, 0.0F};
            a->scroll_snap = false;
        }
        eng_spring_step(&a->scroll, a->scroll_first, 3.2F, 1.0F, dt);
    }
    const bool cards = eng_menu_family(m->screen) == ENG_MENU_FAMILY_CARDS;
    a->thread += cards ? dt / 0.5F : -dt / 0.22F;
    a->thread = clamp01(a->thread);
    update_cards(a, m, dt, aspect);
    if (eng_menu_is_open(m)) {
        float title_y = 0.0F;
        float title_w = 0.0F;
        float cam[3];
        float dim = 0.0F;
        scene_targets(m, aspect, &title_y, &title_w, cam, &dim);
        a->title_menu = approach(a->title_menu, 1.0F, 5.0F, dt);
        eng_spring_step(&a->title_y, title_y, 1.7F, 0.9F, dt);
        eng_spring_step(&a->title_w, title_w, 1.7F, 0.9F, dt);
        eng_spring_step(&a->cam_x, cam[0], 0.9F, 1.0F, dt);
        eng_spring_step(&a->cam_y, cam[1], 0.9F, 1.0F, dt);
        eng_spring_step(&a->cam_zoom, cam[2], 0.9F, 1.0F, dt);
        eng_spring_step(&a->dim, dim, 2.2F, 1.0F, dt);
    }
    remember(a, m);
}
