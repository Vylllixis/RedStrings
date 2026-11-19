#include "engine/physics.h"

#include <math.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "engine/assert.h"
#include "engine/log.h"

/* Matrice d'interaction (voir physics.h) : 0 aucune, 1 solide, 2 détection. Symétrique. */
static const uint8_t INTERACTION_MATRIX[ENG_LAYER_COUNT][ENG_LAYER_COUNT] = {
    /*            STATIC DYNAMIC CHARACTER HITBOX TRIGGER */
    /* STATIC */ {0, 1, 1, 0, 0},
    /* DYNAMIC */ {1, 1, 1, 2, 2},
    /* CHARACTER */ {1, 1, 1, 2, 2},
    /* HITBOX */ {0, 2, 2, 0, 0},
    /* TRIGGER */ {0, 2, 2, 0, 0},
};

EngLayerInteraction eng_physics_layer_interaction(EngPhysicsLayer a, EngPhysicsLayer b)
{
    if ((unsigned)a >= (unsigned)ENG_LAYER_COUNT || (unsigned)b >= (unsigned)ENG_LAYER_COUNT) {
        return ENG_INTERACT_NONE;
    }
    return (EngLayerInteraction)INTERACTION_MATRIX[a][b];
}

bool eng_physics_layer_is_sensor(EngPhysicsLayer layer)
{
    return layer == ENG_LAYER_HITBOX || layer == ENG_LAYER_TRIGGER;
}

const char *eng_physics_layer_name(EngPhysicsLayer layer)
{
    static const char *const names[ENG_LAYER_COUNT] = {"static", "dynamic", "character", "hitbox",
                                                       "trigger"};
    return (unsigned)layer < (unsigned)ENG_LAYER_COUNT ? names[layer] : "?";
}

EngPhysicsPose eng_physics_pose_identity(void)
{
    return (EngPhysicsPose){
        .position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F, 1.0F}, .scale = 1.0F};
}

EngDebugColor eng_physics_layer_color(EngPhysicsLayer layer)
{
    switch (layer) {
    case ENG_LAYER_STATIC:
        return ENG_DEBUG_GRAY;
    case ENG_LAYER_DYNAMIC:
        return ENG_DEBUG_GREEN;
    case ENG_LAYER_CHARACTER:
        return ENG_DEBUG_CYAN;
    case ENG_LAYER_HITBOX:
        return ENG_DEBUG_RED;
    case ENG_LAYER_TRIGGER:
        return ENG_DEBUG_YELLOW;
    case ENG_LAYER_COUNT:
    default:
        return ENG_DEBUG_MAGENTA;
    }
}

#ifndef OW_PHYSICS

bool eng_physics_available(void)
{
    return false;
}

EngPhysics *eng_physics_create(const EngPhysicsDesc *desc)
{
    (void)desc;
    ENG_LOG_ERROR("physique indisponible : moteur compilé sans Jolt (OW_PHYSICS=OFF)");
    return NULL;
}
void eng_physics_destroy(EngPhysics *world)
{
    (void)world;
}
bool eng_physics_step(EngPhysics *world, float dt)
{
    (void)world;
    (void)dt;
    return false;
}
EngBody eng_physics_add_sphere(EngPhysics *world, float radius, const float position[3],
                               EngPhysicsLayer layer)
{
    (void)world;
    (void)radius;
    (void)position;
    (void)layer;
    return ENG_BODY_INVALID;
}
EngBody eng_physics_add_box(EngPhysics *world, const float half_extents[3], const float position[3],
                            EngPhysicsLayer layer)
{
    (void)world;
    (void)half_extents;
    (void)position;
    (void)layer;
    return ENG_BODY_INVALID;
}
EngBody eng_physics_add_capsule(EngPhysics *world, float half_height, float radius,
                                const float position[3], EngPhysicsLayer layer)
{
    (void)world;
    (void)half_height;
    (void)radius;
    (void)position;
    (void)layer;
    return ENG_BODY_INVALID;
}
void eng_physics_remove_body(EngPhysics *world, EngBody body)
{
    (void)world;
    (void)body;
}
EngPhysicsLayer eng_physics_body_layer(const EngPhysics *world, EngBody body)
{
    (void)world;
    (void)body;
    return ENG_LAYER_COUNT;
}
bool eng_physics_move_kinematic(EngPhysics *world, EngBody body, const float target[3], float dt)
{
    (void)world;
    (void)body;
    (void)target;
    (void)dt;
    return false;
}
uint32_t eng_physics_overlaps(const EngPhysics *world, EngBody body, EngBody *out, uint32_t max)
{
    (void)world;
    (void)body;
    (void)out;
    (void)max;
    return 0;
}
bool eng_physics_body_position(const EngPhysics *world, EngBody body, float out[3])
{
    (void)world;
    (void)body;
    (void)out;
    return false;
}
bool eng_physics_body_rotation(const EngPhysics *world, EngBody body, float out[4])
{
    (void)world;
    (void)body;
    (void)out;
    return false;
}
bool eng_physics_body_velocity(const EngPhysics *world, EngBody body, float out[3])
{
    (void)world;
    (void)body;
    (void)out;
    return false;
}
bool eng_physics_set_body_velocity(EngPhysics *world, EngBody body, const float velocity[3])
{
    (void)world;
    (void)body;
    (void)velocity;
    return false;
}
bool eng_physics_body_is_active(const EngPhysics *world, EngBody body)
{
    (void)world;
    (void)body;
    return false;
}
EngPhysicsShape *eng_physics_shape_heightfield(EngPhysics *world, const struct EngHeightmap *map)
{
    (void)world;
    (void)map;
    return NULL;
}
EngPhysicsShape *eng_physics_shape_mesh(EngPhysics *world, const float *positions,
                                        size_t stride_bytes, uint32_t vertex_count,
                                        const uint32_t *indices, uint32_t index_count)
{
    (void)world;
    (void)positions;
    (void)stride_bytes;
    (void)vertex_count;
    (void)indices;
    (void)index_count;
    return NULL;
}
EngPhysicsShape *eng_physics_shape_model(EngPhysics *world, const struct EngModel *model)
{
    (void)world;
    (void)model;
    return NULL;
}
void eng_physics_shape_destroy(EngPhysicsShape *shape)
{
    (void)shape;
}
EngBody eng_physics_add_static_shape(EngPhysics *world, const EngPhysicsShape *shape,
                                     const EngPhysicsPose *pose)
{
    (void)world;
    (void)shape;
    (void)pose;
    return ENG_BODY_INVALID;
}
bool eng_physics_raycast(const EngPhysics *world, const float origin[3], const float direction[3],
                         float max_distance, uint32_t layer_mask, EngRayHit *hit)
{
    (void)world;
    (void)origin;
    (void)direction;
    (void)max_distance;
    (void)layer_mask;
    (void)hit;
    return false;
}
bool eng_physics_spherecast(const EngPhysics *world, const float origin[3],
                            const float direction[3], float max_distance, float radius,
                            uint32_t layer_mask, EngRayHit *hit)
{
    (void)world;
    (void)origin;
    (void)direction;
    (void)max_distance;
    (void)radius;
    (void)layer_mask;
    (void)hit;
    return false;
}
uint32_t eng_physics_body_count(const EngPhysics *world)
{
    (void)world;
    return 0;
}
uint32_t eng_physics_debug_draw(const EngPhysics *world, EngDebugLines *lines,
                                const EngPhysicsDebugDraw *options)
{
    (void)world;
    (void)lines;
    (void)options;
    return 0;
}
uint32_t eng_physics_advance(EngPhysics *world, EngLoop *loop, uint64_t frame_delta_counter)
{
    (void)world;
    (void)loop;
    (void)frame_delta_counter;
    return 0;
}
uint64_t eng_physics_step_count(const EngPhysics *world)
{
    (void)world;
    return 0;
}
bool eng_physics_body_interpolated(const EngPhysics *world, EngBody body, double alpha,
                                   float position[3], float rotation[4])
{
    (void)world;
    (void)body;
    (void)alpha;
    (void)position;
    (void)rotation;
    return false;
}

EngCharacter *eng_character_create(EngPhysics *world, const EngCharacterDesc *desc)
{
    (void)world;
    (void)desc;
    return NULL;
}
void eng_character_destroy(EngCharacter *character)
{
    (void)character;
}
bool eng_character_update(EngCharacter *character, float dt, const float move[3])
{
    (void)character;
    (void)dt;
    (void)move;
    return false;
}
bool eng_character_jump(EngCharacter *character, float speed)
{
    (void)character;
    (void)speed;
    return false;
}
bool eng_character_launch(EngCharacter *character, float speed)
{
    (void)character;
    (void)speed;
    return false;
}
bool eng_character_cut_jump(EngCharacter *character, float factor)
{
    (void)character;
    (void)factor;
    return false;
}
bool eng_character_set_gravity_scale(EngCharacter *character, float rise, float fall)
{
    (void)character;
    (void)rise;
    (void)fall;
    return false;
}
bool eng_character_teleport(EngCharacter *character, const float feet[3])
{
    (void)character;
    (void)feet;
    return false;
}
bool eng_character_position(const EngCharacter *character, float feet[3])
{
    (void)character;
    (void)feet;
    return false;
}
bool eng_character_interpolated(const EngCharacter *character, double alpha, float feet[3])
{
    (void)character;
    (void)alpha;
    (void)feet;
    return false;
}
bool eng_character_velocity(const EngCharacter *character, float out[3])
{
    (void)character;
    (void)out;
    return false;
}
EngCharacterGround eng_character_ground(const EngCharacter *character)
{
    (void)character;
    return ENG_GROUND_AIR;
}
bool eng_character_ground_normal(const EngCharacter *character, float normal[3])
{
    (void)character;
    (void)normal;
    return false;
}
EngBody eng_character_body(const EngCharacter *character)
{
    (void)character;
    return ENG_BODY_INVALID;
}

