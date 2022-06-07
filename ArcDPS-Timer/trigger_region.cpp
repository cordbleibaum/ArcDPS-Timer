#include "trigger_region.h"

#include <cmath>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "arcdps.h"
#include "arcdps-extension/imgui_stdlib.h"

using json = nlohmann::json;

bool TriggerRegion::trigger(Eigen::Vector3f player_position)
{
    if (is_triggered) return false;

    if (check(player_position)) {
        is_triggered = true;
        return true;
    }

    return false;
}

void TriggerRegion::reset() {
    is_triggered = false;
}

bool TriggerRegion::get_is_triggered() const {
    return is_triggered;
}

SphereTrigger::SphereTrigger(){
}

SphereTrigger::SphereTrigger(Eigen::Vector3f position, float radius)
 :  position(position),
    radius(radius) {
}

bool SphereTrigger::check(Eigen::Vector3f player_position) {
    float distance = (position - player_position).norm();
    return distance < radius;
}

std::string SphereTrigger::get_typename_id() const {
    return "TypeSphere";
}

Eigen::Vector3f SphereTrigger::get_middle() const {
    return position;
}

PlaneTrigger::PlaneTrigger(){
}

PlaneTrigger::PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z, float thickness)
:   side1(side1),
    side2(side2),
    height(height),
    thickness(thickness),
    z(z) {
}

bool PlaneTrigger::check(Eigen::Vector3f player_position) {
    if (player_position.z() > z && player_position.z() < z + height) {
        auto player_position_2d = Eigen::Vector2f(player_position.x(), player_position.y());

        auto e1 = side2 - side1;
        auto e2 = player_position_2d - side1;
        float alpha = e1.dot(e2) / e1.dot(e1);

        auto projected_point = (1.0f - alpha) * side1 + alpha * side2;
        float distance = (projected_point - player_position_2d).norm();

        if (alpha >= 0 && alpha <= 1.0 && distance < thickness) {
            return true;
        }
    }

    return false;
}

std::string PlaneTrigger::get_typename_id() const {
    return "TypePlane";
}

Eigen::Vector3f PlaneTrigger::get_middle() const {
    auto middle_xy = (side1 + side2) / 2;
    return Eigen::Vector3f(middle_xy.x(), middle_xy.y(), z + height/2);
}

TriggerWatcher::TriggerWatcher(GW2MumbleLink& mumble_link)
    : mumble_link(mumble_link) {
    if (!std::filesystem::exists(trigger_directory)) {
        std::filesystem::create_directory(trigger_directory);
    }
}

bool TriggerWatcher::watch() {
    Eigen::Vector3f player_position = Eigen::Vector3f(mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]);

    for (int i = 0; i < regions.size(); ++i) {
        if (regions[i]->trigger(player_position)) {
            trigger_signal(regions[i]->name);
            return true;
        }
    }
    return false;
}

void TriggerWatcher::reset() {
    log_debug("timer: resetting triggers");
    for (auto& region : regions) {
        region->reset();
    }
}

void TriggerWatcher::map_change(uint32_t map_id) {
    json triggers_out = regions;
    std::ofstream o(trigger_directory + std::to_string(last_map_id) + ".json");
    o << std::setw(4) << triggers_out << std::endl;

    last_map_id = mumble_link->getMumbleContext()->mapId;
    if (std::filesystem::exists(trigger_directory + std::to_string(last_map_id) + ".json")) {
        json triggers_in;
        std::ifstream input(trigger_directory + std::to_string(last_map_id) + ".json");
        input >> triggers_in;
        regions = triggers_in;
    }
}

TriggerWatcher::~TriggerWatcher() {
    json triggers_out = regions;
    std::ofstream o(trigger_directory + std::to_string(last_map_id) + ".json");
    o << std::setw(4) << triggers_out << std::endl;
}

TriggerEditor::TriggerEditor(Translation& translation, GW2MumbleLink& mumble_link, std::vector<std::shared_ptr<TriggerRegion>>& regions)
:   translation(translation),
    mumble_link(mumble_link),
    regions(regions) {
    if (!std::filesystem::exists(trigger_set_directory)) {
        std::filesystem::create_directory(trigger_set_directory);
    }
}

