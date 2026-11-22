#include "engine/combat_state.h"

#include <stddef.h>

#include "engine/assert.h"

#define S(x) (1U << (x))

/* Pour chaque état, le masque des états où l'on peut aller (voir docs/combat-design.md §5). */
static const uint32_t LEGAL[ENG_CS_COUNT] = {
    [ENG_CS_IDLE] = S(ENG_CS_MOVE) | S(ENG_CS_ATTACK) | S(ENG_CS_DODGE) | S(ENG_CS_PARRY) |
                    S(ENG_CS_HITSTUN) | S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_MOVE] = S(ENG_CS_IDLE) | S(ENG_CS_ATTACK) | S(ENG_CS_DODGE) | S(ENG_CS_PARRY) |
                    S(ENG_CS_HITSTUN) | S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_ATTACK] = S(ENG_CS_IDLE) | S(ENG_CS_ATTACK) | S(ENG_CS_DODGE) | S(ENG_CS_HITSTUN) |
                      S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_DODGE] = S(ENG_CS_IDLE) | S(ENG_CS_HITSTUN) | S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_PARRY] =
        S(ENG_CS_BLOCK) | S(ENG_CS_IDLE) | S(ENG_CS_HITSTUN) | S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_BLOCK] = S(ENG_CS_IDLE) | S(ENG_CS_ATTACK) | S(ENG_CS_DODGE) | S(ENG_CS_HITSTUN) |
                     S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_HITSTUN] = S(ENG_CS_IDLE) | S(ENG_CS_STAGGER) | S(ENG_CS_DEAD),
    [ENG_CS_STAGGER] = S(ENG_CS_IDLE) | S(ENG_CS_DEAD),
    [ENG_CS_DEAD] = 0U,
};

static const char *const NAMES[ENG_CS_COUNT] = {"idle",  "move",    "attack",  "dodge", "block",
                                                "parry", "hitstun", "stagger", "dead"};

const char *eng_combat_state_name(EngCombatState s)
{
    return ((unsigned)s < (unsigned)ENG_CS_COUNT) ? NAMES[s] : "?";
}

uint32_t eng_combat_legal_mask(EngCombatState from)
{
    return ((unsigned)from < (unsigned)ENG_CS_COUNT) ? LEGAL[from] : 0U;
}

bool eng_combat_legal(EngCombatState from, EngCombatState to)
{
    if ((unsigned)from >= (unsigned)ENG_CS_COUNT || (unsigned)to >= (unsigned)ENG_CS_COUNT) {
        return false;
    }
    return (LEGAL[from] & S(to)) != 0U;
}

void eng_fsm_init(EngCombatFsm *f)
{
    ENG_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->state = ENG_CS_IDLE;
    f->previous = ENG_CS_IDLE;
    f->ticks = 0;
    f->transitions = 0;
    f->refused = 0;
}

bool eng_fsm_request(EngCombatFsm *f, EngCombatState to)
{
    ENG_ASSERT(f != NULL);
    if (f == NULL) {
        return false;
    }
    if (to == f->state && to != ENG_CS_ATTACK) {
        return (unsigned)to < (unsigned)ENG_CS_COUNT;
    }
    if (!eng_combat_legal(f->state, to)) {
        f->refused++;
        ENG_ASSERT_MSG(false, "transition de combat illegale : %s -> %s",
                       eng_combat_state_name(f->state), eng_combat_state_name(to));
        return false;
    }
    f->previous = f->state;
    f->state = to;
    f->ticks = 0;
    f->transitions++;
    return true;
}

void eng_fsm_tick(EngCombatFsm *f)
{
    ENG_ASSERT(f != NULL);
    if (f != NULL && f->ticks < UINT32_MAX) {
        f->ticks++;
    }
}
