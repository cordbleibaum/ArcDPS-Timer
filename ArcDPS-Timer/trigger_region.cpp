#include "trigger_region.h"

#include <cmath>
#include <string>
#include <format>

#include "imgui/imgui.h"
#include "arcdps.h"

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

std::string SphereTrigger::get_typename_id() {
    return "TypeSphere";
}

Eigen::Vector3f SphereTrigger::get_middle() {
    return position;
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

std::string PlaneTrigger::get_typename_id() {
    return "TypePlane";
}

Eigen::Vector3f PlaneTrigger::get_middle() {
    auto middle_xy = (side1 + side2) / 2;
    return Eigen::Vector3f(middle_xy.x(), middle_xy.y(), z + height/2);
}

TriggerWatcher::TriggerWatcher(GW2MumbleLink& mumble_link)
    : mumble_link(mumble_link) {
}

void TriggerWatcher::watch() {
    last_triggered = -1;

    Eigen::Vector3f player_position = Eigen::Vector3f(mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]);

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

TriggerEditor::TriggerEditor(Translation& translation, GW2MumbleLink& mumble_link)
:   translation(translation),
    mumble_link(mumble_link) {
}

void TriggerEditor::mod_options() {
    ImGui::Separator();
    if (ImGui::Button(translation.get("ButtonOpenTriggerEditor").c_str())) {
        is_open = true;
        selected_line = -1;
    }
}

void TriggerEditor::mod_imgui() {
    if (is_open) {
        if (ImGui::Begin(translation.get("HeaderTriggerEditor").c_str(), &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {

            std::string position_string = translation.get("TextPlayerPosition") + "%.1f %.1f %.1f";
            ImGui::Text(position_string.c_str(), mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]);
            ImGui::Separator();

            ImGui::Text(translation.get("TextAreaSphere").c_str());
            ImGui::InputFloat(translation.get("TextInputRadius").c_str(), &sphere_radius);
            ImGui::InputFloat3(translation.get("InputXYZ").c_str(), &sphere_position[0]);
            ImGui::SameLine();
            ImGui::PushID("set_sphere_position");
            if (ImGui::Button(translation.get("ButtonSet").c_str())) {
                sphere_position[0] = mumble_link->fAvatarPosition[0];
                sphere_position[1] = mumble_link->fAvatarPosition[2];
                sphere_position[2] = mumble_link->fAvatarPosition[1];
            }
            ImGui::PopID();
            if (ImGui::Button(translation.get("ButtonPlaceSphere").c_str())) {
                std::shared_ptr<TriggerRegion> trigger(
                    new SphereTrigger(Eigen::Vector3f(sphere_position[0], sphere_position[1], sphere_position[2]), sphere_radius)
                );
                regions.push_back(trigger);
            }
            ImGui::Separator();

            ImGui::InputFloat(translation.get("InputThickness").c_str(), &plane_thickness);
            ImGui::InputFloat(translation.get("InputZ").c_str(), &plane_z);
            ImGui::SameLine();
            ImGui::PushID("set_plane_z");
            if (ImGui::Button(translation.get("ButtonSet").c_str())) {
                plane_z = mumble_link->fAvatarPosition[1];
            }
            ImGui::PopID();
            ImGui::InputFloat(translation.get("InputHeight").c_str(), &plane_height);
            ImGui::InputFloat2(translation.get("InputXY1").c_str(), &plane_position1[0]);
            ImGui::SameLine();
            ImGui::PushID("set_plane1");
            if (ImGui::Button(translation.get("ButtonSet").c_str())) {
                plane_position1[0] = mumble_link->fAvatarPosition[0];
                plane_position1[1] = mumble_link->fAvatarPosition[2];
            }
            ImGui::PopID();
            ImGui::InputFloat2(translation.get("InputXY2").c_str(), &plane_position2[0]);
            ImGui::SameLine();
            ImGui::PushID("set_plane2");
            if (ImGui::Button(translation.get("ButtonSet").c_str())) {
                plane_position2[0] = mumble_link->fAvatarPosition[0];
                plane_position2[1] = mumble_link->fAvatarPosition[2];
            }
            ImGui::PopID();
            if (ImGui::Button(translation.get("ButtonPlacePlane").c_str())) {
                std::shared_ptr<TriggerRegion> trigger(
                    new PlaneTrigger(
                        Eigen::Vector2f(plane_position1[0], plane_position1[1]), Eigen::Vector2f(plane_position2[0], plane_position2[1]),
                        plane_height, plane_z, plane_thickness
                    )
                );
                regions.push_back(trigger);
            }
            ImGui::Separator();

            ImGui::BeginTable("##triggertable", 3);
            ImGui::TableSetupColumn(translation.get("HeaderNumColumn").c_str());
            ImGui::TableSetupColumn(translation.get("HeaderTypeColumn").c_str());
            ImGui::TableSetupColumn(translation.get("HeaderMiddleColumn").c_str());
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < regions.size(); ++i) {
                const auto& region = regions[i];

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::to_string(i).c_str(), selected_line == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    selected_line = selected_line == i ? -1 : i;
                }

                ImGui::TableNextColumn();
                ImGui::Text(translation.get(region->get_typename_id()).c_str());

                ImGui::TableNextColumn();
                auto middle = region->get_middle();
                ImGui::Text("%.1f %.1f %.1f", middle.x(), middle.y(), middle.z());
            }

            ImGui::EndTable();

            if (ImGui::Button(translation.get("ButtonDelete").c_str()) && selected_line >= 0) {
                regions.erase(regions.begin() + selected_line);
                selected_line = -1;
            }
        }
        ImGui::End();
    }
}