void TriggerEditor::mod_imgui() {
    ImGui::SetNextWindowSize(ImVec2(680, 0));
    if (is_open && ImGui::Begin(translation.get("HeaderTriggerEditor").c_str(), &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Columns(2, "##editorcolumns", false);
        ImGui::SetColumnWidth(-1, 480);

        std::string position_string = translation.get("TextPlayerPosition") + "%.1f %.1f %.1f";
        ImGui::Text(position_string.c_str(), mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

        ImGui::Text(translation.get("TextAreaSphere").c_str());
        ImGui::InputFloat(translation.get("TextInputRadius").c_str(), &sphere_radius);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 1, 0.2f, 0.3f));
        if (ImGui::Button(translation.get("ButtonPlaceSphere").c_str())) {
            std::shared_ptr<TriggerRegion> trigger(
                new SphereTrigger(Eigen::Vector3f(mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]), sphere_radius)
            );
            regions.push_back(trigger);
        }
        ImGui::PopStyleColor();

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

        ImGui::Text(translation.get("TextAreaPlane").c_str());
        ImGui::InputFloat(translation.get("InputThickness").c_str(), &plane_thickness);
        ImGui::InputFloat(translation.get("InputZ").c_str(), &plane_z);
        ImGui::SameLine();
        ImGui::PushID("set_plane_z");
        if (ImGui::Button(translation.get("ButtonSet").c_str())) {
            plane_z = mumble_link->fAvatarPosition[1] - 0.5f;
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

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 1, 0.2f, 0.3f));
        if (ImGui::Button(translation.get("ButtonPlacePlane").c_str())) {
            std::shared_ptr<TriggerRegion> trigger(
                new PlaneTrigger(
                    Eigen::Vector2f(plane_position1[0], plane_position1[1]), Eigen::Vector2f(plane_position2[0], plane_position2[1]),
                    plane_height, plane_z, plane_thickness
                )
            );
            regions.push_back(trigger);
        }
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 5.0f));

        ImGui::BeginTable("##triggertable", 6, ImGuiTableFlags_Hideable);
        ImGui::TableSetupColumn(translation.get("HeaderNumColumn").c_str());
        ImGui::TableSetupColumn(translation.get("HeaderTypeColumn").c_str());
        ImGui::TableSetupColumn(translation.get("HeaderMiddleColumn").c_str());
        ImGui::TableSetupColumn(translation.get("HeaderIsInRangeColumn").c_str());
        ImGui::TableSetupColumn(translation.get("HeaderIsTriggeredColumn").c_str());
        ImGui::TableSetupColumn(translation.get("HeaderNameColumn").c_str());
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

            ImGui::TableNextColumn();
            Eigen::Vector3f player_position = Eigen::Vector3f(mumble_link->fAvatarPosition[0], mumble_link->fAvatarPosition[2], mumble_link->fAvatarPosition[1]);
            if (region->check(player_position)) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), translation.get("TextYes").c_str());
            }

            ImGui::TableNextColumn();
            if (region->get_is_triggered()) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), translation.get("TextYes").c_str());
            }

            ImGui::TableNextColumn();
            ImGui::Text(region->name.c_str());
        }
        ImGui::EndTable();

        ImGui::PushID("delete_trigger");
        if (ImGui::Button(translation.get("ButtonDelete").c_str()) && selected_line >= 0) {
            regions.erase(regions.begin() + selected_line);
            selected_line = -1;
        }
        ImGui::PopID();
        ImGui::SameLine();
        if (ImGui::Button(translation.get("ButtonMoveUp").c_str()) && selected_line > 0) {
            std::shared_ptr<TriggerRegion> tmp = regions[selected_line - 1];
            regions[selected_line - 1] = regions[selected_line];
            regions[selected_line] = tmp;
        }
        ImGui::SameLine();
        if (ImGui::Button(translation.get("ButtonMoveDown").c_str()) && selected_line >= 0 && selected_line < regions.size() - 1) {
            std::shared_ptr<TriggerRegion> tmp = regions[selected_line + 1];
            regions[selected_line + 1] = regions[selected_line];
            regions[selected_line] = tmp;
        }

        ImGui::InputText(translation.get("InputRegionName").c_str(), &region_name);
        ImGui::SameLine();
        ImGui::PushID("set_region_name");
        if (ImGui::Button(translation.get("ButtonSet").c_str()) && selected_line >= 0) {
            const auto& region = regions[selected_line];
            region->name = region_name;
            region_name = "";
        }
        ImGui::PopID();

        ImGui::NextColumn();
        ImGui::SetColumnWidth(-1, 200);

        if (ImGui::Button(translation.get("ButtonHelp").c_str())) {
            is_help_open = true;
        }
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

        ImGui::BeginTable("##setstable", 1);
        for (auto& [name, _] : trigger_sets) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(name.c_str(), name == set_name, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                set_name = name;
            }
        }
        ImGui::EndTable();

        ImGui::InputText(translation.get("InputSetName").c_str(), &set_name);
        if (ImGui::Button(translation.get("ButtonSave").c_str())) {
            trigger_sets[set_name] = regions;
            save_sets();
        }
        ImGui::SameLine();
        ImGui::PushID("delete_set");
        if (ImGui::Button(translation.get("ButtonDelete").c_str())) {
            trigger_sets.erase(set_name);
            save_sets();
        }
        ImGui::PopID();
        ImGui::SameLine();
        if (ImGui::Button(translation.get("ButtonLoad").c_str())) {
            regions = trigger_sets[set_name];
        }

        ImGui::Columns();
        ImGui::End();
    }

    if (is_help_open && ImGui::Begin(translation.get("HeaderTriggerEditorHelp").c_str(), &is_help_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(translation.get("TriggerHelp1").c_str());
        ImGui::BulletText(translation.get("TriggerHelp2").c_str());
        ImGui::BulletText(translation.get("TriggerHelp3").c_str());
        ImGui::BulletText(translation.get("TriggerHelp4").c_str());
        ImGui::BulletText(translation.get("TriggerHelp5").c_str());
        ImGui::BulletText(translation.get("TriggerHelp6").c_str());
        ImGui::BulletText(translation.get("TriggerHelp7").c_str());
        ImGui::End();
    }
}

