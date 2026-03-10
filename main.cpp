#include <jni.h>
#include <string>
#include <vector>
#include <time.h>
#include <algorithm>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Hooks.h"

#define LOG_TAG "YRENER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

std::vector<BulletTrace> traces;

static uintptr_t g_libBase   = 0;
static bool      g_hooksOK   = false;
static bool      g_initImGui = false;
static int       g_glW = 0, g_glH = 0;

// ── Патч памяти ──────────────────────────────────────────────────
void patch_mem(uintptr_t addr, void* data, size_t size) {
    uintptr_t page = addr & ~0xFFF;
    mprotect((void*)page, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((void*)addr, data, size);
    mprotect((void*)page, 0x1000, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char*)addr, (char*)addr + size);
}

// ── ARM64 inline hook (LDR X17,#8 / BR X17) ─────────────────────
void inline_hook(uintptr_t target, void* new_func, uint8_t* orig_out) {
    memcpy(orig_out, (void*)target, 16);
    uint8_t tramp[16] = {
        0x51,0x00,0x00,0x58, // LDR X17, #8
        0x20,0x02,0x1F,0xD6, // BR X17
        0,0,0,0,0,0,0,0      // адрес
    };
    uint64_t fn = (uint64_t)new_func;
    memcpy(tramp+8, &fn, 8);
    patch_mem(target, tramp, 16);
}

// ── База либы ────────────────────────────────────────────────────
uintptr_t GetLibBase() {
    if (g_libBase) return g_libBase;
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, LIB_NAME)) {
            g_libBase = (uintptr_t)strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(f);
    return g_libBase;
}

// ── Звук ─────────────────────────────────────────────────────────
void PlaySoundEffect(const char* fileName) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "mpv /sdcard/Download/yrener_sounds/%s --no-video --volume=100 &", fileName);
    system(cmd);
}

void OnKill() {
    if (!cfg.announcer) return;
    long now = time(NULL);
    if (now - cfg.last_kill_time > 15) cfg.kills = 0;
    cfg.kills++;
    cfg.last_kill_time = now;
    if      (cfg.kills == 2) PlaySoundEffect("double.mp3");
    else if (cfg.kills == 3) PlaySoundEffect("triple.mp3");
    else if (cfg.kills >= 5) PlaySoundEffect("monster.mp3");
}

void PlayHitSound() {
    if      (cfg.selected_hit == 1) PlaySoundEffect("hit_bell.mp3");
    else if (cfg.selected_hit == 2) PlaySoundEffect("hit_punch.mp3");
}

// ── Хуки ─────────────────────────────────────────────────────────
static uint8_t orig_BulletImpact[16];
static uint8_t orig_HitEvent[16];
typedef void (*tBulletImpact)(uintptr_t, float, float, float, float);
typedef void (*tHitEvent)(uintptr_t, float);

void my_BulletImpact(uintptr_t self, float sx, float sy, float ex, float ey) {
    if (cfg.bullet_track) {
        BulletTrace t;
        t.start    = ImVec2(sx, sy);
        t.end      = ImVec2(ex, ey);
        t.die_time = ImGui::GetTime() + cfg.track_duration;
        t.color    = ImColor(cfg.track_color[0], cfg.track_color[1],
                             cfg.track_color[2], cfg.track_color[3]);
        traces.push_back(t);
    }
    PlayHitSound();
    uintptr_t target = GetLibBase() + ADDR_BULLET_IMPACT;
    patch_mem(target, orig_BulletImpact, 16);
    ((tBulletImpact)target)(self, sx, sy, ex, ey);
    inline_hook(target, (void*)my_BulletImpact, orig_BulletImpact);
}

void my_HitEvent(uintptr_t self, float damage) {
    float hp = 0.0f;
    if (self) memcpy(&hp, (void*)(self + OFF_RP_HEALTH), sizeof(float));
    if (hp <= 0.0f) OnKill();
    uintptr_t target = GetLibBase() + ADDR_HIT_EVENT;
    patch_mem(target, orig_HitEvent, 16);
    ((tHitEvent)target)(self, damage);
    inline_hook(target, (void*)my_HitEvent, orig_HitEvent);
}

void InstallHooks() {
    if (g_hooksOK) return;
    uintptr_t base = GetLibBase();
    if (!base) return;
    inline_hook(base + ADDR_BULLET_IMPACT, (void*)my_BulletImpact, orig_BulletImpact);
    inline_hook(base + ADDR_HIT_EVENT,     (void*)my_HitEvent,     orig_HitEvent);
    g_hooksOK = true;
    LOGI("Hooks installed. Base=0x%lx", base);
}

