#include "engine/gpu.h"

#include <SDL3/SDL.h>

#include "engine/gpu_debug.h"
#include "engine/log.h"

static SDL_GPUPresentMode to_sdl(EngPresentMode mode)
{
    switch (mode) {
    case ENG_PRESENT_MAILBOX:
        return SDL_GPU_PRESENTMODE_MAILBOX;
    case ENG_PRESENT_IMMEDIATE:
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    case ENG_PRESENT_VSYNC:
    default:
        return SDL_GPU_PRESENTMODE_VSYNC;
    }
}

bool eng_gpu_init(EngGpu *gpu, SDL_Window *window, const EngGpuDesc *desc)
{
    *gpu = (EngGpu){0};
    gpu->window = window;
    gpu->clear_color[0] = 0.094F; /* même fond que l'affichage logiciel provisoire */
    gpu->clear_color[1] = 0.110F;
    gpu->clear_color[2] = 0.157F;
    gpu->clear_color[3] = 1.0F;

    /* On annonce tous les formats de shaders que nos outils produiront (M1-02) : SDL choisit
     * alors le pilote de la plateforme (Vulkan, D3D12, Metal). */
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        ENG_LOG_ERROR("GPU : SDL_CreateProperties a échoué : %s", SDL_GetError());
        return false;
    }
    (void)SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, desc->debug);
    (void)SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    (void)SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    (void)SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    gpu->device = SDL_CreateGPUDeviceWithProperties(props);
    SDL_DestroyProperties(props);
    if (gpu->device == NULL) {
        ENG_LOG_WARN("GPU : aucun périphérique utilisable : %s", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu->device, window)) {
        ENG_LOG_ERROR("GPU : rattachement à la fenêtre impossible : %s", SDL_GetError());
        SDL_DestroyGPUDevice(gpu->device);
        *gpu = (EngGpu){0};
        return false;
    }

    /* images en vol : SDL en autorise 2 par défaut ; on annonce notre choix pour que les deux
     * décomptes (celui de SDL et nos barrières) coïncident */
    gpu->frames_in_flight = ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT;
    if (desc->frames_in_flight >= 1U && desc->frames_in_flight <= ENG_GPU_MAX_FRAMES_IN_FLIGHT) {
        gpu->frames_in_flight = desc->frames_in_flight;
    }
    if (!SDL_SetGPUAllowedFramesInFlight(gpu->device, gpu->frames_in_flight)) {
        ENG_LOG_WARN("GPU : %u image(s) en vol refusée(s) (%s) : valeur du pilote conservée",
                     (unsigned)gpu->frames_in_flight, SDL_GetError());
        gpu->frames_in_flight = ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT;
    }
    gpu->present_mode = ENG_PRESENT_VSYNC; /* mode par défaut de SDL après le rattachement */
    if (desc->present_mode != ENG_PRESENT_VSYNC &&
        !eng_gpu_set_present_mode(gpu, desc->present_mode)) {
        ENG_LOG_WARN("GPU : mode de présentation %d indisponible : repli sur la synchronisation "
                     "verticale",
                     (int)desc->present_mode);
    }
    ENG_LOG_INFO("GPU : pilote %s, périphérique « %s », présentation %s%s",
                 SDL_GetGPUDeviceDriver(gpu->device),
                 SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu->device),
                                       SDL_PROP_GPU_DEVICE_NAME_STRING, "inconnu"),
                 gpu->present_mode == ENG_PRESENT_VSYNC
                     ? "vsync"
                     : (gpu->present_mode == ENG_PRESENT_MAILBOX ? "mailbox" : "immédiate"),
                 desc->debug ? ", validation demandée" : "");
    ENG_LOG_INFO("GPU : %u image(s) en vol au plus", (unsigned)gpu->frames_in_flight);
    return true;
}

/* ── Barrières ── */

/* Rend la barrière la plus ancienne (sans attendre) et décale les suivantes. */
static void pop_fence(EngGpu *gpu)
{
    SDL_ReleaseGPUFence(gpu->device, gpu->fences[0]);
    gpu->fences_released++;
    for (uint32_t i = 1; i < gpu->fence_count; i++) {
        gpu->fences[i - 1U] = gpu->fences[i];
    }
    gpu->fence_count--;
    gpu->fences[gpu->fence_count] = NULL;
}

