// Couche C++ entre le moteur (C) et les backends de Dear ImGui. Voir ui_backend.h.
// Ce fichier est du C++ « ordinaire » : il suit nos options de compilation mais pas le style C du
// reste du moteur (pas d'exceptions ni de RTTI utilisées).

#include "ui_backend.h"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

extern "C" {
#include "engine/log.h"
}

namespace
{

bool g_ready = false;
bool g_platform = false; // backend SDL3 initialisé (fenêtre réelle)

// ImGui alloue par SDL_malloc : visible du compteur d'allocations du moteur, et libérable par SDL.
void *ui_alloc(size_t size, void *)
{
    return SDL_malloc(size);
}
void ui_free(void *ptr, void *)
{
    SDL_free(ptr);
}

} // namespace

extern "C" bool ow_ui_create(void *window, void *device, uint32_t color_format)
{
    if (g_ready) {
        ENG_LOG_ERROR("UI : déjà créée");
        return false;
    }
    if (device == nullptr) {
        ENG_LOG_ERROR("UI : pas de périphérique GPU");
        return false;
    }
    ImGui::SetAllocatorFunctions(ui_alloc, ui_free, nullptr);
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // pas de imgui.ini : la disposition est décidée par le code
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();

    g_platform = false;
    if (window != nullptr) {
        if (!ImGui_ImplSDL3_InitForSDLGPU(static_cast<SDL_Window *>(window))) {
            ENG_LOG_ERROR("UI : backend SDL3 non initialisé");
            ImGui::DestroyContext();
            return false;
        }
        g_platform = true;
    } else {
        io.DisplaySize = ImVec2(1.0F, 1.0F);
    }
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = static_cast<SDL_GPUDevice *>(device);
    info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>(color_format);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        ENG_LOG_ERROR("UI : backend SDL_GPU non initialisé");
        if (g_platform) {
            ImGui_ImplSDL3_Shutdown();
            g_platform = false;
        }
        ImGui::DestroyContext();
        return false;
    }
    g_ready = true;
    return true;
}

extern "C" void ow_ui_destroy(void)
{
    if (!g_ready) {
        return;
    }
    ImGui_ImplSDLGPU3_Shutdown();
    if (g_platform) {
        ImGui_ImplSDL3_Shutdown();
        g_platform = false;
    }
    ImGui::DestroyContext();
    g_ready = false;
}

extern "C" bool ow_ui_process_event(const void *sdl_event)
{
    if (!g_ready || !g_platform || sdl_event == nullptr) {
        return false;
    }
    const SDL_Event *ev = static_cast<const SDL_Event *>(sdl_event);
    (void)ImGui_ImplSDL3_ProcessEvent(ev);
    const ImGuiIO &io = ImGui::GetIO();
    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
        return io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
        return io.WantCaptureKeyboard;
    default:
        return false; // les relâchements ne sont jamais volés au jeu (pas de touche coincée)
    }
}

extern "C" void ow_ui_new_frame(int width, int height, float dt)
{
    if (!g_ready) {
        return;
    }
    ImGui_ImplSDLGPU3_NewFrame();
    if (g_platform) {
        ImGui_ImplSDL3_NewFrame();
    } else {
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DeltaTime = dt > 0.0F ? dt : 1.0F / 60.0F;
    }
    ImGui::NewFrame();
}

extern "C" uint32_t ow_ui_render(void)
{
    if (!g_ready) {
        return 0;
    }
    ImGui::Render();
    const ImDrawData *data = ImGui::GetDrawData();
    return data != nullptr && data->TotalVtxCount > 0 ? static_cast<uint32_t>(data->TotalVtxCount)
                                                      : 0U;
}

extern "C" void ow_ui_prepare(void *command_buffer)
{
    ImDrawData *data = g_ready ? ImGui::GetDrawData() : nullptr;
    if (data != nullptr && command_buffer != nullptr) {
        ImGui_ImplSDLGPU3_PrepareDrawData(data,
                                          static_cast<SDL_GPUCommandBuffer *>(command_buffer));
    }
}

extern "C" void ow_ui_draw(void *command_buffer, void *render_pass)
{
    ImDrawData *data = g_ready ? ImGui::GetDrawData() : nullptr;
    if (data != nullptr && command_buffer != nullptr && render_pass != nullptr) {
        ImGui_ImplSDLGPU3_RenderDrawData(data, static_cast<SDL_GPUCommandBuffer *>(command_buffer),
                                         static_cast<SDL_GPURenderPass *>(render_pass));
    }
}

extern "C" bool ow_ui_wants_mouse(void)
{
    return g_ready && ImGui::GetIO().WantCaptureMouse;
}

extern "C" bool ow_ui_wants_keyboard(void)
{
    return g_ready && ImGui::GetIO().WantCaptureKeyboard;
}

extern "C" void ow_ui_add_mouse_pos(float x, float y)
{
    if (g_ready) {
        ImGui::GetIO().AddMousePosEvent(x, y);
    }
}

extern "C" void ow_ui_add_mouse_button(int button, bool down)
{
    if (g_ready) {
        ImGui::GetIO().AddMouseButtonEvent(button, down);
    }
}
