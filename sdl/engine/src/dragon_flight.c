#include "engine/dragon_flight.h"

#include <math.h>
#include <stddef.h>

#define DT (1.0F / 60.0F)
#define PI_F 3.14159265F
#define ORBIT_LEAD 0.5F /* rad : il vise un point du cercle un peu devant lui */
/* il fige son axe quand le but est entre lead + 84 m et lead + 200 m devant lui, dans son axe : son
 * feu met alors au moins 2 s à atteindre le point visé (le temps de s'écarter de 11 m) */
#define LOCK_NEAR 84.0F
#define LOCK_FAR 200.0F
#define LOCK_BEARING 0.03F   /* rad : moins de 2 degrés d'écart quand il fige son axe */
#define LANDING_START 130.0F /* m : distance de l'antre où il commence à se poser */
#define LANDING_BEARING 0.3F /* rad */
#define LANDING_SETTLE 60U   /* derniers ticks : il glisse jusqu'au centre de l'antre */
#define FALL_SPEED 0.9F      /* m par tick : chute quand il meurt en vol */

static bool finite2(const float v[2])
{
    return isfinite(v[0]) && isfinite(v[1]);
}

static float wrap(float a)
{
    while (a > PI_F) {
        a -= 2.0F * PI_F;
    }
    while (a < -PI_F) {
        a += 2.0F * PI_F;
    }
    return a;
}

static float smooth(float u)
{
    const float t = u < 0.0F ? 0.0F : (u > 1.0F ? 1.0F : u);
    return t * t * (3.0F - (2.0F * t));
}

static void forward(const EngDragonFlight *d, float out[2])
{
    out[0] = -sinf(d->yaw);
    out[1] = -cosf(d->yaw);
}

/* angle (rad, signé) entre son cap et la direction du point `goal` ; 0 si le point est sur lui */
static float bearing(const EngDragonFlight *d, const float goal[2])
{
    const float dx = goal[0] - d->pos[0];
    const float dz = goal[1] - d->pos[1];
    if ((dx * dx) + (dz * dz) < 1e-6F) {
        return 0.0F;
    }
    return wrap(atan2f(-dx, -dz) - d->yaw);
}

static float distance(const EngDragonFlight *d, const float goal[2])
{
    return hypotf(goal[0] - d->pos[0], goal[1] - d->pos[1]);
}

/* vire vers `turn` (rad, signé) sans dépasser `rate` rad/s, penche, puis avance à sa vitesse */
static void fly(EngDragonFlight *d, float turn, float rate)
{
    const float max_turn = rate * DT;
    const float applied = turn > max_turn ? max_turn : (turn < -max_turn ? -max_turn : turn);
    d->yaw = wrap(d->yaw + applied);
    d->bank += ((-0.9F * applied / (ENG_DRAGON_TURN_RATE * DT)) - d->bank) * 0.06F;
    float f[2];
    forward(d, f);
    d->pos[0] += f[0] * d->speed * DT;
    d->pos[1] += f[1] * d->speed * DT;
}

static float turn_radius(void)
{
    return ENG_DRAGON_SPEED / ENG_DRAGON_TURN_RATE;
}

/* vole vers `goal`. Si le but est À L'INTÉRIEUR de son cercle de virage (du côté où il devrait
 * tourner), virer ne l'y mènerait jamais : il prend de l'élan, droit devant, jusqu'à en être à
 * `release` mètres (au moins deux rayons de virage : le but est alors hors de ses deux cercles),
 * puis revient. Sans cette mémoire (`extending`), il tournerait sans fin autour de son but. */
static void fly_to(EngDragonFlight *d, const float goal[2], float release)
{
    const float b = bearing(d, goal);
    const float side = b >= 0.0F ? 1.0F : -1.0F;
    float f[2];
    forward(d, f);
    /* un virage positif (cap croissant) tourne l'avant vers (f.z, -f.x) */
    const float cx = d->pos[0] + (side * turn_radius() * f[1]);
    const float cz = d->pos[1] - (side * turn_radius() * f[0]);
    if (hypotf(goal[0] - cx, goal[1] - cz) < turn_radius() * 1.05F) {
        d->extending = true;
    } else if (distance(d, goal) >= release) {
        d->extending = false;
    }
    fly(d, d->extending ? 0.0F : b, ENG_DRAGON_TURN_RATE);
}

static void reach_altitude(EngDragonFlight *d, float wanted)
{
    const float step = ENG_DRAGON_CLIMB_RATE * DT;
    const float gap = wanted - d->altitude;
    d->altitude += gap > step ? step : (gap < -step ? -step : gap);
}

static void enter(EngDragonFlight *d, EngDragonPhase phase)
{
    d->phase = phase;
    d->ticks = 0;
    d->flame = false;
    d->aim_locked = false;
    d->extending = false;
}