void TriggerEditor::mod_windows() {
    ImGui::Checkbox(translation.get("WindowOptionTriggerEditor").c_str(), &is_open);
}

void TriggerEditor::map_change(uint32_t map_id) {
    if (std::filesystem::exists(trigger_set_directory + std::to_string(mumble_link->getMumbleContext()->mapId) + ".json")) {
        json triggers_in;
        std::ifstream input(trigger_set_directory + std::to_string(mumble_link->getMumbleContext()->mapId) + ".json");
        input >> triggers_in;
        trigger_sets = triggers_in;
    }
}

void TriggerEditor::save_sets() {
    json triggers_out = trigger_sets;
    std::ofstream o(trigger_set_directory + std::to_string(mumble_link->getMumbleContext()->mapId) + ".json");
    o << std::setw(4) << triggers_out << std::endl;
}

void region_to_json(json& j, const TriggerRegion* region) {
    if (region->get_typename_id() == "TypeSphere") {
        SphereTrigger* sphere = (SphereTrigger*)(region);
        j = *sphere;
        j["type"] = "sphere";
    }
    else if (region->get_typename_id() == "TypePlane") {
        PlaneTrigger* plane = (PlaneTrigger*)(region);
        j = *plane;
        j["type"] = "plane";
    }
}

void region_from_json(const json& j, TriggerRegion*& region) {
    if (j["type"] == "sphere") {
        SphereTrigger* sphere = new SphereTrigger();
        *sphere = j;
        region = sphere;
    }
    else if (j["type"] == "plane") {
        PlaneTrigger* plane = new PlaneTrigger();
        *plane = j;
        region = plane;
    }
}