static bool fence_done(EngGpu *gpu, SDL_GPUFence *fence)
{
    return gpu->fence_query != NULL ? gpu->fence_query(gpu->fence_user, fence)
                                    : SDL_QueryGPUFence(gpu->device, fence);
}

/* Rend, sans attendre, les barrières des images que le GPU a finies (dans l'ordre : on s'arrête à
 * la première qui ne l'est pas). */
static void retire_done_fences(EngGpu *gpu)
{
    while (gpu->fence_count > 0 && fence_done(gpu, gpu->fences[0])) {
        pop_fence(gpu);
    }
}

/* Attend la plus ancienne image en vol puis rend sa barrière (même si l'attente échoue : on ne
 * reste pas bloqué sur une barrière que le pilote ne signale plus). */
static void wait_oldest_fence(EngGpu *gpu)
{
    const bool waited = gpu->fence_wait != NULL
                            ? gpu->fence_wait(gpu->fence_user, gpu->fences[0])
                            : SDL_WaitForGPUFences(gpu->device, true, &gpu->fences[0], 1);
    if (!waited) {
        ENG_LOG_ERROR("GPU : attente d'une image impossible : %s", SDL_GetError());
    }
    gpu->fence_waits++;
    pop_fence(gpu);
}

/* Rend toutes les barrières sans attendre : réservé aux cas où le GPU est déjà inactif. */
static void drop_all_fences(EngGpu *gpu)
{
    while (gpu->fence_count > 0) {
        pop_fence(gpu);
    }
}

uint32_t eng_gpu_frames_in_flight(const EngGpu *gpu)
{
    return gpu->device != NULL ? gpu->frames_in_flight : 0U;
}

bool eng_gpu_set_frames_in_flight(EngGpu *gpu, uint32_t count)
{
    if (gpu->device == NULL || count < 1U || count > ENG_GPU_MAX_FRAMES_IN_FLIGHT ||
        !SDL_SetGPUAllowedFramesInFlight(gpu->device, count)) {
        return false;
    }
    gpu->frames_in_flight = count;
    return true;
}

uint32_t eng_gpu_frames_pending(EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return 0U;
    }
    retire_done_fences(gpu);
    return gpu->fence_count;
}

void eng_gpu_wait_idle(EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return;
    }
    while (gpu->fence_count > 0) {
        wait_oldest_fence(gpu);
    }
}

bool eng_gpu_resources_balanced(const EngGpu *gpu)
{
    const uint64_t alive =
        (gpu->depth_texture != NULL ? 1U : 0U) + (gpu->hdr_texture != NULL ? 1U : 0U);
    return gpu->fences_acquired == gpu->fences_released + (uint64_t)gpu->fence_count &&
           gpu->targets_created == gpu->targets_released + alive;
}

void eng_gpu_set_fence_hooks(EngGpu *gpu, bool (*query)(void *user, struct SDL_GPUFence *fence),
                             bool (*wait)(void *user, struct SDL_GPUFence *fence), void *user)
{
    gpu->fence_query = query;
    gpu->fence_wait = wait;
    gpu->fence_user = user;
}

void eng_gpu_set_resize_callback(EngGpu *gpu, EngGpuResizeFn fn, void *user)
{
    gpu->on_resize = fn;
    gpu->on_resize_user = user;
}

