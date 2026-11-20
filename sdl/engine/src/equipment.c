#include "engine/equipment.h"

#include <stddef.h>

static const EngWeaponDef WEAPONS[ENG_WEAPON_COUNT] = {
    [ENG_WEAPON_SWORD] = {"Épée et bouclier", "light1", "heavy", false},
    [ENG_WEAPON_GREATSWORD] = {"Espadon", "gs_light1", "gs_heavy", true},
};

#define KNOWN_MASK ((1U << ENG_WEAPON_COUNT) - 1U)

void eng_equip_init(EngEquipment *e)
{
    if (e != NULL) {
        e->owned = 1U << ENG_WEAPON_SWORD;
        e->equipped = ENG_WEAPON_SWORD;
    }
}

const EngWeaponDef *eng_weapon_def(uint32_t id)
{
    return id < ENG_WEAPON_COUNT ? &WEAPONS[id] : NULL;
}

bool eng_equip_owns(const EngEquipment *e, uint32_t id)
{
    return e != NULL && id < ENG_WEAPON_COUNT && (e->owned & (1U << id)) != 0U;
}

bool eng_equip_give(EngEquipment *e, uint32_t id)
{
    if (e == NULL || id >= ENG_WEAPON_COUNT || eng_equip_owns(e, id)) {
        return false;
    }
    e->owned |= 1U << id;
    return true;
}

bool eng_equip_select(EngEquipment *e, uint32_t id)
{
    if (!eng_equip_owns(e, id)) {
        return false;
    }
    e->equipped = id;
    return true;
}

uint32_t eng_equip_next(EngEquipment *e)
{
    if (e == NULL || !eng_equip_valid(e)) {
        return ENG_WEAPON_SWORD;
    }
    for (uint32_t k = 1; k <= ENG_WEAPON_COUNT; k++) {
        const uint32_t id = (e->equipped + k) % ENG_WEAPON_COUNT;
        if (eng_equip_owns(e, id)) {
            e->equipped = id;
            break;
        }
    }
    return e->equipped;
}

bool eng_equip_valid(const EngEquipment *e)
{
    return e != NULL && e->owned != 0U && (e->owned & ~KNOWN_MASK) == 0U &&
           eng_equip_owns(e, e->equipped);
}