void ApplyPatches() {
    uintptr_t base = GetLibBase();
    if (!base) return;
    if (cfg.aim_enabled)
        patch_mem(base + ADDR_FOV,       &cfg.fov_value, sizeof(float));
    if (cfg.fly_enabled)
        patch_mem(base + ADDR_FLY_SPEED, &cfg.fly_speed, sizeof(float));
}

// ── Меню ─────────────────────────────────────────────────────────
void RenderYrenerMenu() {
    ImGui::Begin("YRENER SOFT [MOBILE]");

    if (ImGui::CollapsingHeader("AIM")) {
        ImGui::Checkbox("Aimbot", &cfg.aim_enabled);
        ImGui::TextColored(ImVec4(1,0,0,1), "WARNING 10 SECONDS");
        ImGui::SliderFloat("FOV", &cfg.fov_value, 70.0f, 180.0f);
    }

    if (ImGui::CollapsingHeader("VEHICLE")) {
        ImGui::Checkbox("Enable Fly",  &cfg.fly_enabled);
        ImGui::SliderFloat("Speed",    &cfg.fly_speed,   0.0f, 100.0f);
        ImGui::SliderFloat("Brake",    &cfg.brake_force, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("MISC")) {
        ImGui::Checkbox("Bullet Track", &cfg.bullet_track);
        ImGui::SameLine();
        if (ImGui::Button("Gear")) ImGui::OpenPopup("TrackCfg");
        if (ImGui::BeginPopup("TrackCfg")) {
            ImGui::SliderInt("Duration (s)", &cfg.track_duration, 2, 9);
            ImGui::ColorEdit4("Color",       cfg.track_color);
            ImGui::EndPopup();
        }
        ImGui::Separator();
        const char* hits[] = { "None", "Notification 1", "Notification 2" };
        ImGui::Combo("Hit Sound", &cfg.selected_hit, hits, 3);
        if (ImGui::TreeNode("Sound Mix (Legendary)")) {
            ImGui::Checkbox("Kill Announcer", &cfg.announcer);
            if (cfg.announcer) {
                ImGui::Text("Streak: %d", cfg.kills);
                if (ImGui::Button("Test Monster Kill")) OnKill();
                if (ImGui::Button("Reset streak"))      cfg.kills = 0;
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();

    float curTime = ImGui::GetTime();
    traces.erase(std::remove_if(traces.begin(), traces.end(),
        [curTime](const BulletTrace& t){ return t.die_time < curTime; }), traces.end());
    for (auto& t : traces) {
        ImGui::GetBackgroundDrawList()->AddLine(t.start, t.end, t.color, 2.0f);
        ImGui::GetBackgroundDrawList()->AddCircleFilled(t.end, 3.0f, t.color);
    }
}

// ── EGL хук ──────────────────────────────────────────────────────
static uint8_t orig_egl[16];
typedef EGLBoolean (*tEglSwap)(EGLDisplay, EGLSurface);
static tEglSwap real_eglSwapBuffers = nullptr;

EGLBoolean my_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH,  &g_glW);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &g_glH);
    if (g_glW <= 0 || g_glH <= 0)
        return real_eglSwapBuffers(dpy, surface);

    if (!g_initImGui) {
        ImGui::CreateContext();
        ImGuiStyle* s = &ImGui::GetStyle();
        ImGui::StyleColorsDark(s);
        s->WindowRounding = 5.0f;
        s->FrameRounding  = 3.0f;
        s->GrabMinSize    = 13.0f;
        ImGui_ImplAndroid_Init(nullptr);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        ImGui::GetIO().IniFilename = NULL;
        g_initImGui = true;
        LOGI("ImGui initialized");
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    InstallHooks();
    ApplyPatches();
    RenderYrenerMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Вызов оригинального eglSwapBuffers
    uintptr_t target = (uintptr_t)real_eglSwapBuffers;
    patch_mem(target, orig_egl, 16);
    EGLBoolean ret = real_eglSwapBuffers(dpy, surface);
    inline_hook(target, (void*)my_eglSwapBuffers, orig_egl);
    return ret;
}

// ── Main thread ──────────────────────────────────────────────────
void* main_thread(void*) {
    while (!GetLibBase()) {
        LOGI("Waiting for %s...", LIB_NAME);
        sleep(1);
    }
    LOGI("Base: 0x%lx", g_libBase);

    void* egl  = dlopen("libEGL.so", RTLD_NOW);
    void* addr = dlsym(egl, "eglSwapBuffers");
    real_eglSwapBuffers = (tEglSwap)addr;
    inline_hook((uintptr_t)addr, (void*)my_eglSwapBuffers, orig_egl);
    LOGI("eglSwapBuffers hooked");

    return nullptr;
}

__attribute__((constructor)) void _init() {
    pthread_t t;
    pthread_create(&t, nullptr, main_thread, nullptr);
}
