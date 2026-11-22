#include "creature_parts.h"

/* Le dragon (voir engine/creature.h). Une pose est d'abord RÉSOLUE (tête, pieds, angles), puis les
 * boîtes sont posées autour : les repères donnés au jeu (tête, gueule, pieds) et le dessin viennent
 * ainsi des mêmes nombres. */

#define PI_F 3.14159265F
#define FLAME_BOXES 5

typedef struct Rig {
    float t;
    float open; /* gueule */
    float rear; /* cabré */
    float fire; /* jet */
    float aim;  /* plongée du jet */
    float fly;  /* vol */
    float breathe;
    float sway;
    float head[3];
    float neck[3];   /* base du cou */
    float paw[4][3]; /* dessous de chaque pied */
} Rig;

static float mix(float a, float b, float t)
{
    return a + ((b - a) * t);
}

static void resolve(const EngDragonPose *pose, Rig *r)
{
    static const EngDragonPose rest = {0};
    const EngDragonPose *p = pose != NULL ? pose : &rest;
    r->t = finite_or_zero(p->time);
    r->open = unit(p->bite);
    r->fire = unit(p->flame);
    r->fly = unit(p->flight);
    r->rear = unit(p->rear) * (1.0F - r->fly); /* en vol, il ne se cabre pas */
    const float aim = finite_or_zero(p->aim);
    r->aim = aim < 0.0F ? 0.0F : (aim > 1.4F ? 1.4F : aim);
    r->breathe = 0.06F * sinf(1.6F * r->t);
    r->sway = 0.25F * sinf(1.1F * r->t) * (1.0F - (0.6F * r->fly)); /* queue et cou */
    /* tête : dressée au repos ; elle descend au sol pour mordre, là où `lunge` le demande (de ses
     * pieds au bout de son cou) ; en vol le cou se tend */
    const float ground_y = 3.4F + r->breathe - (2.9F * r->open) + (0.6F * r->rear);
    const float bite_z =
        -(ENG_DRAGON_BITE_NEAR + ((ENG_DRAGON_BITE_FAR - ENG_DRAGON_BITE_NEAR) * unit(p->lunge)));
    const float ground_z = mix(-3.9F, bite_z, r->open);
    r->head[0] = r->sway;
    r->head[1] = mix(ground_y, 2.5F + r->breathe - (0.5F * r->open), r->fly);
    r->head[2] = mix(ground_z, -4.6F, r->fly);
    r->neck[0] = 0.0F;
    r->neck[1] = 2.4F + r->breathe + (0.35F * r->rear);
    r->neck[2] = -2.1F;
    /* pieds : le pas lève les pattes en diagonale ; cabré, les pattes avant montent ; en vol,
     * elles sont repliées sous le ventre */
    static const float px[4] = {-0.8F, 0.8F, -0.8F, 0.8F};
    static const float pz[4] = {-1.3F, -1.3F, 1.4F, 1.4F};
    const float stride = unit(p->stride) * (1.0F - r->fly);
    const float walk = finite_or_zero(p->walk);
    for (int i = 0; i < 4; i++) {
        const float phase = walk + ((i == 0 || i == 3) ? 0.0F : PI_F);
        const float step = sinf(phase);
        float lift = stride * 0.35F * (step > 0.0F ? step : 0.0F);
        if (i < 2) {
            lift += 0.9F * r->rear;
        }
        r->paw[i][0] = px[i];
        r->paw[i][1] = mix(lift, 0.75F, r->fly);
        r->paw[i][2] = mix(pz[i] - (stride * 0.3F * cosf(phase)), pz[i] + 0.5F, r->fly);
    }
}

void eng_creature_dragon_head(const EngDragonPose *pose, float out[3])
{
    if (out == NULL) {
        return;
    }
    Rig r;
    resolve(pose, &r);
    out[0] = r.head[0];
    out[1] = r.head[1];
    out[2] = r.head[2];
}

void eng_creature_dragon_mouth(const EngDragonPose *pose, float out[3])
{
    if (out == NULL) {
        return;
    }
    Rig r;
    resolve(pose, &r);
    out[0] = r.head[0];
    out[1] = r.head[1] - 0.5F;
    out[2] = r.head[2] - 1.0F;
}