void eng_dragon_flight_init(EngDragonFlight *d, const float pos[2], float yaw)
{
    if (d == NULL) {
        return;
    }
    *d = (EngDragonFlight){.phase = ENG_DRAGON_GROUND};
    if (pos != NULL && finite2(pos)) {
        d->pos[0] = pos[0];
        d->pos[1] = pos[1];
    }
    d->yaw = isfinite(yaw) ? wrap(yaw) : 0.0F;
}

bool eng_dragon_flight_airborne(const EngDragonFlight *d)
{
    return d != NULL && d->phase != ENG_DRAGON_GROUND;
}

static void step_ground(EngDragonFlight *d, const EngDragonFlightInput *in, bool has_target)
{
    if (finite2(in->ground) && isfinite(in->ground_yaw)) {
        d->pos[0] = in->ground[0];
        d->pos[1] = in->ground[1];
        d->yaw = wrap(in->ground_yaw);
    }
    d->altitude = 0.0F;
    d->speed = 0.0F;
    d->bank = 0.0F;
    const uint32_t wait = has_target ? ENG_DRAGON_GROUND_FIGHT_TICKS : ENG_DRAGON_REST_TICKS;
    if (in->can_take_off && d->ticks >= wait) {
        enter(d, ENG_DRAGON_TAKEOFF);
        d->passes = 0;
    }
}

static void step_patrol(EngDragonFlight *d, const EngDragonFlightInput *in, bool has_target)
{
    const float angle = atan2f(d->pos[1] - in->home[1], d->pos[0] - in->home[0]) + ORBIT_LEAD;
    const float goal[2] = {in->home[0] + (ENG_DRAGON_ORBIT_RADIUS * cosf(angle)),
                           in->home[1] + (ENG_DRAGON_ORBIT_RADIUS * sinf(angle))};
    fly(d, bearing(d, goal), ENG_DRAGON_TURN_RATE);
    reach_altitude(d, ENG_DRAGON_CRUISE_ALT);
    /* il rentre quand ses passages sont faits ou, sans cible, quand sa ronde a assez duré */
    const uint32_t asked = in->passes == 0U ? ENG_DRAGON_PASSES : in->passes;
    const uint32_t passes = asked > ENG_DRAGON_PASSES_MAX ? ENG_DRAGON_PASSES_MAX : asked;
    const bool done = has_target ? d->passes >= passes : d->ticks >= ENG_DRAGON_PATROL_TICKS;
    if (done) {
        enter(d, ENG_DRAGON_RETURN);
    } else if (has_target && d->ticks >= ENG_DRAGON_PATROL_MIN_TICKS) {
        enter(d, ENG_DRAGON_STRAFE);
    }
}

static void step_strafe(EngDragonFlight *d, const EngDragonFlightInput *in, bool has_target)
{
    const float lead = isfinite(in->fire_lead) && in->fire_lead > 0.0F ? in->fire_lead : 0.0F;
    if (!d->aim_locked) {
        if (!has_target) { /* la cible a disparu : il reprend sa ronde */
            enter(d, ENG_DRAGON_PATROL);
            return;
        }
        if (d->ticks > ENG_DRAGON_STRAFE_TIMEOUT) { /* passage qui n'aboutit pas : abandonné */
            d->passes++;
            enter(d, ENG_DRAGON_PATROL);
            return;
        }
        /* trop près pour que son feu précède sa cible : il prend de l'élan, droit devant, jusqu'à
         * pouvoir faire demi-tour (deux rayons de virage) et revenir dans l'axe */
        if (distance(d, in->target) < lead + LOCK_NEAR) {
            d->extending = true;
        }
        fly_to(d, in->target, lead + LOCK_NEAR + (2.0F * turn_radius()));
        reach_altitude(d, ENG_DRAGON_CRUISE_ALT);
        /* plus près que lead + LOCK_NEAR, il a déjà pris de l'élan (`extending`) */
        if (!d->extending && fabsf(bearing(d, in->target)) < LOCK_BEARING &&
            distance(d, in->target) <= lead + LOCK_FAR) {
            d->aim[0] = in->target[0]; /* axe figé : la cible a le temps de s'en écarter */
            d->aim[1] = in->target[1];
            d->aim_locked = true;
        }
        return;
    }
    /* droit sur le point figé (il y affine son axe : moins de 2 degrés à corriger) */
    fly(d, bearing(d, d->aim), ENG_DRAGON_TURN_RATE);
    reach_altitude(d, ENG_DRAGON_STRAFE_ALT);
    float f[2];
    forward(d, f);
    const float along = ((d->aim[0] - d->pos[0]) * f[0]) + ((d->aim[1] - d->pos[1]) * f[1]);
    const float gap = along - lead; /* > 0 : son feu n'a pas encore atteint le point visé */
    d->flame = gap <= ENG_DRAGON_FIRE_SPAN && gap >= -ENG_DRAGON_FIRE_SPAN;
    if (gap < -ENG_DRAGON_FIRE_SPAN) {
        d->passes++;
        enter(d, ENG_DRAGON_PATROL);
    }
}