#else /* OW_PHYSICS */

#include <stdlib.h>

#include "engine/heightmap.h"
#include "engine/model.h"
#include "joltc.h"

#define DEFAULT_MAX_BODIES 10240U
#define MAX_STEP_SECONDS 0.1F

struct EngPhysics {
    JPH_PhysicsSystem *system;
    JPH_JobSystem *jobs;
    JPH_BroadPhaseLayerInterface *bp_interface;
    JPH_ObjectLayerPairFilter *pair_filter;
    JPH_ObjectVsBroadPhaseLayerFilter *vs_bp_filter;
    float *prev;         /* par emplacement : position (3) + rotation (4) avant le dernier pas */
    uint8_t *has_prev;   /* par emplacement : `prev` est valide (le corps a vécu un pas) */
    uint64_t steps;      /* pas exécutés avec succès */
    uint32_t characters; /* personnages vivants (doivent être détruits avant le monde) */
    uint32_t next_character_id;
    JPH_BodyID *ids; /* tampon de eng_physics_debug_draw (max_bodies identifiants) */
    uint8_t *layers; /* par emplacement de corps : couche + 1 ; 0 = emplacement libre */
    uint32_t max_bodies;
};

/* Indice d'un corps dans les tableaux du monde : les 23 bits bas de l'identifiant de Jolt (les bits
 * hauts sont un numéro de séquence qui distingue deux corps successifs du même emplacement). */
#define BODY_SLOT_MASK 0x007FFFFFU

/* Jolt a un état global (allocateur, fabrique de types) : initialisé par le premier monde, libéré
 * avec le dernier. */
static int g_users;

static void trace_handler(const char *message)
{
    ENG_LOG_DEBUG("jolt : %s", message);
}

/* Assertions de Jolt (builds avec assertions). Jolt les arrête par un point d'arrêt ; une seule
 * est un cas d'attente prévu : « No jobs available! » dans JobSystemThreadPool::AllocateJob, dont
 * la boucle dort 100 µs puis réessaie jusqu'à ce qu'un job se libère. Elle se déclenche quand la
 * machine est saturée (plusieurs processus de test en parallèle) et ne signale aucune erreur ; la
 * traiter en arrêt rendait engine.character instable sous charge. Toute autre assertion reste
 * fatale (valeur de retour true = point d'arrêt). */
static bool assert_handler(const char *expression, const char *message, const char *file,
                           uint32_t line)
{
    if (message != NULL && strcmp(message, "No jobs available!") == 0) {
        return false;
    }
    ENG_LOG_ERROR("jolt : assertion %s (%s) %s:%u", expression != NULL ? expression : "?",
                  message != NULL ? message : "", file != NULL ? file : "?", (unsigned)line);
    return true;
}

bool eng_physics_available(void)
{
    return true;
}

static JPH_Vec3 to_jph(const float v[3])
{
    return (JPH_Vec3){v[0], v[1], v[2]};
}

