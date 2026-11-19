#include "engine/asset_cook.h"

#include <SDL3/SDL_process.h>
#include <string.h>

#include "engine/log.h"

bool eng_asset_cook_init(EngAssetCook *cook, const EngCookRule *rules, uint32_t count,
                         double interval_seconds)
{
    *cook = (EngAssetCook){0};
    if (count > ENG_COOK_MAX_RULES) {
        ENG_LOG_ERROR("cuisson : %u règles, au plus %u", (unsigned)count,
                      (unsigned)ENG_COOK_MAX_RULES);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        const EngCookRule *rule = &rules[i];
        if (rule->name == NULL || rule->source == NULL || rule->argv == NULL ||
            rule->argv[0] == NULL) {
            ENG_LOG_ERROR("cuisson : règle %u invalide (nom, source et commande obligatoires)",
                          (unsigned)i);
            *cook = (EngAssetCook){0};
            return false;
        }
        cook->rules[i].rule = *rule;
        if (!eng_file_watch_init(&cook->rules[i].watch, rule->source, interval_seconds)) {
            *cook = (EngAssetCook){0};
            return false;
        }
    }
    cook->rule_count = count;
    return true;
}

bool eng_asset_cook_busy(const EngAssetCook *cook)
{
    if (cook->process != NULL) {
        return true;
    }
    for (uint32_t i = 0; i < cook->rule_count; i++) {
        if (cook->rules[i].pending) {
            return true;
        }
    }
    return false;
}

void eng_asset_cook_shutdown(EngAssetCook *cook)
{
    if (cook->process != NULL) {
        (void)SDL_KillProcess(cook->process, true);
        int code = 0;
        (void)SDL_WaitProcess(cook->process, true, &code);
        SDL_DestroyProcess(cook->process);
    }
    *cook = (EngAssetCook){0};
}

uint32_t eng_asset_cook_poll(EngAssetCook *cook, double now_seconds)
{
    uint32_t done = 0;
    for (uint32_t i = 0; i < cook->rule_count; i++) {
        if (eng_file_watch_poll(&cook->rules[i].watch, now_seconds)) {
            cook->rules[i].pending = true;
        }
    }
    if (cook->process != NULL) {
        int code = 0;
        if (SDL_WaitProcess(cook->process, false, &code)) {
            const char *name = cook->rules[cook->running].rule.name;
            if (code == 0) {
                cook->succeeded++;
                done++;
                ENG_LOG_INFO("cuisson « %s » terminée", name);
            } else {
                cook->failed++;
                ENG_LOG_ERROR("cuisson « %s » échouée (code %d)", name, code);
            }
            SDL_DestroyProcess(cook->process);
            cook->process = NULL;
        }
    }
    if (cook->process == NULL) {
        for (uint32_t i = 0; i < cook->rule_count; i++) {
            EngCookState *state = &cook->rules[i];
            if (!state->pending) {
                continue;
            }
            state->pending = false;
            cook->process = SDL_CreateProcess(state->rule.argv, false);
            if (cook->process == NULL) {
                cook->failed++;
                ENG_LOG_ERROR("cuisson « %s » : lancement impossible (%s)", state->rule.name,
                              SDL_GetError());
                continue;
            }
            cook->running = i;
            ENG_LOG_INFO("cuisson « %s » lancée : source modifiée", state->rule.name);
            break;
        }
    }
    return done;
}