void eng_creature_dragon_paw(const EngDragonPose *pose, uint32_t paw, float out[3])
{
    if (out == NULL || paw >= ENG_DRAGON_PAWS) {
        return;
    }
    Rig r;
    resolve(pose, &r);
    out[0] = r.paw[paw][0];
    out[1] = r.paw[paw][1];
    out[2] = r.paw[paw][2];
}

static void body_and_neck(Builder *b, const Rig *r)
{
    const float pitch = 0.1F * r->rear; /* cabré : l'avant se soulève */
    box(b, ENG_CREATURE_RED, 0.0F, 1.9F + r->breathe + (0.15F * r->rear), 0.0F, 0.95F, 0.8F, 2.0F,
        pitch, 0.0F, 0.0F);
    box(b, ENG_CREATURE_ORANGE, 0.0F, 1.25F + r->breathe + (0.15F * r->rear), 0.0F, 0.75F, 0.25F,
        1.8F, pitch, 0.0F, 0.0F);
    for (int i = 0; i < 5; i++) { /* crête d'épines le long du dos */
        const float z = -1.4F + (0.7F * (float)i);
        box(b, ENG_CREATURE_DARK, 0.0F, 2.85F + r->breathe + (0.15F * r->rear) - (0.1F * pitch * z),
            z, 0.07F, 0.24F, 0.2F, pitch - 0.35F, 0.0F, 0.0F);
    }
    /* cou : trois segments de sa base à la tête, alignés sur la direction du cou */
    const float dy = r->head[1] - r->neck[1];
    const float dz = r->head[2] - r->neck[2];
    const float slope = atan2f(dy, -dz);
    for (int i = 0; i < 3; i++) {
        const float u = ((float)i + 0.6F) / 3.4F;
        box(b, ENG_CREATURE_RED, mix(r->neck[0], r->head[0], u), mix(r->neck[1], r->head[1], u),
            mix(r->neck[2], r->head[2], u), 0.42F - (0.05F * (float)i), 0.4F, 0.48F, slope, 0.0F,
            0.0F);
    }
}

static void head(Builder *b, const Rig *r)
{
    const float hx = r->head[0];
    const float hy = r->head[1];
    const float hz = r->head[2];
    const float jaw = 0.15F + (0.55F * (r->open > r->fire ? r->open : r->fire));
    box(b, ENG_CREATURE_RED, hx, hy, hz, 0.5F, 0.34F, 0.85F, 0.0F, 0.0F, 0.0F);
    box(b, ENG_CREATURE_RED, hx, hy - 0.42F - (0.25F * jaw), hz - 0.1F + (0.1F * jaw), 0.42F, 0.12F,
        0.72F, -jaw, 0.0F, 0.0F);
    box(b, ENG_CREATURE_YELLOW, hx - 0.46F, hy + 0.12F, hz - 0.35F, 0.08F, 0.1F, 0.12F, 0.0F, 0.0F,
        0.0F);
    box(b, ENG_CREATURE_YELLOW, hx + 0.46F, hy + 0.12F, hz - 0.35F, 0.08F, 0.1F, 0.12F, 0.0F, 0.0F,
        0.0F);
    box(b, ENG_CREATURE_BONE, hx - 0.3F, hy + 0.55F, hz + 0.55F, 0.08F, 0.35F, 0.08F, -0.6F, 0.0F,
        0.0F);
    box(b, ENG_CREATURE_BONE, hx + 0.3F, hy + 0.55F, hz + 0.55F, 0.08F, 0.35F, 0.08F, -0.6F, 0.0F,
        0.0F);
}

static void tail(Builder *b, const Rig *r)
{
    /* quatre segments de plus en plus fins, qui ondulent ; en vol la queue se tend à l'horizontale
     */
    for (int i = 0; i < 4; i++) {
        const float k = (float)i;
        box(b, ENG_CREATURE_RED, r->sway * 0.5F * (k + 1.0F) * sinf((1.3F * r->t) + k),
            mix(1.7F - (0.28F * k), 1.85F - (0.05F * k), r->fly) + r->breathe, 2.5F + (0.95F * k),
            0.5F - (0.1F * k), 0.42F - (0.08F * k), 0.55F, 0.0F, 0.0F, 0.0F);
    }
    box(b, ENG_CREATURE_BONE, 0.0F, mix(0.75F, 1.65F, r->fly), 6.0F, 0.35F, 0.12F, 0.3F, 0.0F, 0.0F,
        0.0F);
}