static bool finite3(const float v[3])
{
    return v != NULL && isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

static bool layer_filter_should_collide(void *user_data, JPH_ObjectLayer layer);

/* joltc garde le POINTEUR vers cette table (pas une copie) : elle doit vivre aussi longtemps que le
 * processus. */
static const JPH_ObjectLayerFilter_Procs LAYER_FILTER_PROCS = {.ShouldCollide =
                                                                   layer_filter_should_collide};

EngPhysics *eng_physics_create(const EngPhysicsDesc *desc)
{
    EngPhysicsDesc d = {0};
    if (desc != NULL) {
        d = *desc;
    }
    if (d.max_bodies == 0) {
        d.max_bodies = DEFAULT_MAX_BODIES;
    }
    if (d.gravity[0] == 0.0F && d.gravity[1] == 0.0F && d.gravity[2] == 0.0F) {
        d.gravity[1] = -9.81F;
    }
    if (!finite3(d.gravity)) {
        ENG_LOG_ERROR("physique : gravité invalide");
        return NULL;
    }
    if (g_users == 0) {
        if (!JPH_Init()) {
            ENG_LOG_ERROR("physique : JPH_Init a échoué");
            return NULL;
        }
        JPH_SetTraceHandler(trace_handler);
        JPH_SetAssertFailureHandler(assert_handler);
        JPH_ObjectLayerFilter_SetProcs(&LAYER_FILTER_PROCS);
    }
    g_users++;

    EngPhysics *w = SDL_calloc(1, sizeof(*w));
    if (w == NULL) {
        g_users--;
        if (g_users == 0) {
            JPH_Shutdown();
        }
        return NULL;
    }
    w->max_bodies = d.max_bodies;
    w->layers = SDL_calloc(d.max_bodies, sizeof(uint8_t));
    w->ids = SDL_calloc(d.max_bodies, sizeof(JPH_BodyID));
    w->prev = SDL_calloc((size_t)d.max_bodies * 7U, sizeof(float));
    w->has_prev = SDL_calloc(d.max_bodies, sizeof(uint8_t));
    if (w->layers == NULL || w->ids == NULL || w->prev == NULL || w->has_prev == NULL) {
        ENG_LOG_ERROR("physique : mémoire insuffisante (%u corps)", (unsigned)d.max_bodies);
        eng_physics_destroy(w);
        return NULL;
    }

    JobSystemThreadPoolConfig jc = {0};
    /* joltc : numThreads <= 0 signifie « automatique » ; zéro fil de travail n'existe pas côté C,
     * on demande donc 1 fil pour worker_threads == 0 (le fil appelant participe aussi). */
    jc.numThreads = d.worker_threads > 0 ? d.worker_threads : (d.worker_threads == 0 ? 1 : -1);
    w->jobs = JPH_JobSystemThreadPool_Create(&jc);

    const uint32_t layers = (uint32_t)ENG_LAYER_COUNT;
    w->pair_filter = JPH_ObjectLayerPairFilterTable_Create(layers);
    /* Seules les paires SOLIDES sont déclarées à Jolt : les capteurs (HITBOX, TRIGGER) n'y ont
     * aucune paire, donc aucun contact ni aucune force avec quoi que ce soit. */
    for (uint32_t a = 0; a < layers; a++) {
        for (uint32_t b = a; b < layers; b++) {
            if (eng_physics_layer_interaction((EngPhysicsLayer)a, (EngPhysicsLayer)b) ==
                ENG_INTERACT_SOLID) {
                JPH_ObjectLayerPairFilterTable_EnableCollision(w->pair_filter, a, b);
                JPH_ObjectLayerPairFilterTable_EnableCollision(w->pair_filter, b, a);
            }
        }
    }
    /* une couche large par couche d'objets (1 pour 1) */
    w->bp_interface = JPH_BroadPhaseLayerInterfaceTable_Create(layers, layers);
    for (uint32_t i = 0; i < layers; i++) {
        JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(w->bp_interface, i,
                                                                     (JPH_BroadPhaseLayer)i);
    }
    w->vs_bp_filter = JPH_ObjectVsBroadPhaseLayerFilterTable_Create(w->bp_interface, layers,
                                                                    w->pair_filter, layers);

    JPH_PhysicsSystemSettings ss;
    memset(&ss, 0, sizeof(ss));
    ss.maxBodies = d.max_bodies;
    ss.numBodyMutexes = 0;
    ss.maxBodyPairs = d.max_bodies * 4U;
    ss.maxContactConstraints = d.max_bodies;
    ss.broadPhaseLayerInterface = w->bp_interface;
    ss.objectLayerPairFilter = w->pair_filter;
    ss.objectVsBroadPhaseLayerFilter = w->vs_bp_filter;
    w->system = JPH_PhysicsSystem_Create(&ss);
    if (w->system == NULL) {
        ENG_LOG_ERROR("physique : création du système Jolt impossible");
        eng_physics_destroy(w);
        return NULL;
    }
    const JPH_Vec3 g = to_jph(d.gravity);
    JPH_PhysicsSystem_SetGravity(w->system, &g);
    return w;
}

void eng_physics_destroy(EngPhysics *world)
{
    if (world == NULL) {
        return;
    }
    if (world->characters != 0U) {
        ENG_LOG_ERROR("physique : %u personnage(s) encore vivant(s) à la destruction du monde "
                      "(à détruire avant le monde)",
                      (unsigned)world->characters);
    }
    if (world->system != NULL) {
        JPH_PhysicsSystem_Destroy(world->system);
    }
    if (world->jobs != NULL) {
        JPH_JobSystem_Destroy(world->jobs);
    }
    SDL_free(world->has_prev);
    SDL_free(world->prev);
    SDL_free(world->ids);
    SDL_free(world->layers);
    SDL_free(world);
    g_users--;
    if (g_users == 0) {
        JPH_Shutdown();
    }
}

/* Mémorise la position et la rotation de chaque corps simulé (hors décor) avant un pas : le rendu
 * interpole entre cet état et le suivant. */
static void snapshot_bodies(EngPhysics *world)
{
    const uint32_t count = JPH_PhysicsSystem_GetNumBodies(world->system);
    JPH_PhysicsSystem_GetBodies(world->system, world->ids, count);
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    for (uint32_t n = 0; n < count; n++) {
        const JPH_BodyID id = world->ids[n];
        const uint32_t slot = id & BODY_SLOT_MASK;
        if (slot >= world->max_bodies || world->layers[slot] == 0U ||
            world->layers[slot] - 1U == (uint32_t)ENG_LAYER_STATIC) {
            continue;
        }
        JPH_RVec3 pos;
        JPH_Quat rot;
        JPH_BodyInterface_GetPositionAndRotation(bi, id, &pos, &rot);
        float *p = world->prev + ((size_t)slot * 7U);
        p[0] = pos.x;
        p[1] = pos.y;
        p[2] = pos.z;
        p[3] = rot.x;
        p[4] = rot.y;
        p[5] = rot.z;
        p[6] = rot.w;
        world->has_prev[slot] = 1U;
    }
}

bool eng_physics_step(EngPhysics *world, float dt)
{
    if (world == NULL || !(dt > 0.0F) || dt > MAX_STEP_SECONDS) {
        return false;
    }
    snapshot_bodies(world);
    const JPH_PhysicsUpdateError err = JPH_PhysicsSystem_Update(world->system, dt, 1, world->jobs);
    if (err != JPH_PhysicsUpdateError_None) {
        ENG_LOG_WARN("physique : erreur de mise à jour Jolt (0x%x), corps ou contacts en trop ?",
                     (unsigned)err);
        return false;
    }
    world->steps++;
    return true;
}

uint32_t eng_physics_advance(EngPhysics *world, EngLoop *loop, uint64_t frame_delta_counter)
{
    if (world == NULL || loop == NULL || !(loop->tick_seconds > 0.0) ||
        loop->tick_seconds > (double)MAX_STEP_SECONDS) {
        return 0;
    }
    const uint32_t due = eng_loop_advance(loop, frame_delta_counter);
    uint32_t done = 0;
    while (done < due && eng_physics_step(world, (float)loop->tick_seconds)) {
        done++;
    }
    return done;
}

uint64_t eng_physics_step_count(const EngPhysics *world)
{
    return world != NULL ? world->steps : 0U;
}

/* `release` : vrai si `shape` est une référence qui nous appartient (forme créée pour ce corps),
 * faux si elle appartient à un EngPhysicsShape partagé. */
static EngBody add_shape(EngPhysics *world, JPH_Shape *shape, const JPH_Vec3 *pose_position,
                         const JPH_Quat *rotation, EngPhysicsLayer layer, bool release)
{
    const JPH_Vec3 pos = *pose_position;
    const bool sensor = eng_physics_layer_is_sensor(layer);
    JPH_MotionType motion = JPH_MotionType_Dynamic;
    if (layer == ENG_LAYER_STATIC) {
        motion = JPH_MotionType_Static;
    } else if (sensor) {
        motion = JPH_MotionType_Kinematic;
    }
    JPH_BodyCreationSettings *bs =
        JPH_BodyCreationSettings_Create3(shape, &pos, rotation, motion, (JPH_ObjectLayer)layer);
    EngBody id = ENG_BODY_INVALID;
    if (bs != NULL) {
        JPH_BodyCreationSettings_SetIsSensor(bs, sensor);
        if (layer == ENG_LAYER_CHARACTER) { /* un personnage reste debout : pas de rotation */
            JPH_AllowedDOFs dofs =
                JPH_AllowedDOFs_TranslationX; /* bits combinés, pas un énumérateur */
            dofs |= JPH_AllowedDOFs_TranslationY;
            dofs |= JPH_AllowedDOFs_TranslationZ;
            JPH_BodyCreationSettings_SetAllowedDOFs(bs, dofs);
        }
        JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
        const JPH_BodyID bid = JPH_BodyInterface_CreateAndAddBody(
            bi, bs,
            motion == JPH_MotionType_Dynamic ? JPH_Activation_Activate
                                             : JPH_Activation_DontActivate);
        JPH_BodyCreationSettings_Destroy(bs);
        if (bid != ENG_BODY_INVALID) {
            if ((bid & BODY_SLOT_MASK) < world->max_bodies) {
                world->layers[bid & BODY_SLOT_MASK] = (uint8_t)(layer + 1);
                world->has_prev[bid & BODY_SLOT_MASK] = 0U;
                id = bid;
            } else { /* ne devrait pas arriver : emplacement hors tableau */
                JPH_BodyInterface_RemoveAndDestroyBody(bi, bid);
            }
        }
    }
    if (release) {
        JPH_Shape_Destroy(shape); /* le corps garde sa propre référence */
    }
    if (id == ENG_BODY_INVALID) {
        ENG_LOG_WARN("physique : corps refusé (monde plein ? %u corps max)",
                     (unsigned)world->max_bodies);
    }
    return id;
}

EngBody eng_physics_add_sphere(EngPhysics *world, float radius, const float position[3],
                               EngPhysicsLayer layer)
{
    if (world == NULL || !(radius > 0.0F) || !isfinite(radius) || !finite3(position) ||
        (unsigned)layer >= (unsigned)ENG_LAYER_COUNT) {
        return ENG_BODY_INVALID;
    }
    const JPH_Vec3 pos = to_jph(position);
    return add_shape(world, (JPH_Shape *)JPH_SphereShape_Create(radius), &pos, NULL, layer, true);
}

EngBody eng_physics_add_box(EngPhysics *world, const float half_extents[3], const float position[3],
                            EngPhysicsLayer layer)
{
    if (world == NULL || !finite3(half_extents) || !finite3(position) ||
        !(half_extents[0] > 0.0F) || !(half_extents[1] > 0.0F) || !(half_extents[2] > 0.0F) ||
        (unsigned)layer >= (unsigned)ENG_LAYER_COUNT) {
        return ENG_BODY_INVALID;
    }
    const JPH_Vec3 he = to_jph(half_extents);
    /* rayon d'arrondi de Jolt, borné au plus petit demi-côté */
    const float convex = fminf(JPH_DEFAULT_CONVEX_RADIUS,
                               fminf(half_extents[0], fminf(half_extents[1], half_extents[2])));
    const JPH_Vec3 pos = to_jph(position);
    return add_shape(world, (JPH_Shape *)JPH_BoxShape_Create(&he, convex), &pos, NULL, layer, true);
}

static bool known(const EngPhysics *world, EngBody body)
{
    if (world == NULL || body == ENG_BODY_INVALID) {
        return false;
    }
    if ((body & BODY_SLOT_MASK) >= world->max_bodies || world->layers[body & BODY_SLOT_MASK] == 0) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    return JPH_BodyInterface_IsAdded(bi, body);
}

void eng_physics_remove_body(EngPhysics *world, EngBody body)
{
    if (!known(world, body)) {
        return;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_BodyInterface_RemoveAndDestroyBody(bi, body);
    world->layers[body & BODY_SLOT_MASK] = 0;
    world->has_prev[body & BODY_SLOT_MASK] = 0U;
}

EngBody eng_physics_add_capsule(EngPhysics *world, float half_height, float radius,
                                const float position[3], EngPhysicsLayer layer)
{
    if (world == NULL || !(half_height > 0.0F) || !isfinite(half_height) || !(radius > 0.0F) ||
        !isfinite(radius) || !finite3(position) || (unsigned)layer >= (unsigned)ENG_LAYER_COUNT) {
        return ENG_BODY_INVALID;
    }
    const JPH_Vec3 pos = to_jph(position);
    return add_shape(world, (JPH_Shape *)JPH_CapsuleShape_Create(half_height, radius), &pos, NULL,
                     layer, true);
}

EngPhysicsLayer eng_physics_body_layer(const EngPhysics *world, EngBody body)
{
    if (!known(world, body)) {
        return ENG_LAYER_COUNT;
    }
    return (EngPhysicsLayer)(world->layers[body & BODY_SLOT_MASK] - 1U);
}

bool eng_physics_move_kinematic(EngPhysics *world, EngBody body, const float target[3], float dt)
{
    if (!known(world, body) || !finite3(target) || !(dt > 0.0F) || !isfinite(dt) ||
        !eng_physics_layer_is_sensor(eng_physics_body_layer(world, body))) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_RVec3 pos = to_jph(target);
    JPH_Quat rot;
    JPH_BodyInterface_GetRotation(bi, body, &rot);
    JPH_BodyInterface_MoveKinematic(bi, body, &pos, &rot, dt);
    return true;
}

typedef struct OverlapCollector {
    const EngPhysics *world;
    EngBody self;
    EngPhysicsLayer self_layer;
    uint32_t count;
    EngBody ids[ENG_PHYSICS_MAX_OVERLAPS];
} OverlapCollector;

static void overlap_callback(void *context, const JPH_CollideShapeResult *result)
{
    OverlapCollector *c = context;
    const EngBody other = result->bodyID2;
    if (other == c->self || (other & BODY_SLOT_MASK) >= c->world->max_bodies) {
        return;
    }
    const uint8_t slot = c->world->layers[other & BODY_SLOT_MASK];
    if (slot == 0 || eng_physics_layer_interaction(c->self_layer, (EngPhysicsLayer)(slot - 1U)) ==
                         ENG_INTERACT_NONE) {
        return;
    }
    for (uint32_t i = 0; i < c->count;
         i++) { /* un corps à plusieurs sous-formes : une seule fois */
        if (c->ids[i] == other) {
            return;
        }
    }
    if (c->count < ENG_PHYSICS_MAX_OVERLAPS) {
        c->ids[c->count++] = other;
    }
}

static int compare_ids(const void *a, const void *b)
{
    const EngBody x = *(const EngBody *)a;
    const EngBody y = *(const EngBody *)b;
    return (x > y) - (x < y);
}

uint32_t eng_physics_overlaps(const EngPhysics *world, EngBody body, EngBody *out, uint32_t max)
{
    if (!known(world, body)) {
        return 0;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    OverlapCollector c = {
        .world = world, .self = body, .self_layer = eng_physics_body_layer(world, body)};
    JPH_RVec3 pos;
    JPH_Quat rot;
    JPH_BodyInterface_GetCenterOfMassPosition(bi, body, &pos);
    JPH_BodyInterface_GetRotation(bi, body, &rot);
    JPH_RMat4 transform;
    JPH_Mat4_RotationTranslation(&transform, &rot, &pos);
    JPH_CollideShapeSettings settings;
    JPH_CollideShapeSettings_Init(&settings);
    const JPH_Vec3 scale = {1.0F, 1.0F, 1.0F};
    JPH_RVec3 base_offset = {0.0F, 0.0F, 0.0F};
    JPH_NarrowPhaseQuery_CollideShape2(JPH_PhysicsSystem_GetNarrowPhaseQuery(world->system),
                                       JPH_BodyInterface_GetShape(bi, body), &scale, &transform,
                                       &settings, &base_offset, JPH_CollisionCollectorType_AllHit,
                                       overlap_callback, &c, NULL, NULL, NULL, NULL);
    qsort(c.ids, c.count, sizeof(c.ids[0]), compare_ids);
    for (uint32_t i = 0; i < c.count && i < max && out != NULL; i++) {
        out[i] = c.ids[i];
    }
    return c.count;
}

/* ── Formes du monde ── */

struct EngPhysicsShape {
    JPH_Shape *shape;
    bool scalable; /* vrai pour un maillage, faux pour un champ de hauteurs */
};

static EngPhysicsShape *wrap_shape(JPH_Shape *shape, bool scalable)
{
    if (shape == NULL) {
        return NULL;
    }
    EngPhysicsShape *out = SDL_calloc(1, sizeof(*out));
    if (out == NULL) {
        JPH_Shape_Destroy(shape);
        return NULL;
    }
    out->shape = shape;
    out->scalable = scalable;
    return out;
}

EngPhysicsShape *eng_physics_shape_heightfield(EngPhysics *world, const struct EngHeightmap *map)
{
    if (world == NULL || map == NULL || map->samples == NULL || map->size < 4U ||
        map->size > ENG_PHYSICS_MAX_HEIGHTFIELD || !(map->spacing > 0.0F) ||
        !isfinite(map->spacing) || !isfinite(map->altitude_min) || !isfinite(map->altitude_max) ||
        !(map->altitude_max > map->altitude_min)) {
        ENG_LOG_ERROR("physique : champ de hauteurs invalide pour la collision");
        return NULL;
    }
    const size_t count = (size_t)map->size * map->size;
    float *heights = SDL_malloc(count * sizeof(float));
    if (heights == NULL) {
        ENG_LOG_ERROR("physique : mémoire insuffisante pour le champ de hauteurs (%zu octets)",
                      count * sizeof(float));
        return NULL;
    }
    const float range = map->altitude_max - map->altitude_min;
    for (size_t i = 0; i < count; i++) {
        heights[i] = map->altitude_min + ((float)map->samples[i] / 65535.0F * range);
    }
    const JPH_Vec3 offset = {0.0F, 0.0F, 0.0F};
    const JPH_Vec3 scale = {map->spacing, 1.0F, map->spacing};
    JPH_HeightFieldShapeSettings *settings =
        JPH_HeightFieldShapeSettings_Create(heights, &offset, &scale, map->size, NULL);
    JPH_Shape *shape = NULL;
    if (settings != NULL) {
        JPH_HeightFieldShapeSettings_SetBitsPerSample(settings, 16);
        shape = (JPH_Shape *)JPH_HeightFieldShapeSettings_CreateShape(settings);
        JPH_ShapeSettings_Destroy((JPH_ShapeSettings *)settings);
    }
    SDL_free(heights); /* Jolt a recopié (et compressé) les hauteurs */
    if (shape == NULL) {
        ENG_LOG_ERROR("physique : Jolt refuse le champ de hauteurs %ux%u", (unsigned)map->size,
                      (unsigned)map->size);
    }
    return wrap_shape(shape, false);
}

EngPhysicsShape *eng_physics_shape_mesh(EngPhysics *world, const float *positions,
                                        size_t stride_bytes, uint32_t vertex_count,
                                        const uint32_t *indices, uint32_t index_count)
{
    if (world == NULL || positions == NULL || indices == NULL ||
        stride_bytes < 3U * sizeof(float) || vertex_count < 3U || index_count < 3U ||
        index_count % 3U != 0U) {
        ENG_LOG_ERROR("physique : maillage de collision invalide (tailles)");
        return NULL;
    }
    const uint32_t triangle_count = index_count / 3U;
    JPH_Vec3 *vertices = SDL_malloc((size_t)vertex_count * sizeof(JPH_Vec3));
    JPH_IndexedTriangle *triangles =
        SDL_malloc((size_t)triangle_count * sizeof(JPH_IndexedTriangle));
    JPH_Shape *shape = NULL;
    bool valid = vertices != NULL && triangles != NULL;
    for (uint32_t i = 0; valid && i < vertex_count; i++) {
        float p[3];
        memcpy(p, (const char *)positions + ((size_t)i * stride_bytes), sizeof(p));
        valid = finite3(p);
        vertices[i] = to_jph(p);
    }
    for (uint32_t t = 0; valid && t < triangle_count; t++) {
        const uint32_t *tri = indices + ((size_t)t * 3U);
        valid = tri[0] < vertex_count && tri[1] < vertex_count && tri[2] < vertex_count;
        triangles[t] = (JPH_IndexedTriangle){tri[0], tri[1], tri[2], 0U, 0U};
    }
    if (valid) {
        JPH_MeshShapeSettings *settings =
            JPH_MeshShapeSettings_Create2(vertices, vertex_count, triangles, triangle_count);
        if (settings != NULL) {
            JPH_MeshShapeSettings_Sanitize(settings); /* retire les triangles dégénérés */
            shape = (JPH_Shape *)JPH_MeshShapeSettings_CreateShape(settings);
            JPH_ShapeSettings_Destroy((JPH_ShapeSettings *)settings);
        }
    }
    SDL_free(vertices);
    SDL_free(triangles);
    if (shape == NULL) {
        ENG_LOG_ERROR("physique : maillage de collision refusé (NaN, indice hors borne ou aucun "
                      "triangle non dégénéré)");
    }
    return wrap_shape(shape, true);
}

EngPhysicsShape *eng_physics_shape_model(EngPhysics *world, const struct EngModel *model)
{
    if (model == NULL || model->vertices == NULL || model->indices == NULL) {
        ENG_LOG_ERROR("physique : modèle vide pour la collision");
        return NULL;
    }
    return eng_physics_shape_mesh(world, model->vertices[0].position, sizeof(EngModelVertex),
                                  model->vertex_count, model->indices, model->index_count);
}

void eng_physics_shape_destroy(EngPhysicsShape *shape)
{
    if (shape == NULL) {
        return;
    }
    JPH_Shape_Destroy(shape->shape);
    SDL_free(shape);
}

EngBody eng_physics_add_static_shape(EngPhysics *world, const EngPhysicsShape *shape,
                                     const EngPhysicsPose *pose)
{
    if (world == NULL || shape == NULL) {
        return ENG_BODY_INVALID;
    }
    const EngPhysicsPose p = pose != NULL ? *pose : eng_physics_pose_identity();
    const float q_len2 = (p.rotation[0] * p.rotation[0]) + (p.rotation[1] * p.rotation[1]) +
                         (p.rotation[2] * p.rotation[2]) + (p.rotation[3] * p.rotation[3]);
    if (!finite3(p.position) || !finite3(p.rotation) || !isfinite(p.rotation[3]) ||
        !(fabsf(q_len2 - 1.0F) <= 2e-3F) || !(p.scale > 0.0F) || !isfinite(p.scale) ||
        (p.scale != 1.0F && !shape->scalable)) {
        ENG_LOG_WARN("physique : pose de forme invalide (rotation non unitaire, échelle <= 0, ou "
                     "échelle d'un champ de hauteurs)");
        return ENG_BODY_INVALID;
    }
    const JPH_Vec3 pos = to_jph(p.position);
    const JPH_Quat rot = {p.rotation[0], p.rotation[1], p.rotation[2], p.rotation[3]};
    if (p.scale == 1.0F) {
        return add_shape(world, shape->shape, &pos, &rot, ENG_LAYER_STATIC, false);
    }
    const JPH_Vec3 scale = {p.scale, p.scale, p.scale};
    JPH_Shape *scaled = (JPH_Shape *)JPH_ScaledShape_Create(shape->shape, &scale);
    if (scaled == NULL) {
        return ENG_BODY_INVALID;
    }
    return add_shape(world, scaled, &pos, &rot, ENG_LAYER_STATIC, true);
}

/* ── Lancer de rayon ── */

/* Filtre de couches de Jolt : le masque de couches est passé dans userData. (joltc n'a qu'un jeu de
 * fonctions de filtre pour tout le processus : on l'installe une fois, à la création du premier
 * monde.) */
static bool layer_filter_should_collide(void *user_data, JPH_ObjectLayer layer)
{
    const uint32_t mask = *(const uint32_t *)user_data;
    return layer < 32U && ((mask >> layer) & 1U) != 0U;
}

bool eng_physics_raycast(const EngPhysics *world, const float origin[3], const float direction[3],
                         float max_distance, uint32_t layer_mask, EngRayHit *hit)
{
    if (world == NULL || hit == NULL || !finite3(origin) || !finite3(direction) ||
        !(max_distance > 0.0F) || !isfinite(max_distance)) {
        return false;
    }
    const float len = sqrtf((direction[0] * direction[0]) + (direction[1] * direction[1]) +
                            (direction[2] * direction[2]));
    if (!(len > 1e-12F)) {
        return false;
    }
    const JPH_RVec3 o = to_jph(origin);
    const JPH_Vec3 d = {direction[0] / len * max_distance, direction[1] / len * max_distance,
                        direction[2] / len * max_distance};
    /* le masque vit sur la pile pendant l'appel : le filtre n'est utilisé que dans CastRay */
    uint32_t mask_storage = layer_mask;
    JPH_ObjectLayerFilter *filter = JPH_ObjectLayerFilter_Create(&mask_storage);
    JPH_RayCastResult result;
    memset(&result, 0, sizeof(result));
    const bool found = JPH_NarrowPhaseQuery_CastRay(
        JPH_PhysicsSystem_GetNarrowPhaseQuery(world->system), &o, &d, &result, NULL, filter, NULL);
    JPH_ObjectLayerFilter_Destroy(filter);
    if (!found) {
        return false;
    }
    hit->body = result.bodyID;
    hit->distance = result.fraction * max_distance;
    hit->point[0] = o.x + (d.x * result.fraction);
    hit->point[1] = o.y + (d.y * result.fraction);
    hit->point[2] = o.z + (d.z * result.fraction);
    const JPH_RVec3 point = to_jph(hit->point);
    JPH_Vec3 normal = {0.0F, 1.0F, 0.0F};
    const JPH_Body *body = JPH_PhysicsSystem_GetBodyPtr(world->system, result.bodyID);
    if (body != NULL) {
        JPH_Body_GetWorldSpaceSurfaceNormal(body, result.subShapeID2, &point, &normal);
    }
    hit->normal[0] = normal.x;
    hit->normal[1] = normal.y;
    hit->normal[2] = normal.z;
    return true;
}

typedef struct SweepResult {
    bool hit;
    JPH_ShapeCastResult result;
} SweepResult;
static void sweep_callback(void *context, const JPH_ShapeCastResult *result)
{
    SweepResult *out = context;
    out->hit = true;
    out->result = *result;
}

bool eng_physics_spherecast(const EngPhysics *world, const float origin[3],
                            const float direction[3], float max_distance, float radius,
                            uint32_t layer_mask, EngRayHit *hit)
{
    if (world == NULL || hit == NULL || !finite3(origin) || !finite3(direction) ||
        !(max_distance > 0.0F) || !isfinite(max_distance) || !(radius > 0.0F) ||
        !(radius <= 10.0F)) {
        return false;
    }
    const float len = sqrtf((direction[0] * direction[0]) + (direction[1] * direction[1]) +
                            (direction[2] * direction[2]));
    if (!(len > 1e-12F)) {
        return false;
    }
    const JPH_Vec3 d = {direction[0] / len * max_distance, direction[1] / len * max_distance,
                        direction[2] / len * max_distance};
    JPH_Shape *sphere = (JPH_Shape *)JPH_SphereShape_Create(radius);
    if (sphere == NULL) {
        return false;
    }
    const JPH_Quat identity = {0.0F, 0.0F, 0.0F, 1.0F};
    const JPH_RVec3 o = to_jph(origin);
    JPH_RMat4 transform;
    JPH_Mat4_RotationTranslation(&transform, &identity, &o);
    JPH_ShapeCastSettings settings;
    memset(&settings, 0, sizeof(settings));
    JPH_ShapeCastSettings_Init(&settings);
    JPH_RVec3 base_offset = {0.0F, 0.0F, 0.0F};
    uint32_t mask_storage = layer_mask;
    JPH_ObjectLayerFilter *filter = JPH_ObjectLayerFilter_Create(&mask_storage);
    SweepResult out;
    memset(&out, 0, sizeof(out));
    JPH_NarrowPhaseQuery_CastShape2(JPH_PhysicsSystem_GetNarrowPhaseQuery(world->system), sphere,
                                    &transform, &d, &settings, &base_offset,
                                    JPH_CollisionCollectorType_ClosestHit, sweep_callback, &out,
                                    NULL, filter, NULL, NULL);
    JPH_ObjectLayerFilter_Destroy(filter);
    JPH_Shape_Destroy(sphere);
    if (!out.hit) {
        return false;
    }
    hit->body = out.result.bodyID2;
    hit->distance = fminf(fmaxf(out.result.fraction, 0.0F), 1.0F) * max_distance;
    hit->point[0] = out.result.contactPointOn2.x;
    hit->point[1] = out.result.contactPointOn2.y;
    hit->point[2] = out.result.contactPointOn2.z;
    const JPH_Vec3 axis = out.result.penetrationAxis;
    const float alen = sqrtf((axis.x * axis.x) + (axis.y * axis.y) + (axis.z * axis.z));
    if (alen > 1e-12F) { /* l'axe va de la sphère vers le corps : la normale va dans l'autre sens */
        hit->normal[0] = -axis.x / alen;
        hit->normal[1] = -axis.y / alen;
        hit->normal[2] = -axis.z / alen;
    } else {
        hit->normal[0] = -d.x / max_distance;
        hit->normal[1] = -d.y / max_distance;
        hit->normal[2] = -d.z / max_distance;
    }
    return true;
}

bool eng_physics_body_position(const EngPhysics *world, EngBody body, float out[3])
{
    if (!known(world, body) || out == NULL) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_RVec3 p;
    JPH_BodyInterface_GetCenterOfMassPosition(bi, body, &p);
    out[0] = p.x; /* joltc sans double précision : JPH_RVec3 est un JPH_Vec3 */
    out[1] = p.y;
    out[2] = p.z;
    return true;
}

bool eng_physics_body_rotation(const EngPhysics *world, EngBody body, float out[4])
{
    if (!known(world, body) || out == NULL) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_Quat q;
    JPH_BodyInterface_GetRotation(bi, body, &q);
    out[0] = q.x;
    out[1] = q.y;
    out[2] = q.z;
    out[3] = q.w;
    return true;
}

bool eng_physics_body_interpolated(const EngPhysics *world, EngBody body, double alpha,
                                   float position[3], float rotation[4])
{
    if (!known(world, body) || !(alpha >= 0.0) || !(alpha <= 1.0)) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_RVec3 pos;
    JPH_Quat rot;
    JPH_BodyInterface_GetPositionAndRotation(bi, body, &pos, &rot);
    const uint32_t slot = body & BODY_SLOT_MASK;
    /* alpha = 0 et alpha = 1 rendent EXACTEMENT l'état d'avant le pas et l'état courant (sans
     * arrondi de lerp ni de slerp) */
    const float *p = world->prev + ((size_t)slot * 7U);
    const bool have = world->has_prev[slot] != 0U;
    const bool at_start = have && alpha == 0.0;
    const bool blend = have && alpha > 0.0 && alpha < 1.0;
    const float t = (float)alpha;
    if (position != NULL) {
        for (int k = 0; k < 3; k++) {
            const float cur = k == 0 ? pos.x : (k == 1 ? pos.y : pos.z);
            position[k] = at_start ? p[k] : (blend ? p[k] + ((cur - p[k]) * t) : cur);
        }
    }
    if (rotation != NULL) {
        if (at_start) {
            memcpy(rotation, p + 3, 4U * sizeof(float));
        } else if (blend) {
            versor from = {p[3], p[4], p[5], p[6]};
            versor to = {rot.x, rot.y, rot.z, rot.w};
            glm_quat_slerp(from, to, t, rotation);
        } else {
            rotation[0] = rot.x;
            rotation[1] = rot.y;
            rotation[2] = rot.z;
            rotation[3] = rot.w;
        }
    }
    return true;
}

bool eng_physics_body_velocity(const EngPhysics *world, EngBody body, float out[3])
{
    if (!known(world, body) || out == NULL) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    JPH_Vec3 v;
    JPH_BodyInterface_GetLinearVelocity(bi, body, &v);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
    return true;
}

bool eng_physics_set_body_velocity(EngPhysics *world, EngBody body, const float velocity[3])
{
    if (!known(world, body) || !finite3(velocity)) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    const JPH_Vec3 v = to_jph(velocity);
    JPH_BodyInterface_SetLinearVelocity(bi, body, &v);
    return true;
}

bool eng_physics_body_is_active(const EngPhysics *world, EngBody body)
{
    if (!known(world, body)) {
        return false;
    }
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    return JPH_BodyInterface_IsActive(bi, body);
}

uint32_t eng_physics_body_count(const EngPhysics *world)
{
    return world != NULL ? JPH_PhysicsSystem_GetNumBodies(world->system) : 0U;
}

/* ── Debug draw ── */

static void mat4_from_jph(const JPH_Mat4 *m, mat4 out)
{
    for (int c = 0; c < 4; c++) {
        out[c][0] = m->column[c].x;
        out[c][1] = m->column[c].y;
        out[c][2] = m->column[c].z;
        out[c][3] = m->column[c].w;
    }
}

/* Couleur d'un corps endormi : 40 % de l'intensité. */
static EngDebugColor dimmed(EngDebugColor c)
{
    return (EngDebugColor){(uint8_t)(c.r * 2U / 5U), (uint8_t)(c.g * 2U / 5U),
                           (uint8_t)(c.b * 2U / 5U), c.a};
}

static bool draw_bounds(EngDebugLines *lines, const JPH_Body *body, EngDebugColor color)
{
    JPH_AABox box;
    JPH_Body_GetWorldSpaceBounds(body, &box);
    const vec3 lo = {box.min.x, box.min.y, box.min.z};
    const vec3 hi = {box.max.x, box.max.y, box.max.z};
    return eng_debug_aabb(lines, lo, hi, color);
}

static bool draw_capsule(EngDebugLines *lines, const JPH_CapsuleShape *capsule, mat4 transform,
                         EngDebugColor color, uint32_t segments)
{
    const float half = JPH_CapsuleShape_GetHalfHeightOfCylinder(capsule);
    const float radius = JPH_CapsuleShape_GetRadius(capsule);
    vec3 top;
    vec3 bottom;
    glm_mat4_mulv3(transform, (vec3){0.0F, half, 0.0F}, 1.0F, top);
    glm_mat4_mulv3(transform, (vec3){0.0F, -half, 0.0F}, 1.0F, bottom);
    bool ok = eng_debug_sphere(lines, top, radius, color, segments);
    ok = eng_debug_sphere(lines, bottom, radius, color, segments) && ok;
    const float offsets[4][2] = {{radius, 0.0F}, {-radius, 0.0F}, {0.0F, radius}, {0.0F, -radius}};
    for (int i = 0; i < 4; i++) {
        vec3 a;
        vec3 b;
        glm_mat4_mulv3(transform, (vec3){offsets[i][0], half, offsets[i][1]}, 1.0F, a);
        glm_mat4_mulv3(transform, (vec3){offsets[i][0], -half, offsets[i][1]}, 1.0F, b);
        ok = eng_debug_line(lines, a, b, color) && ok;
    }
    return ok;
}

/* Grille de Jolt dans une fenêtre autour de `focus` : arêtes de chaque cellule et diagonale a-d
 * (la seule que Jolt utilise, voir docs/physics.md). */
static bool draw_heightfield(EngDebugLines *lines, const JPH_HeightFieldShape *field,
                             mat4 transform, const float focus[3], float radius,
                             EngDebugColor color)
{
    const uint32_t n = JPH_HeightFieldShape_GetSampleCount(field);
    /* l'échantillon (0, 0) est à l'origine locale en x et z (décalage nul, voir
     * eng_physics_shape_heightfield) : l'échantillon (1, 0) donne donc le pas */
    JPH_Vec3 p1;
    JPH_HeightFieldShape_GetPosition(field, 1U, 0U, &p1);
    const float spacing = p1.x;
    if (n < 2U || !(spacing > 0.0F)) {
        return false;
    }
    /* foyer en coordonnées locales : la forme n'est ni mise à l'échelle ni déformée (rotation
     * orthonormée), l'inverse est donc la transposée */
    const vec3 rel = {focus[0] - transform[3][0], focus[1] - transform[3][1],
                      focus[2] - transform[3][2]};
    const float lx =
        (transform[0][0] * rel[0]) + (transform[0][1] * rel[1]) + (transform[0][2] * rel[2]);
    const float lz =
        (transform[2][0] * rel[0]) + (transform[2][1] * rel[1]) + (transform[2][2] * rel[2]);
    /* fenêtre centrée sur le foyer : `half` cellules de chaque côté, au plus la moitié du plafond
     */
    const float half = fminf(ceilf(radius / spacing), (float)ENG_PHYSICS_DEBUG_MAX_HF_CELLS * 0.5F);
    const float last = (float)(n - 1U);
    const float ci = fminf(fmaxf(roundf(lx / spacing), 0.0F), last);
    const float cj = fminf(fmaxf(roundf(lz / spacing), 0.0F), last);
    const uint32_t i0 = (uint32_t)fmaxf(ci - half, 0.0F);
    const uint32_t i1 = (uint32_t)fminf(ci + half, last);
    const uint32_t j0 = (uint32_t)fmaxf(cj - half, 0.0F);
    const uint32_t j1 = (uint32_t)fminf(cj + half, last);
    bool ok = true;
    for (uint32_t j = j0; j <= j1; j++) {
        for (uint32_t i = i0; i <= i1; i++) {
            /* Jolt complète un champ dont la taille n'est pas un multiple de son bloc par des
             * échantillons « sans collision » (33 -> 34) : ils n'ont pas de surface, on ne les
             * trace pas */
            if (JPH_HeightFieldShape_IsNoCollision(field, i, j)) {
                continue;
            }
            JPH_Vec3 a;
            JPH_HeightFieldShape_GetPosition(field, i, j, &a);
            vec3 wa;
            glm_mat4_mulv3(transform, (vec3){a.x, a.y, a.z}, 1.0F, wa);
            const uint32_t di[3] = {1U, 0U, 1U};
            const uint32_t dj[3] = {0U, 1U, 1U};
            for (int k = 0; k < 3; k++) {
                if (i + di[k] > i1 || j + dj[k] > j1 ||
                    JPH_HeightFieldShape_IsNoCollision(field, i + di[k], j + dj[k])) {
                    continue;
                }
                JPH_Vec3 b;
                JPH_HeightFieldShape_GetPosition(field, i + di[k], j + dj[k], &b);
                vec3 wb;
                glm_mat4_mulv3(transform, (vec3){b.x, b.y, b.z}, 1.0F, wb);
                ok = eng_debug_line(lines, wa, wb, color) && ok;
            }
        }
    }
    return ok;
}

/* Le corps touche-t-il la sphère (focus, radius) ? `horizontal` : on ignore la hauteur (un terrain
 * reste visible quand le foyer est haut au-dessus de lui). */
static bool body_near(const JPH_Body *body, const float focus[3], float radius, bool horizontal)
{
    JPH_AABox box;
    JPH_Body_GetWorldSpaceBounds(body, &box);
    const float lo[3] = {box.min.x, box.min.y, box.min.z};
    const float hi[3] = {box.max.x, box.max.y, box.max.z};
    float d2 = 0.0F;
    for (int k = 0; k < 3; k++) {
        if (horizontal && k == 1) {
            continue;
        }
        const float d = fmaxf(fmaxf(lo[k] - focus[k], focus[k] - hi[k]), 0.0F);
        d2 += d * d;
    }
    return d2 <= radius * radius;
}

uint32_t eng_physics_debug_draw(const EngPhysics *world, EngDebugLines *lines,
                                const EngPhysicsDebugDraw *options)
{
    if (world == NULL || lines == NULL || options == NULL || !finite3(options->focus) ||
        !isfinite(options->radius) || options->radius < 0.0F) {
        return 0;
    }
    const uint32_t count = JPH_PhysicsSystem_GetNumBodies(world->system);
    JPH_PhysicsSystem_GetBodies(world->system, world->ids, count);
    JPH_BodyInterface *bi = JPH_PhysicsSystem_GetBodyInterface(world->system);
    uint32_t drawn = 0;
    for (uint32_t n = 0; n < count; n++) {
        const JPH_BodyID id = world->ids[n];
        const uint32_t slot = id & BODY_SLOT_MASK;
        if (slot >= world->max_bodies || world->layers[slot] == 0U) {
            continue;
        }
        const EngPhysicsLayer layer = (EngPhysicsLayer)(world->layers[slot] - 1U);
        if ((options->layer_mask & (1U << (uint32_t)layer)) == 0U) {
            continue;
        }
        const JPH_Body *body = JPH_PhysicsSystem_GetBodyPtr(world->system, id);
        if (body == NULL) {
            continue;
        }
        const JPH_Shape *shape = JPH_BodyInterface_GetShape(bi, id);
        const JPH_ShapeSubType kind = JPH_Shape_GetSubType(shape);
        if (options->radius > 0.0F && !body_near(body, options->focus, options->radius,
                                                 kind == JPH_ShapeSubType_HeightField)) {
            continue;
        }
        EngDebugColor color = eng_physics_layer_color(layer);
        /* seuls les corps simulés « s'endorment » : un décor ou un capteur cinématique est
         * toujours inactif pour Jolt, sans que cela veuille dire quelque chose */
        if ((layer == ENG_LAYER_DYNAMIC || layer == ENG_LAYER_CHARACTER) &&
            !JPH_BodyInterface_IsActive(bi, id)) {
            color = dimmed(color);
        }
        JPH_RMat4 tr;
        JPH_BodyInterface_GetWorldTransform(bi, id, &tr);
        mat4 transform;
        mat4_from_jph(&tr, transform);
        bool ok;
        switch (kind) {
        case JPH_ShapeSubType_Sphere:
            ok = eng_debug_sphere(lines, transform[3],
                                  JPH_SphereShape_GetRadius((const JPH_SphereShape *)shape), color,
                                  options->segments);
            break;
        case JPH_ShapeSubType_Box: {
            JPH_Vec3 half;
            JPH_BoxShape_GetHalfExtent((const JPH_BoxShape *)shape, &half);
            ok = eng_debug_box(lines, transform, (vec3){half.x, half.y, half.z}, color);
            break;
        }
        case JPH_ShapeSubType_Capsule:
            ok = draw_capsule(lines, (const JPH_CapsuleShape *)shape, transform, color,
                              options->segments);
            break;
        case JPH_ShapeSubType_HeightField:
            ok = options->radius > 0.0F
                     ? draw_heightfield(lines, (const JPH_HeightFieldShape *)shape, transform,
                                        options->focus, options->radius, color)
                     : draw_bounds(lines, body, color);
            break;
        default:
            ok = draw_bounds(lines, body, color);
            break;
        }
        if (ok) {
            drawn++;
        }
    }
    return drawn;
}

/* ── Contrôleur de personnage ── */

struct EngCharacter {
    EngPhysics *world;
    JPH_CharacterVirtual *cv;
    EngBody inner;       /* corps intérieur (détection par les capteurs, debug draw) */
    float centre_height; /* du centre de la capsule aux pieds */
    float step_height;
    float gravity_rise;
    float gravity_fall;
    bool rising;   /* monte à cause d'un saut (jump/launch) : la vitesse verticale est voulue */
    float prev[3]; /* pieds avant le dernier update */
    bool has_prev;
};

#define CHARACTER_DEFAULT_RADIUS 0.3F
#define CHARACTER_DEFAULT_HEIGHT 1.8F
#define CHARACTER_DEFAULT_SLOPE_DEG 45.0F
#define CHARACTER_DEFAULT_STEP 0.3F
#define CHARACTER_DEFAULT_MASS 70.0F

EngCharacter *eng_character_create(EngPhysics *world, const EngCharacterDesc *desc)
{
    if (world == NULL || desc == NULL || !finite3(desc->position)) {
        ENG_LOG_ERROR("physique : personnage invalide (monde, description ou position)");
        return NULL;
    }
    const float radius = desc->radius != 0.0F ? desc->radius : CHARACTER_DEFAULT_RADIUS;
    const float height = desc->height != 0.0F ? desc->height : CHARACTER_DEFAULT_HEIGHT;
    const float slope =
        desc->max_slope_deg != 0.0F ? desc->max_slope_deg : CHARACTER_DEFAULT_SLOPE_DEG;
    const float step = desc->step_height != 0.0F ? desc->step_height : CHARACTER_DEFAULT_STEP;
    const float mass = desc->mass != 0.0F ? desc->mass : CHARACTER_DEFAULT_MASS;
    if (!isfinite(radius) || !(radius > 0.0F) || !isfinite(height) || !(height > 2.0F * radius) ||
        !isfinite(slope) || !(slope > 0.0F) || !(slope < 90.0F) || !isfinite(step) ||
        !(step >= 0.0F) || !isfinite(mass) || !(mass > 0.0F)) {
        ENG_LOG_ERROR("physique : paramètres de personnage invalides (rayon %.3f, hauteur %.3f, "
                      "pente %.1f, marche %.3f, masse %.1f)",
                      (double)radius, (double)height, (double)slope, (double)step, (double)mass);
        return NULL;
    }
    EngCharacter *c = SDL_calloc(1, sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    const float half_cylinder = (height * 0.5F) - radius;
    c->world = world;
    c->centre_height = height * 0.5F;
    c->step_height = step;
    c->gravity_rise = 1.0F;
    c->rising = false;
    c->gravity_fall = 1.0F;
    JPH_Shape *capsule = (JPH_Shape *)JPH_CapsuleShape_Create(half_cylinder, radius);
    if (capsule == NULL) {
        SDL_free(c);
        return NULL;
    }
    /* Valeurs par défaut de Jolt 5.7.0 écrites ici : JPH_CharacterVirtualSettings_Init de joltc
     * fait fuir un EmptyShapeSettings à chaque appel. */
    JPH_CharacterVirtualSettings settings;
    memset(&settings, 0, sizeof(settings));
    settings.ID = world->next_character_id++;
    settings.maxStrength = 100.0F;
    settings.backFaceMode = JPH_BackFaceMode_CollideWithBackFaces;
    settings.predictiveContactDistance = 0.1F;
    settings.maxCollisionIterations = 5U;
    settings.maxConstraintIterations = 15U;
    settings.minTimeRemaining = 1.0e-4F;
    settings.collisionTolerance = 1.0e-3F;
    settings.characterPadding = 0.02F;
    settings.maxNumHits = 256U;
    settings.hitReductionCosMaxAngle = 0.999F;
    settings.penetrationRecoverySpeed = 1.0F;
    settings.innerBodyIDOverride = ENG_BODY_INVALID;
    settings.base.shape = capsule;
    settings.base.up = (JPH_Vec3){0.0F, 1.0F, 0.0F};
    settings.base.maxSlopeAngle = slope * (3.14159265358979F / 180.0F);
    /* seule la moitié basse de la capsule compte comme appui : on ne se tient pas sur un mur */
    settings.base.supportingVolume.normal = (JPH_Vec3){0.0F, 1.0F, 0.0F};
    settings.base.supportingVolume.distance = -radius;
    settings.mass = mass;
    settings.innerBodyShape = capsule;
    settings.innerBodyLayer = (JPH_ObjectLayer)ENG_LAYER_CHARACTER;
    const JPH_RVec3 centre = {desc->position[0], desc->position[1] + c->centre_height,
                              desc->position[2]};
    const JPH_Quat identity = {0.0F, 0.0F, 0.0F, 1.0F};
    c->cv = JPH_CharacterVirtual_Create(&settings, &centre, &identity, 0U, world->system);
    JPH_Shape_Destroy(capsule); /* le personnage et son corps gardent leurs propres références */
    if (c->cv == NULL) {
        SDL_free(c);
        return NULL;
    }
    c->inner = JPH_CharacterVirtual_GetInnerBodyID(c->cv);
    if (c->inner != ENG_BODY_INVALID && (c->inner & BODY_SLOT_MASK) < world->max_bodies) {
        world->layers[c->inner & BODY_SLOT_MASK] = (uint8_t)(ENG_LAYER_CHARACTER + 1);
        world->has_prev[c->inner & BODY_SLOT_MASK] = 0U;
    }
    world->characters++;
    memcpy(c->prev, desc->position, sizeof(c->prev));
    return c;
}

void eng_character_destroy(EngCharacter *character)
{
    if (character == NULL) {
        return;
    }
    EngPhysics *world = character->world;
    const EngBody inner = character->inner;
    JPH_CharacterBase_Destroy((JPH_CharacterBase *)character->cv);
    if (inner != ENG_BODY_INVALID && (inner & BODY_SLOT_MASK) < world->max_bodies) {
        world->layers[inner & BODY_SLOT_MASK] = 0U;
        world->has_prev[inner & BODY_SLOT_MASK] = 0U;
    }
    world->characters--;
    SDL_free(character);
}

static void character_feet(const EngCharacter *c, float feet[3])
{
    JPH_RVec3 p;
    JPH_CharacterVirtual_GetPosition(c->cv, &p);
    feet[0] = p.x;
    feet[1] = p.y - c->centre_height;
    feet[2] = p.z;
}

bool eng_character_update(EngCharacter *character, float dt, const float move[3])
{
    if (character == NULL || move == NULL || !finite3(move) || !(dt > 0.0F) ||
        dt > MAX_STEP_SECONDS) {
        return false;
    }
    JPH_PhysicsSystem *system = character->world->system;
    character_feet(character, character->prev);
    character->has_prev = true;

    JPH_Vec3 gravity;
    JPH_PhysicsSystem_GetGravity(system, &gravity);
    JPH_Vec3 current;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &current);
    JPH_Vec3 ground_velocity;
    JPH_CharacterBase_GetGroundVelocity((JPH_CharacterBase *)character->cv, &ground_velocity);
    /* debout : on prend la vitesse du sol (sauf si on monte déjà : saut) ; sinon on garde la
     * vitesse verticale acquise. Puis la gravité. */
    float vy = current.y;
    bool walking = false; /* debout et pas en train de sauter */
    if (JPH_CharacterBase_GetGroundState((JPH_CharacterBase *)character->cv) ==
            JPH_GroundState_OnGround &&
        (vy - ground_velocity.y) < 0.1F) {
        vy = ground_velocity.y;
        walking = true;
    } else {
        vy += gravity.y * dt * (vy < 0.0F ? character->gravity_fall : character->gravity_rise);
        /* debout, la gravité n'ajoute pas de vitesse : le sol la porte */
    }
    const JPH_Vec3 wanted = {move[0], 0.0F, move[2]};
    JPH_Vec3 allowed;
    JPH_CharacterVirtual_CancelVelocityTowardsSteepSlopes(character->cv, &wanted, &allowed);
    JPH_Vec3 along = allowed;
    if (walking) {
        /* Sur une pente, Jolt projette la vitesse sur le plan : la vitesse horizontale baisserait
         * de cos² (angle). On garde au contraire la vitesse le long du sol égale à la vitesse
         * voulue : on prend la composante tangente de la vitesse voulue et on lui rend sa
         * longueur. */
        JPH_Vec3 n;
        JPH_CharacterBase_GetGroundNormal((JPH_CharacterBase *)character->cv, &n);
        const float d = (allowed.x * n.x) + (allowed.y * n.y) + (allowed.z * n.z);
        const JPH_Vec3 t = {allowed.x - (n.x * d), allowed.y - (n.y * d), allowed.z - (n.z * d)};
        const float t_len = sqrtf((t.x * t.x) + (t.y * t.y) + (t.z * t.z));
        const float w_len = sqrtf((allowed.x * allowed.x) + (allowed.z * allowed.z));
        if (n.y > 0.0F && t_len > 1e-6F) {
            const float k = w_len / t_len;
            along = (JPH_Vec3){t.x * k, t.y * k, t.z * k};
        }
    }
    const JPH_Vec3 velocity = {along.x, vy + along.y, along.z};
    JPH_CharacterVirtual_SetLinearVelocity(character->cv, &velocity);

    JPH_ExtendedUpdateSettings ext;
    memset(&ext, 0, sizeof(ext));
    ext.stickToFloorStepDown = (JPH_Vec3){0.0F, -0.5F, 0.0F};
    ext.walkStairsStepUp = (JPH_Vec3){0.0F, character->step_height, 0.0F};
    ext.walkStairsMinStepForward = 0.02F;
    ext.walkStairsStepForwardTest = 0.15F;
    ext.walkStairsCosAngleForwardContact = 0.2588190451F; /* cos 75 degrés */
    ext.walkStairsStepDownExtra = (JPH_Vec3){0.0F, 0.0F, 0.0F};
    JPH_CharacterVirtual_ExtendedUpdate(character->cv, dt, &ext,
                                        (JPH_ObjectLayer)ENG_LAYER_CHARACTER, system, NULL, NULL);
    /* Contre le bord d'une marche, la capsule glisse le long du contact et Jolt lui laisse une
     * vitesse verticale vers le HAUT (mesuré : +3 m/s à 3,5 m/s de course) : sans correction, le
     * personnage décolle et décrit un arc. Une vitesse montante qui ne vient pas d'un saut est
     * supprimée ; au sol on la garde (elle suit la pente, et elle est recalculée à chaque pas). */
    const JPH_GroundState after =
        JPH_CharacterBase_GetGroundState((JPH_CharacterBase *)character->cv);
    JPH_Vec3 v;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &v);
    if (after == JPH_GroundState_OnGround) {
        character->rising = false;
    } else if (character->rising) {
        if (v.y <= 0.0F) {
            character->rising = false;
        }
    } else if (v.y > 0.0F) {
        v.y = 0.0F;
        JPH_CharacterVirtual_SetLinearVelocity(character->cv, &v);
    }
    return true;
}

bool eng_character_jump(EngCharacter *character, float speed)
{
    if (character == NULL || !isfinite(speed) || !(speed > 0.0F) ||
        eng_character_ground(character) != ENG_GROUND_SUPPORTED) {
        return false;
    }
    JPH_Vec3 v;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &v);
    JPH_Vec3 ground;
    JPH_CharacterBase_GetGroundVelocity((JPH_CharacterBase *)character->cv, &ground);
    v.y = ground.y + speed;
    JPH_CharacterVirtual_SetLinearVelocity(character->cv, &v);
    character->rising = true;
    return true;
}

bool eng_character_launch(EngCharacter *character, float speed)
{
    if (character == NULL || !isfinite(speed) || !(speed > 0.0F)) {
        return false;
    }
    JPH_Vec3 v;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &v);
    v.y = speed;
    JPH_CharacterVirtual_SetLinearVelocity(character->cv, &v);
    character->rising = true;
    return true;
}

