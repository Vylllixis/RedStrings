#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "avatar.h"
#include "engine/attack_data.h"
#include "engine/camera.h"

/* Galerie des états d'animation (M4, option --avatar-gallery) : une rangée de chevaliers, chacun
 * dans un état du jeu (repos, marche, course, saut, attaques AU TICK D'IMPACT, garde, parade,
 * touché, mort, roulade à 25 / 50 / 75 %), devant une caméra fixe. Sert de preuve visuelle
 * (« captures de chaque état ») et à régler les clips. Les attaques sont lues dans
 * assets/combat/attacks.txt (clip, vitesse, décalage, frame data). */

#define AVATAR_GALLERY_MAX 16U

typedef struct AvatarGallery {
    bool ready;
    uint32_t count;
    Avatar avatars[AVATAR_GALLERY_MAX];
    EngActorDrive drives[AVATAR_GALLERY_MAX];
    float x[AVATAR_GALLERY_MAX];
    const char *labels[AVATAR_GALLERY_MAX];
    EngAttackSet attacks;
    float time;
} AvatarGallery;

/* `attacks_text` : contenu de attacks.txt (NULL : attaques absentes de la galerie). */
bool avatar_gallery_init(AvatarGallery *g, AvatarLibrary *lib, const char *attacks_text);
void avatar_gallery_update(AvatarGallery *g, const AvatarLibrary *lib, float dt);
void avatar_gallery_draw(const AvatarGallery *g, const AvatarLibrary *lib, EngRenderQueue *queue,
                         mat4 view_proj, const EngLighting *light, const EngModelShadow *shadow,
                         bool shadow_pass);
/* Caméra qui cadre toute la rangée (position et orientation ; le champ de vision est gardé). */
void avatar_gallery_camera(const AvatarGallery *g, EngCamera *camera);