bool eng_gpu_shutdown(EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return true;
    }
    (void)SDL_WaitForGPUIdle(gpu->device);
    if (gpu->command_buffer != NULL) {
        (void)SDL_CancelGPUCommandBuffer(gpu->command_buffer);
    }
    drop_all_fences(gpu); /* le GPU est inactif : toutes les barrières sont signalées */
    if (gpu->depth_texture != NULL) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->depth_texture);
        gpu->targets_released++;
    }
    if (gpu->hdr_texture != NULL) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->hdr_texture);
        gpu->targets_released++;
    }
    SDL_ReleaseWindowFromGPUDevice(gpu->device, gpu->window);
    SDL_DestroyGPUDevice(gpu->device);
    gpu->depth_texture = NULL; /* libérées ci-dessus : le bilan ne les compte plus comme vivantes */
    gpu->hdr_texture = NULL;
    /* à l'arrêt le GPU est inactif : plus aucune barrière ne peut rester « en vol » */
    const bool balanced = eng_gpu_resources_balanced(gpu) && gpu->fence_count == 0;
    if (!balanced) {
        ENG_LOG_ERROR(
            "GPU : fuite à l'arrêt : barrières %llu/%llu rendues, cibles %llu/%llu libérées",
            (unsigned long long)gpu->fences_released, (unsigned long long)gpu->fences_acquired,
            (unsigned long long)gpu->targets_released, (unsigned long long)gpu->targets_created);
    }
    ENG_LOG_INFO("GPU : %llu image(s) présentée(s), %llu ignorée(s)",
                 (unsigned long long)gpu->frames_submitted,
                 (unsigned long long)gpu->frames_skipped);
    ENG_LOG_INFO("GPU : %u image(s) en vol au plus (limite %u), %llu attente(s) du GPU, %llu/%llu "
                 "barrière(s) rendue(s), %llu redimensionnement(s), %llu/%llu cible(s) libérée(s)",
                 (unsigned)gpu->max_pending, (unsigned)gpu->frames_in_flight,
                 (unsigned long long)gpu->fence_waits, (unsigned long long)gpu->fences_released,
                 (unsigned long long)gpu->fences_acquired, (unsigned long long)gpu->resizes,
                 (unsigned long long)gpu->targets_released,
                 (unsigned long long)gpu->targets_created);
    *gpu = (EngGpu){0};
    return balanced;
}

const char *eng_gpu_driver_name(const EngGpu *gpu)
{
    return gpu->device != NULL ? SDL_GetGPUDeviceDriver(gpu->device) : "aucun";
}

bool eng_gpu_set_present_mode(EngGpu *gpu, EngPresentMode mode)
{
    if (gpu->device == NULL ||
        !SDL_WindowSupportsGPUPresentMode(gpu->device, gpu->window, to_sdl(mode))) {
        return false;
    }
    if (!SDL_SetGPUSwapchainParameters(gpu->device, gpu->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       to_sdl(mode))) {
        ENG_LOG_WARN("GPU : changement de mode de présentation refusé : %s", SDL_GetError());
        return false;
    }
    gpu->present_mode = mode;
    return true;
}

void eng_gpu_set_clear_color(EngGpu *gpu, float r, float g, float b, float a)
{
    gpu->clear_color[0] = r;
    gpu->clear_color[1] = g;
    gpu->clear_color[2] = b;
    gpu->clear_color[3] = a;
}

bool eng_gpu_enable_depth(EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return false;
    }
    if (gpu->depth_format != 0) {
        return true;
    }
    static const SDL_GPUTextureFormat candidates[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                                      SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                                      SDL_GPU_TEXTUREFORMAT_D16_UNORM};
    for (size_t i = 0; i < SDL_arraysize(candidates); i++) {
        if (SDL_GPUTextureSupportsFormat(gpu->device, candidates[i], SDL_GPU_TEXTURETYPE_2D,
                                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            gpu->depth_format = (uint32_t)candidates[i];
            return true;
        }
    }
    ENG_LOG_ERROR("GPU : aucun format de profondeur utilisable");
    return false;
}

uint32_t eng_gpu_depth_format(const EngGpu *gpu)
{
    return gpu->depth_format;
}

/* (Re)crée la texture de profondeur à la taille de la swapchain. false en cas d'échec. */
static bool ensure_depth(EngGpu *gpu, uint32_t width, uint32_t height)
{
    if (gpu->depth_format == 0 ||
        (gpu->depth_texture != NULL && gpu->depth_width == width && gpu->depth_height == height)) {
        return true;
    }
    if (gpu->depth_texture != NULL) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->depth_texture); /* libéré une fois inutilisé */
        gpu->depth_texture = NULL;
        gpu->targets_released++;
    }
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = (SDL_GPUTextureFormat)gpu->depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    gpu->depth_texture = SDL_CreateGPUTexture(gpu->device, &info);
    eng_gpu_debug_name_texture(gpu->device, gpu->depth_texture, "Depth");
    if (gpu->depth_texture == NULL) {
        ENG_LOG_ERROR("GPU : texture de profondeur %ux%u impossible : %s", (unsigned)width,
                      (unsigned)height, SDL_GetError());
        return false;
    }
    gpu->targets_created++;
    gpu->depth_width = width;
    gpu->depth_height = height;
    return true;
}