bool eng_character_cut_jump(EngCharacter *character, float factor)
{
    if (character == NULL || !isfinite(factor) || !(factor >= 0.0F) || !(factor < 1.0F)) {
        return false;
    }
    JPH_Vec3 v;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &v);
    if (v.y > 0.0F) {
        v.y *= factor;
        JPH_CharacterVirtual_SetLinearVelocity(character->cv, &v);
    }
    return true;
}

bool eng_character_set_gravity_scale(EngCharacter *character, float rise, float fall)
{
    if (character == NULL || !isfinite(rise) || !(rise > 0.0F) || rise > 5.0F || !isfinite(fall) ||
        !(fall > 0.0F) || fall > 5.0F) {
        return false;
    }
    character->gravity_rise = rise;
    character->gravity_fall = fall;
    return true;
}

bool eng_character_teleport(EngCharacter *character, const float feet[3])
{
    if (character == NULL || !finite3(feet)) {
        return false;
    }
    const JPH_RVec3 centre = {feet[0], feet[1] + character->centre_height, feet[2]};
    JPH_CharacterVirtual_SetPosition(character->cv, &centre);
    const JPH_Vec3 zero = {0.0F, 0.0F, 0.0F};
    JPH_CharacterVirtual_SetLinearVelocity(character->cv, &zero);
    memcpy(character->prev, feet, sizeof(character->prev));
    character->has_prev = false;
    character->rising = false;
    return true;
}