static void step_landing(EngDragonFlight *d, const EngDragonFlightInput *in)
{
    const float u = (float)d->ticks / (float)ENG_DRAGON_LANDING_TICKS;
    const float dist = distance(d, in->home);
    const float arrive = 0.5F + (1.2F * dist); /* il ralentit en approchant */
    d->speed = d->speed < arrive ? d->speed : arrive;
    /* tout près du centre, la direction de l'antre n'a plus de sens : il garde son cap */
    fly(d, dist > 4.0F ? bearing(d, in->home) : 0.0F, 2.0F * ENG_DRAGON_TURN_RATE);
    d->altitude = d->from_alt * (1.0F - smooth(u));
    if (d->ticks + LANDING_SETTLE >= ENG_DRAGON_LANDING_TICKS) {
        d->pos[0] += (in->home[0] - d->pos[0]) * 0.1F;
        d->pos[1] += (in->home[1] - d->pos[1]) * 0.1F;
    }
    if (d->ticks >= ENG_DRAGON_LANDING_TICKS) {
        enter(d, ENG_DRAGON_GROUND);
        d->altitude = 0.0F;
        d->speed = 0.0F;
        d->bank = 0.0F;
        d->landed = true;
    }
}

bool eng_dragon_flight_step(EngDragonFlight *d, const EngDragonFlightInput *in)
{
    if (d == NULL || in == NULL) {
        return false;
    }
    d->landed = false;
    if (!finite2(in->home)) {
        return d->phase != ENG_DRAGON_GROUND;
    }
    const bool has_target = in->has_target && finite2(in->target);
    d->ticks++;
    switch (d->phase) {
    case ENG_DRAGON_TAKEOFF: {
        const float u = (float)d->ticks / (float)ENG_DRAGON_TAKEOFF_TICKS;
        d->altitude = ENG_DRAGON_CRUISE_ALT * smooth(u);
        d->speed = ENG_DRAGON_SPEED * (u > 1.0F ? 1.0F : u);
        fly(d, 0.0F, ENG_DRAGON_TURN_RATE);
        if (d->ticks >= ENG_DRAGON_TAKEOFF_TICKS) {
            d->speed = ENG_DRAGON_SPEED;
            enter(d, ENG_DRAGON_PATROL);
        }
        break;
    }
    case ENG_DRAGON_PATROL:
        step_patrol(d, in, has_target);
        break;
    case ENG_DRAGON_STRAFE:
        step_strafe(d, in, has_target);
        break;
    case ENG_DRAGON_RETURN:
        fly_to(d, in->home, 2.2F * turn_radius());
        reach_altitude(d, ENG_DRAGON_CRUISE_ALT);
        if (distance(d, in->home) < LANDING_START &&
            fabsf(bearing(d, in->home)) < LANDING_BEARING) {
            enter(d, ENG_DRAGON_LANDING);
            d->from_alt = d->altitude;
        }
        break;
    case ENG_DRAGON_LANDING:
        step_landing(d, in);
        break;
    case ENG_DRAGON_GROUND:
    default:
        step_ground(d, in, has_target);
        break;
    }
    return d->phase != ENG_DRAGON_GROUND;
}

bool eng_dragon_flight_fall(EngDragonFlight *d)
{
    if (d == NULL || d->phase == ENG_DRAGON_GROUND) {
        return false;
    }
    d->flame = false;
    d->landed = false;
    d->speed *= 0.97F;
    fly(d, 0.0F, ENG_DRAGON_TURN_RATE);
    d->altitude -= FALL_SPEED;
    if (d->altitude > 0.0F) {
        return false;
    }
    enter(d, ENG_DRAGON_GROUND);
    d->altitude = 0.0F;
    d->speed = 0.0F;
    d->bank = 0.0F;
    return true;
}

void eng_dragon_flight_shift(EngDragonFlight *d, float dx, float dz)
{
    if (d == NULL || !isfinite(dx) || !isfinite(dz)) {
        return;
    }
    d->pos[0] += dx;
    d->pos[1] += dz;
    d->aim[0] += dx;
    d->aim[1] += dz;
}

void eng_dragon_flight_fire_point(const EngDragonFlight *d, float lead, float out[2])
{
    if (d == NULL || out == NULL) {
        return;
    }
    float f[2];
    forward(d, f);
    const float l = isfinite(lead) ? lead : 0.0F;
    out[0] = d->pos[0] + (f[0] * l);
    out[1] = d->pos[1] + (f[1] * l);
}

const char *eng_dragon_phase_name(EngDragonPhase phase)
{
    switch (phase) {
    case ENG_DRAGON_GROUND:
        return "au sol";
    case ENG_DRAGON_TAKEOFF:
        return "décollage";
    case ENG_DRAGON_PATROL:
        return "ronde";
    case ENG_DRAGON_STRAFE:
        return "passage de feu";
    case ENG_DRAGON_RETURN:
        return "retour";
    case ENG_DRAGON_LANDING:
        return "atterrissage";
    default:
        return "?";
    }
}