bool eng_gpu_enable_hdr(EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return false;
    }
    if (gpu->hdr_enabled) {
        return true;
    }
    if (!SDL_GPUTextureSupportsFormat(
            gpu->device, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
        ENG_LOG_ERROR("GPU : texture RGBA16F (cible et lecture) non supportée");
        return false;
    }
    gpu->hdr_enabled = true;
    return true;
}

void eng_gpu_disable_hdr(EngGpu *gpu)
{
    if (gpu->device != NULL && gpu->hdr_texture != NULL) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->hdr_texture);
        gpu->targets_released++;
    }
    gpu->hdr_texture = NULL;
    gpu->hdr_width = 0;
    gpu->hdr_height = 0;
    gpu->hdr_enabled = false;
    gpu->resolve = NULL;
    gpu->resolve_user = NULL;
}

uint32_t eng_gpu_hdr_format(void)
{
    return (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
}

uint32_t eng_gpu_scene_format(const EngGpu *gpu)
{
    return gpu->hdr_enabled ? eng_gpu_hdr_format() : eng_gpu_swapchain_format(gpu);
}

void eng_gpu_set_resolve(EngGpu *gpu, EngGpuResolveFn fn, void *user)
{
    gpu->resolve = fn;
    gpu->resolve_user = user;
}

/* (Re)crée la texture HDR à la taille de la swapchain. false en cas d'échec. */
static bool ensure_hdr(EngGpu *gpu, uint32_t width, uint32_t height)
{
    if (!gpu->hdr_enabled ||
        (gpu->hdr_texture != NULL && gpu->hdr_width == width && gpu->hdr_height == height)) {
        return true;
    }
    if (gpu->hdr_texture != NULL) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->hdr_texture); /* libérée une fois inutilisée */
        gpu->hdr_texture = NULL;
        gpu->targets_released++;
    }
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    gpu->hdr_texture = SDL_CreateGPUTexture(gpu->device, &info);
    eng_gpu_debug_name_texture(gpu->device, gpu->hdr_texture, "HDR scene color");
    if (gpu->hdr_texture == NULL) {
        ENG_LOG_ERROR("GPU : texture HDR %ux%u impossible : %s", (unsigned)width, (unsigned)height,
                      SDL_GetError());
        return false;
    }
    gpu->targets_created++;
    gpu->hdr_width = width;
    gpu->hdr_height = height;
    return true;
}

bool eng_gpu_begin_frame(EngGpu *gpu)
{
    if (gpu->device == NULL || gpu->command_buffer != NULL) {
        return false; /* arrêté, ou image déjà commencée */
    }
    /* au plus `frames_in_flight` images en vol : si le GPU est en retard, on l'attend ici, avant de
     * préparer une image de plus (sinon la file d'attente du CPU grossit sans limite) */
    retire_done_fences(gpu);
    while (gpu->fence_count >= gpu->frames_in_flight) {
        wait_oldest_fence(gpu);
    }
    gpu->command_buffer = SDL_AcquireGPUCommandBuffer(gpu->device);
    if (gpu->command_buffer == NULL) {
        ENG_LOG_ERROR("GPU : tampon de commandes indisponible : %s", SDL_GetError());
        gpu->frames_skipped++;
        return false;
    }
    SDL_GPUTexture *texture = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(gpu->command_buffer, gpu->window, &texture, &width,
                                               &height)) {
        ENG_LOG_ERROR("GPU : image de la swapchain indisponible : %s", SDL_GetError());
        (void)SDL_CancelGPUCommandBuffer(gpu->command_buffer);
        gpu->command_buffer = NULL;
        gpu->frames_skipped++;
        return false;
    }
    if (texture == NULL) { /* fenêtre réduite : rien à présenter cette fois */
        (void)SDL_SubmitGPUCommandBuffer(gpu->command_buffer);
        gpu->command_buffer = NULL;
        gpu->frames_skipped++;
        return false;
    }
    if (!ensure_depth(gpu, width, height) || !ensure_hdr(gpu, width, height)) {
        (void)SDL_SubmitGPUCommandBuffer(gpu->command_buffer); /* l'image est présentée vide */
        gpu->command_buffer = NULL;
        gpu->frames_skipped++;
        return false;
    }
    gpu->swapchain_texture = texture;
    gpu->swapchain_width = width;
    gpu->swapchain_height = height;
    if (width != gpu->target_width || height != gpu->target_height) {
        gpu->target_width =
            width; /* cibles déjà recréées ci-dessus : on annonce la nouvelle taille */
        gpu->target_height = height;
        gpu->resizes++;
        if (gpu->on_resize != NULL) {
            gpu->on_resize(gpu->on_resize_user, width, height);
        }
    }
    /* Groupe racine de l'image : tout ce qui suit jusqu'à end_frame s'y range dans l'outil. */
    eng_gpu_debug_push_fmt(gpu->command_buffer, ENG_GPU_PASS_FRAME_FORMAT,
                           (unsigned long long)gpu->frames_submitted + 1ULL);
    return true;
}

