#include "skeleton_view.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"

bool skeleton_view_load(SkeletonView *view, const char *path)
{
    memset(view, 0, sizeof *view);
    EngArena scratch;
    if (!eng_arena_init(&view->arena, "skeleton-view", (size_t)1 << 22) ||
        !eng_arena_init(&scratch, "skeleton-view-scratch", (size_t)1 << 20)) {
        eng_arena_shutdown(&view->arena);
        ENG_LOG_ERROR("squelette : mémoire insuffisante");
        return false;
    }
    const bool ok = eng_skeleton_load(&view->arena, &scratch, path, &view->skeleton) &&
                    eng_skeleton_rest_world(&view->skeleton, view->rest);
    eng_arena_shutdown(&scratch);
    if (!ok) {
        eng_arena_shutdown(&view->arena);
        memset(view, 0, sizeof *view);
        return false;
    }
    view->mismatch = eng_skeleton_bind_mismatch(&view->skeleton);
    view->ready = true;
    ENG_LOG_INFO("squelette : %s, %u articulations, écart pose de liaison / pose repos %.4f m",
                 path, (unsigned)view->skeleton.joint_count, (double)view->mismatch);
    return true;
}

bool skeleton_view_load_anim(SkeletonView *view, const char *skel_path, const char *anim_path)
{
    if (skel_path == NULL) {
        /* le squelette est celui que désigne le clip : on lit d'abord le clip dans une arène
         * jetable */
        EngArena a;
        EngArena s;
        EngAnimClip probe;
        if (!eng_arena_init(&a, "anim-probe", (size_t)1 << 22) ||
            !eng_arena_init(&s, "anim-probe-scratch", (size_t)1 << 20)) {
            eng_arena_shutdown(&a);
            return false;
        }
        char target[512];
        target[0] = '\0';
        if (eng_anim_load(&a, &s, anim_path, &probe) && probe.skeleton_path != NULL) {
            (void)snprintf(target, sizeof target, "%s", probe.skeleton_path);
        }
        eng_arena_shutdown(&s);
        eng_arena_shutdown(&a);
        if (target[0] == '\0' || !skeleton_view_load(view, target)) {
            ENG_LOG_ERROR("animation : squelette introuvable (donnez --skeleton)");
            return false;
        }
    } else if (!view->ready) {
        return false;
    }
    EngArena scratch;
    if (!eng_arena_init(&scratch, "anim-scratch", (size_t)1 << 20)) {
        return false;
    }
    const bool ok = eng_anim_load(&view->arena, &scratch, anim_path, &view->clip) &&
                    eng_anim_debug_init(&view->anim, &view->arena, &view->skeleton, &view->clip);
    eng_arena_shutdown(&scratch);
    if (!ok) {
        ENG_LOG_ERROR("animation : %s ne va pas avec ce squelette", anim_path);
        return false;
    }
    view->animated = true;
    ENG_LOG_INFO("animation : %s, %.2f s, %u canaux. Raccourcis : P pause, , . image, J K "
                 "articulation, T trajectoire",
                 anim_path, (double)view->clip.duration, (unsigned)view->clip.channel_count);
    return true;
}

bool skeleton_view_load_sockets(SkeletonView *view, const char *path)
{
    if (!view->ready) {
        return false;
    }
    EngArena scratch;
    if (!eng_arena_init(&scratch, "sockets-scratch", (size_t)1 << 16)) {
        return false;
    }
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(&scratch, path, &file)) {
        /* le texte doit se terminer par zéro : on le copie dans l'arène */
        char *text = eng_arena_alloc(&scratch, file.size + 1U, 1);
        char error[160];
        if (text != NULL) {
            memcpy(text, file.data, file.size);
            text[file.size] = '\0';
            ok = eng_socketset_parse(text, &view->skeleton, &view->sockets, error, sizeof error);
            if (!ok) {
                ENG_LOG_ERROR("sockets : %s : %s", path, error);
            }
        }
    }
    eng_arena_shutdown(&scratch);
    if (ok) {
        ENG_LOG_INFO("sockets : %s, %u points d'attache", path, (unsigned)view->sockets.count);
    }
    return ok;
}

void skeleton_view_update(SkeletonView *view, float dt, bool keyboard_free)
{
    if (!view->animated) {
        return;
    }
    static const SDL_Scancode keys[6] = {SDL_SCANCODE_P, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD,
                                         SDL_SCANCODE_J, SDL_SCANCODE_K,     SDL_SCANCODE_T};
    bool down[6] = {false, false, false, false, false, false};
    if (keyboard_free) {
        const bool *state = SDL_GetKeyboardState(NULL);
        if (state != NULL) {
            for (int i = 0; i < 6; i++) {
                down[i] = state[keys[i]];
            }
        }
    }
    EngAnimDebug *a = &view->anim;
    const int32_t n = (int32_t)view->skeleton.joint_count;
    if (down[0] && !view->prev_keys[0]) {
        a->paused = !a->paused;
    }
    if (down[1] && !view->prev_keys[1]) {
        (void)eng_anim_debug_step(a, -1);
    }
    if (down[2] && !view->prev_keys[2]) {
        (void)eng_anim_debug_step(a, 1);
    }
    if (down[3] && !view->prev_keys[3]) {
        a->selected = a->selected <= -1 ? n - 1 : a->selected - 1;
    }
    if (down[4] && !view->prev_keys[4]) {
        a->selected = a->selected >= n - 1 ? -1 : a->selected + 1;
    }
    if (down[5] && !view->prev_keys[5]) {
        a->show_trail = !a->show_trail;
    }
    memcpy(view->prev_keys, down, sizeof down);
    (void)eng_anim_debug_update(a, dt);
    (void)eng_anim_debug_describe(a, view->status, sizeof view->status);
}

void skeleton_view_shutdown(SkeletonView *view)
{
    if (view->ready) {
        eng_arena_shutdown(&view->arena);
    }
    memset(view, 0, sizeof *view);
}

uint32_t skeleton_view_draw(SkeletonView *view, EngDebugLines *lines, mat4 model)
{
    if (!view->ready) {
        return 0;
    }
    if (view->animated) {
        const uint32_t bones = eng_anim_debug_draw(&view->anim, model, lines);
        (void)eng_socketset_debug_draw(&view->sockets, view->anim.world, view->skeleton.joint_count,
                                       model, lines, 0.12F);
        return bones;
    }
    eng_debug_lines_set_overlay(lines, true);
    const uint32_t bones =
        eng_skeleton_debug_draw(&view->skeleton, view->rest, model, lines, ENG_DEBUG_CYAN);
    eng_debug_lines_set_overlay(lines, false);
    (void)eng_socketset_debug_draw(&view->sockets, view->rest, view->skeleton.joint_count, model,
                                   lines, 0.12F);
    return bones;
}
