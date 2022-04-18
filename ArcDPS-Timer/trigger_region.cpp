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

PlaneTrigger::PlaneTrigger(Eigen::Vector3f center, Eigen::Vector3f normal, float width, float height)
:   center(center),
    normal(normal),
    width(width),
    height(height) {
}

bool PlaneTrigger::trigger(Eigen::Vector3f player_position) {
    if (is_triggered) return false;


    return false;
}

TriggerWatcher::TriggerWatcher(GW2MumbleLink& mumble_link)
    : mumble_link(mumble_link) {
}

void TriggerWatcher::watch() {
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