bool eng_character_position(const EngCharacter *character, float feet[3])
{
    if (character == NULL || feet == NULL) {
        return false;
    }
    character_feet(character, feet);
    return true;
}

bool eng_character_interpolated(const EngCharacter *character, double alpha, float feet[3])
{
    if (character == NULL || feet == NULL || !(alpha >= 0.0) || !(alpha <= 1.0)) {
        return false;
    }
    float now[3];
    character_feet(character, now);
    const float t = (float)alpha;
    for (int k = 0; k < 3; k++) {
        if (!character->has_prev || alpha >= 1.0) {
            feet[k] = now[k];
        } else if (alpha == 0.0) {
            feet[k] = character->prev[k];
        } else {
            feet[k] = character->prev[k] + ((now[k] - character->prev[k]) * t);
        }
    }
    return true;
}

bool eng_character_velocity(const EngCharacter *character, float out[3])
{
    if (character == NULL || out == NULL) {
        return false;
    }
    JPH_Vec3 v;
    JPH_CharacterVirtual_GetLinearVelocity(character->cv, &v);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
    return true;
}

EngCharacterGround eng_character_ground(const EngCharacter *character)
{
    if (character == NULL) {
        return ENG_GROUND_AIR;
    }
    switch (JPH_CharacterBase_GetGroundState((JPH_CharacterBase *)character->cv)) {
    case JPH_GroundState_OnGround:
        return ENG_GROUND_SUPPORTED;
    case JPH_GroundState_OnSteepGround:
        return ENG_GROUND_STEEP;
    case JPH_GroundState_NotSupported:
        return ENG_GROUND_UNSUPPORTED;
    default:
        return ENG_GROUND_AIR;
    }
}

bool eng_character_ground_normal(const EngCharacter *character, float normal[3])
{
    if (character == NULL || normal == NULL ||
        JPH_CharacterBase_GetGroundState((JPH_CharacterBase *)character->cv) ==
            JPH_GroundState_InAir) {
        return false;
    }
    JPH_Vec3 n;
    JPH_CharacterBase_GetGroundNormal((JPH_CharacterBase *)character->cv, &n);
    normal[0] = n.x;
    normal[1] = n.y;
    normal[2] = n.z;
    return true;
}

EngBody eng_character_body(const EngCharacter *character)
{
    return character != NULL ? character->inner : ENG_BODY_INVALID;
}

#endif /* OW_PHYSICS */
