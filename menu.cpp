#include "imgui/imgui.h"
#include "Hooks.h"

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
                if (ImGui::Button("Test"))  { /* OnKill() */ }
                if (ImGui::Button("Reset")) cfg.kills = 0;
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
