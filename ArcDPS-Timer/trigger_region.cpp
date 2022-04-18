#include "trigger_region.h"

#include <cmath>

#include "imgui/imgui.h"

void TriggerRegion::reset() {
    is_triggered = false;
}

SphereTrigger::SphereTrigger(Eigen::Vector3f position, float radius)
 :  position(position),
    radius(radius) {
}

bool SphereTrigger::trigger(Eigen::Vector3f player_position) {
    if (is_triggered) return false;

    float distance = (position - player_position).norm();
    if (distance < radius) {
        is_triggered = true;
        return true;
    }

    return false;
}

PlaneTrigger::PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z, float thickness)
:   side1(side1),
    side2(side2),
    height(height),
    thickness(thickness),
    z(z) {
}

bool PlaneTrigger::trigger(Eigen::Vector3f player_position) {
    if (is_triggered) return false;
    
    if (player_position.z() > z && player_position.z() < z + height) {
        auto player_position_2d = Eigen::Vector2f(player_position.x(), player_position.y());

        auto e1 = side2 - side1;
        auto e2 = player_position_2d - side1;
        float alpha = std::sqrtf(e1.dot(e2)) / e1.norm();
        
        auto projected_point = side1 + alpha * e1;
        float distance = (projected_point - player_position_2d).norm();

        if (alpha >= 0 && alpha <= 1.0 && distance < thickness) {
            is_triggered = true;
            return true;
        }
    }

    return false;
}

TriggerWatcher::TriggerWatcher(GW2MumbleLink& mumble_link)
    : mumble_link(mumble_link) {
}

void TriggerWatcher::watch() {
    last_triggered = -1;

    Eigen::Vector3f player_position = Eigen::Vector3f(mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[1], mumble_link->fAvatarPosition[2]);

    for (int i = 0; i < regions.size(); ++i) {
        if (regions[i]->trigger(player_position)) {
            last_triggered = i;
            return;
        }
    }
}

int TriggerWatcher::get_last_triggered() {
    return last_triggered;
}

TriggerEditor::TriggerEditor(Translation& translation)
:   translation(translation) {
}

void TriggerEditor::mod_options() {
    ImGui::Separator();
    if (ImGui::Button(translation.get("ButtonOpenTriggerEditor").c_str())) {
        is_open = true;
    }
}

void TriggerEditor::mod_imgui() {
    if (is_open) {
        ImGui::Begin(translation.get("HeaderTriggerEditor").c_str(), &is_open, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::End();
    }
}