void eng_gpu_fill_depth_target(struct SDL_GPUTexture *texture, SDL_GPUDepthStencilTargetInfo *out)
{
    SDL_zero(*out);
    out->texture = texture;
    out->clear_depth = 1.0F;
    out->load_op = SDL_GPU_LOADOP_CLEAR;
    out->store_op = SDL_GPU_STOREOP_DONT_CARE; /* la profondeur ne sert que pendant l'image */
    out->stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    out->stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
}

static bool open_pass(EngGpu *gpu, SDL_GPURenderPass **out)
{
    SDL_GPUColorTargetInfo target = {
        .texture = gpu->hdr_texture != NULL ? gpu->hdr_texture : gpu->swapchain_texture,
        .clear_color = {gpu->clear_color[0], gpu->clear_color[1], gpu->clear_color[2],
                        gpu->clear_color[3]},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo depth;
    eng_gpu_fill_depth_target(gpu->depth_texture, &depth);
    eng_gpu_debug_push(gpu->command_buffer, ENG_GPU_PASS_SCENE);
    *out = SDL_BeginGPURenderPass(gpu->command_buffer, &target, 1,
                                  gpu->depth_texture != NULL ? &depth : NULL);
    if (*out == NULL) {
        ENG_LOG_ERROR("GPU : passe de rendu impossible : %s", SDL_GetError());
        eng_gpu_debug_pop(gpu->command_buffer);
    }
    return *out != NULL;
}

void eng_gpu_fill_resolve_target(struct SDL_GPUTexture *texture, SDL_GPUColorTargetInfo *out)
{
    SDL_zero(*out);
    out->texture = texture;
    out->clear_color = (SDL_FColor){0.0F, 0.0F, 0.0F, 1.0F};
    out->load_op = SDL_GPU_LOADOP_CLEAR; /* le plein écran écrase tout ; sans résolveur : noir */
    out->store_op = SDL_GPU_STOREOP_STORE;
}

/* Termine la passe principale ; avec le HDR, résout ensuite la texture HDR dans la swapchain. */
static void end_main_pass(EngGpu *gpu, SDL_GPURenderPass *main_pass)
{
    SDL_EndGPURenderPass(main_pass);
    eng_gpu_debug_pop(gpu->command_buffer); /* « Scene » */
    if (gpu->hdr_texture == NULL) {
        return;
    }
    SDL_GPUColorTargetInfo target;
    eng_gpu_fill_resolve_target(gpu->swapchain_texture, &target);
    eng_gpu_debug_push(gpu->command_buffer, ENG_GPU_PASS_TONEMAP);
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(gpu->command_buffer, &target, 1, NULL);
    if (pass == NULL) {
        ENG_LOG_ERROR("GPU : passe de résolution HDR impossible : %s", SDL_GetError());
        eng_gpu_debug_pop(gpu->command_buffer);
        return;
    }
    if (gpu->resolve != NULL) {
        gpu->resolve(gpu->resolve_user, gpu->command_buffer, pass, gpu->hdr_texture);
    }
    SDL_EndGPURenderPass(pass);
    eng_gpu_debug_pop(gpu->command_buffer); /* « Tonemap » */
    gpu->resolve_passes++;
}

SDL_GPURenderPass *eng_gpu_begin_render_pass(EngGpu *gpu)
{
    if (gpu->command_buffer == NULL || gpu->swapchain_texture == NULL || gpu->render_pass != NULL) {
        return NULL;
    }
    (void)open_pass(gpu, &gpu->render_pass);
    return gpu->render_pass;
}

void eng_gpu_fill_overlay_target(struct SDL_GPUTexture *texture, SDL_GPUColorTargetInfo *out)
{
    SDL_zero(*out);
    out->texture = texture;
    out->load_op = SDL_GPU_LOADOP_LOAD; /* la scène déjà dessinée reste */
    out->store_op = SDL_GPU_STOREOP_STORE;
}

SDL_GPURenderPass *eng_gpu_begin_overlay_pass(EngGpu *gpu)
{
    if (gpu->command_buffer == NULL || gpu->swapchain_texture == NULL || gpu->overlay_open) {
        return NULL;
    }
    if (!gpu->main_pass_ended) {
        SDL_GPURenderPass *main_pass = gpu->render_pass;
        if (main_pass == NULL &&
            !open_pass(gpu, &main_pass)) { /* rien dessiné : on efface d'abord */
            return NULL;
        }
        end_main_pass(gpu, main_pass);
        gpu->render_pass = NULL;
        gpu->main_pass_ended = true; /* si l'overlay échoue, end_frame ne doit pas ré-effacer */
    }
    SDL_GPUColorTargetInfo target;
    eng_gpu_fill_overlay_target(gpu->swapchain_texture, &target);
    eng_gpu_debug_push(gpu->command_buffer, ENG_GPU_PASS_OVERLAY);
    gpu->render_pass = SDL_BeginGPURenderPass(gpu->command_buffer, &target, 1, NULL);
    if (gpu->render_pass == NULL) {
        ENG_LOG_ERROR("GPU : passe overlay impossible : %s", SDL_GetError());
        eng_gpu_debug_pop(gpu->command_buffer);
        return NULL;
    }
    gpu->overlay_open = true;
    gpu->overlay_passes++;
    return gpu->render_pass;
}

uint32_t eng_gpu_swapchain_format(const EngGpu *gpu)
{
    if (gpu->device == NULL) {
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
    return (uint32_t)SDL_GetGPUSwapchainTextureFormat(gpu->device, gpu->window);
}

void eng_gpu_end_frame(EngGpu *gpu)
{
    if (gpu->command_buffer == NULL || gpu->swapchain_texture == NULL) {
        return;
    }
    SDL_GPURenderPass *pass = gpu->render_pass;
    if (pass == NULL && !gpu->main_pass_ended) {
        (void)open_pass(gpu, &pass); /* personne n'a dessiné : on efface seulement */
    }
    if (pass != NULL) {
        if (gpu->overlay_open) {
            SDL_EndGPURenderPass(pass);
            eng_gpu_debug_pop(gpu->command_buffer); /* « Overlay » */
        } else {
            end_main_pass(gpu, pass); /* personne n'a ouvert l'overlay : la principale finit ici */
        }
    }
    gpu->render_pass = NULL;
    gpu->overlay_open = false;
    gpu->main_pass_ended = false;
    eng_gpu_debug_pop(gpu->command_buffer); /* « Frame N » : fermé avant la soumission */
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(gpu->command_buffer);
    if (fence != NULL) {
        gpu->frames_submitted++;
        gpu->fences_acquired++;
        while (gpu->fence_count >= ENG_GPU_MAX_FRAMES_IN_FLIGHT) {
            wait_oldest_fence(gpu); /* ne peut arriver que si begin_frame a été contourné */
        }
        gpu->fences[gpu->fence_count++] = fence;
        if (gpu->fence_count > gpu->max_pending) {
            gpu->max_pending = gpu->fence_count;
        }
    } else {
        ENG_LOG_ERROR("GPU : soumission refusée : %s", SDL_GetError());
        gpu->frames_skipped++;
    }
    gpu->command_buffer = NULL;
    gpu->swapchain_texture = NULL;
}
