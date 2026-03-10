#ifndef HOOKS_H
#define HOOKS_H

#include <stdint.h>
#include <vector>
#include "imgui/imgui.h"

// --- [ OFFSETS libblackrussia-client.so v16.65.13320 ] ---
#define LIB_NAME            "libblackrussia-client.so"
#define ADDR_FOV             0x00298d94
#define ADDR_BULLET_IMPACT   0x002e6d07
#define ADDR_HIT_EVENT       0x002ffbbd
#define ADDR_FLY_SPEED       0x002c5153
#define OFF_RP_HEALTH        0x130

// --- [ SETTINGS ] ---
struct Config {
    bool  aim_enabled    = false;
    float fov_value      = 70.0f;

    bool  fly_enabled    = false;
    float fly_speed      = 0.0f;
    float brake_force    = 0.0f;

    bool  bullet_track   = false;
    int   track_duration = 2;
    float track_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};

    bool  announcer      = false;
    int   kills          = 0;
    long  last_kill_time = 0;
    int   selected_hit   = 0;
} cfg;

struct BulletTrace {
    ImVec2  start;
    ImVec2  end;
    float   die_time;
    ImColor color;
};

extern std::vector<BulletTrace> traces;

#endif
