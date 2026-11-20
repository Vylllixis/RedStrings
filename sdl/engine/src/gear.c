#include "engine/gear.h"

#include <stddef.h>

static const float DAMAGE[ENG_GEAR_TIERS] = {1.0F, 1.35F, 1.8F, 2.4F, 3.2F};
static const float ARMOR[ENG_GEAR_TIERS] = {1.0F, 0.8F, 0.65F, 0.5F, 0.4F};
static const float BLOCK[ENG_GEAR_TIERS] = {0.2F, 0.15F, 0.1F, 0.06F, 0.03F};
static const char *const TIER_NAMES[ENG_GEAR_TIERS] = {"de départ", "du chevalier", "du givre",
                                                       "des sables", "du dragon"};
static const char *const NAMES[ENG_GEAR_TIERS][ENG_GEAR_SLOTS] = {
    {"Épée de départ", "Armure de départ", "Écu de départ"},
    {"Épée du chevalier", "Armure du chevalier", "Écu du chevalier"},
    {"Lame du givre", "Armure du givre", "Écu du givre"},
    {"Sabre des sables", "Armure des sables", "Écu des sables"},
    {"Lame du dragon", "Armure du dragon", "Écu du dragon"},
};

uint32_t eng_gear_clamp(uint32_t tier)
{
    return tier < ENG_GEAR_TIERS ? tier : ENG_GEAR_TIERS - 1U;
}

float eng_gear_damage(uint32_t tier)
{
    return DAMAGE[eng_gear_clamp(tier)];
}

float eng_gear_armor(uint32_t tier)
{
    return ARMOR[eng_gear_clamp(tier)];
}

float eng_gear_block(uint32_t tier)
{
    return BLOCK[eng_gear_clamp(tier)];
}

const char *eng_gear_name(uint32_t tier, EngGearSlot slot)
{
    if ((uint32_t)slot >= (uint32_t)ENG_GEAR_SLOTS) {
        return "?";
    }
    return NAMES[eng_gear_clamp(tier)][slot];
}

const char *eng_gear_tier_name(uint32_t tier)
{
    return TIER_NAMES[eng_gear_clamp(tier)];
}

bool eng_gear_give(uint32_t *tier, uint32_t reward)
{
    if (tier == NULL) {
        return false;
    }
    const uint32_t r = eng_gear_clamp(reward);
    if (r <= *tier) {
        return false;
    }
    *tier = r;
    return true;
}