/* ailes : trois segments articulés de l'épaule au bout de l'aile. Au sol les deux premiers
 * battent doucement, à demi repliés, et le troisième reste rentré ; en vol l'aile se déploie
 * (160 m d'envergure à l'échelle du jeu) et bat lentement, le bout en retard sur l'épaule. */
static void wings(Builder *b, const Rig *r)
{
    static const float half_len[3] = {1.2F, 1.0F, 0.9F};
    static const float half_chord[3] = {1.0F, 0.8F, 0.6F};
    static const float sweep[3] = {0.1F, 0.4F, 0.75F};
    const float idle = 0.35F + (0.3F * sinf(2.4F * r->t));
    const float beat = 3.0F * r->t;
    const float ground[3] = {idle, idle * 1.5F, idle * 1.5F};
    const float air[3] = {0.12F + (0.5F * sinf(beat)), 0.12F + (0.75F * sinf(beat - 0.6F)),
                          0.12F + (0.95F * sinf(beat - 1.2F))};
    for (int side = -1; side <= 1; side += 2) {
        const float sgn = (float)side;
        float jx = sgn * 0.85F; /* articulation courante : l'épaule d'abord */
        float jy = 2.6F + r->breathe;
        for (int i = 0; i < 3; i++) {
            const float len = i == 2 ? half_len[i] * r->fly : half_len[i];
            if (len < 0.05F) {
                continue; /* bout de l'aile rentré au sol */
            }
            const float a = mix(ground[i], air[i], r->fly);
            const float cx = jx + (sgn * cosf(a) * len);
            const float cy = jy + (sinf(a) * len);
            box(b, ENG_CREATURE_DARK, cx, cy, sweep[i], len, 0.07F - (0.01F * (float)i),
                half_chord[i], 0.0F, 0.0F, sgn * a);
            jx += sgn * cosf(a) * 2.0F * len;
            jy += sinf(a) * 2.0F * len;
        }
    }
}

static void legs(Builder *b, const Rig *r)
{
    /* une patte : le pied suit `paw` ; en vol elle se replie vers l'arrière, plus courte */
    for (int i = 0; i < 4; i++) {
        const float *p = r->paw[i];
        const float half_h = mix(0.7F, 0.45F, r->fly);
        box(b, ENG_CREATURE_RED, p[0], p[1] + half_h, p[2], 0.3F, half_h, 0.32F, -1.0F * r->fly,
            0.0F, 0.0F);
        box(b, ENG_CREATURE_BONE, p[0], p[1] + 0.08F, p[2] - mix(0.35F, -0.3F, r->fly), 0.28F,
            0.08F, 0.2F, 0.0F, 0.0F, 0.0F);
    }
}

/* jet de flammes : cinq boîtes de la gueule vers l'avant, de plus en plus larges, qui plongent de
 * `aim` sous l'horizontale (en vol, il souffle vers le sol) */
static void flames(Builder *b, const Rig *r)
{
    if (!(r->fire > 0.0F)) {
        return;
    }
    const float length = r->fire * ENG_DRAGON_JET;
    const float mouth[3] = {r->head[0], r->head[1] - 0.5F, r->head[2] - 1.0F};
    const float down = sinf(r->aim);
    const float ahead = cosf(r->aim);
    for (int i = 0; i < FLAME_BOXES; i++) {
        const float k = (float)i;
        const float at = length * (k + 0.5F) / (float)FLAME_BOXES;
        const float flick = 0.1F * sinf((17.0F * r->t) + (2.0F * k));
        /* cœur jaune, puis orange : deux couleurs émissives (les écailles rouges ne le sont pas) */
        const EngCreatureColor color = i == 0 ? ENG_CREATURE_YELLOW : ENG_CREATURE_ORANGE;
        box(b, color, mouth[0], mouth[1] - (down * at), mouth[2] - (ahead * at),
            (0.3F + (0.28F * k)) * (1.0F + flick), 0.25F + (0.2F * k),
            0.58F * length / (float)FLAME_BOXES, -r->aim, 0.0F, flick);
    }
}

uint32_t eng_creature_dragon(const EngDragonPose *pose, EngCreaturePart *out, uint32_t max)
{
    Builder b = {out, max, 0};
    Rig r;
    resolve(pose, &r);
    body_and_neck(&b, &r);
    head(&b, &r);
    tail(&b, &r);
    wings(&b, &r);
    legs(&b, &r);
    flames(&b, &r);
    return b.count;
}
