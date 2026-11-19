#include "avatar_gallery.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define SPACING 1.5F
#define GROUND_Y (-0.5F) /* dessus du sol de la démonstration */

typedef enum Kind {
    K_IDLE,
    K_WALK,
    K_RUN,
    K_AIR,
    K_ATTACK,
    K_BLOCK,
    K_PARRY,
    K_HIT,
    K_DEATH,
    K_ROLL
} Kind;

typedef struct Entry {
    const char *label;
    const char *attack; /* K_ATTACK : nom dans attacks.txt */
    Kind kind;
    float roll_progress; /* K_ROLL */
} Entry;

static const Entry ENTRIES[] = {
    {"repos", NULL, K_IDLE, 0.0F},         {"marche", NULL, K_WALK, 0.0F},
    {"course", NULL, K_RUN, 0.0F},         {"en l'air", NULL, K_AIR, 0.0F},
    {"light1", "light1", K_ATTACK, 0.0F},  {"light2", "light2", K_ATTACK, 0.0F},
    {"light3", "light3", K_ATTACK, 0.0F},  {"heavy", "heavy", K_ATTACK, 0.0F},
    {"garde", NULL, K_BLOCK, 0.0F},        {"parade", NULL, K_PARRY, 0.0F},
    {"touché", NULL, K_HIT, 0.0F},         {"mort", NULL, K_DEATH, 0.0F},
    {"roulade 25 %", NULL, K_ROLL, 0.25F}, {"roulade 50 %", NULL, K_ROLL, 0.5F},
    {"roulade 75 %", NULL, K_ROLL, 0.75F}};
#define ENTRY_COUNT (sizeof ENTRIES / sizeof ENTRIES[0])

bool avatar_gallery_init(AvatarGallery *g, AvatarLibrary *lib, const char *attacks_text)
{
    if (g == NULL || lib == NULL || !lib->cpu_ready) {
        return false;
    }
    memset(g, 0, sizeof *g);
    char err[160];
    const bool have_attacks =
        attacks_text != NULL && eng_attackset_parse(attacks_text, &g->attacks, err, sizeof err);
    if (attacks_text != NULL && !have_attacks) {
        ENG_LOG_WARN("galerie : attaques illisibles (%s)", err);
    }
    for (uint32_t i = 0; i < ENTRY_COUNT && g->count < AVATAR_GALLERY_MAX; i++) {
        const Entry *e = &ENTRIES[i];
        EngActorDrive d = {.grounded = true, .state = ENG_CS_IDLE};
        switch (e->kind) {
        case K_WALK:
            d.ground_speed = 1.2F;
            d.state = ENG_CS_MOVE;
            break;
        case K_RUN:
            d.ground_speed = 4.5F;
            d.state = ENG_CS_MOVE;
            break;
        case K_AIR:
            d.grounded = false;
            break;
        case K_ATTACK: {
            const uint32_t a =
                have_attacks ? eng_attackset_find(&g->attacks, e->attack) : ENG_ATTACK_NONE;
            if (a == ENG_ATTACK_NONE) {
                continue;
            }
            const EngAttack *atk = &g->attacks.attacks[a];
            d.state = ENG_CS_ATTACK;
            d.attack_clip = avatar_clip(lib, atk->anim);
            d.attack_speed = atk->anim_speed;
            d.attack_offset = atk->anim_offset;
            d.attack_serial = 1;
            d.state_ticks = atk->startup + (atk->active / 2U); /* milieu des ticks actifs */
            break;
        }
        case K_BLOCK:
            d.state = ENG_CS_BLOCK;
            break;
        case K_PARRY:
            d.state = ENG_CS_PARRY;
            d.state_ticks = 6;
            break;
        case K_HIT:
            d.state = ENG_CS_HITSTUN;
            d.state_ticks = 8;
            break;
        case K_DEATH:
            d.state = ENG_CS_DEAD;
            d.state_ticks = 600;
            break;
        case K_ROLL:
            d.state = ENG_CS_DODGE;
            d.dodge_progress = e->roll_progress;
            d.dodge_dir[1] = 1.0F; /* vers l'avant du personnage */
            break;
        case K_IDLE:
        default:
            break;
        }
        Avatar *av = &g->avatars[g->count];
        if (!avatar_init(av, lib, AVATAR_KNIGHT, AVATAR_SWORD, AVATAR_SHIELD, 1.0F)) {
            return false;
        }
        g->drives[g->count] = d;
        g->labels[g->count] = e->label;
        g->count++;
    }
    const float first = -0.5F * SPACING * (float)(g->count - 1U);
    for (uint32_t i = 0; i < g->count; i++) {
        g->x[i] = first + (SPACING * (float)i);
    }
    g->ready = g->count > 0U;
    ENG_LOG_INFO("galerie : %u états, de gauche à droite (vus de face) :", g->count);
    for (uint32_t i = 0; i < g->count; i++) {
        ENG_LOG_INFO("  %2u. %s", (unsigned)(i + 1U), g->labels[g->count - 1U - i]);
    }
    return g->ready;
}

void avatar_gallery_update(AvatarGallery *g, const AvatarLibrary *lib, float dt)
{
    if (g == NULL || !g->ready) {
        return;
    }
    g->time += dt;
    for (uint32_t i = 0; i < g->count; i++) {
        const float feet[3] = {g->x[i], GROUND_Y, 0.0F};
        avatar_update(&g->avatars[i], lib, &g->drives[i], dt, feet, 0.0F);
    }
}

void avatar_gallery_draw(const AvatarGallery *g, const AvatarLibrary *lib, EngRenderQueue *queue,
                         mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                         bool shadow_pass)
{
    if (g == NULL || !g->ready) {
        return;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        if (shadow_pass) {
            avatar_draw_shadow(&g->avatars[i], lib, queue, view_proj);
        } else {
            avatar_draw(&g->avatars[i], lib, queue, view_proj, light, shadow);
        }
    }
}

void avatar_gallery_camera(const AvatarGallery *g, EngCamera *camera)
{
    const float half = g != NULL && g->count > 0U ? 0.5F * SPACING * (float)g->count : 6.0F;
    /* devant la rangée (les personnages regardent vers -Z), assez loin pour tout cadrer */
    const float dist = (half / tanf(0.5F * camera->fov_y * 1.6F)) + 1.0F;
    const float pos[3] = {0.0F, GROUND_Y + 1.1F, -dist};
    const float fov = camera->fov_y;
    eng_camera_init(camera, pos, 3.14159265F, -0.05F);
    camera->fov_y = fov;
}
