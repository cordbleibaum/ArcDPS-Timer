#pragma once

#include <Eigen/Dense>
#include <array>
#include <string>
#include <memory>

#include "lang.h"
#include "mumble_link.h"

class TriggerRegion {
public:
	bool trigger(Eigen::Vector3f player_position);
	virtual bool check(Eigen::Vector3f player_position) = 0;
	virtual std::string get_typename_id() = 0;
	virtual Eigen::Vector3f get_middle() = 0;
	void reset();
protected:
	bool is_triggered;
};

class SphereTrigger : public TriggerRegion {
public:
	SphereTrigger(Eigen::Vector3f position, float radius);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() override;
	virtual Eigen::Vector3f get_middle() override;
private:
	Eigen::Vector3f position;
	float radius;
};

class PlaneTrigger : public TriggerRegion {
public:
	PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z, float thickness);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() override;
	virtual Eigen::Vector3f get_middle() override;
private:
	float height, z, thickness;
	Eigen::Vector2f side1;
	Eigen::Vector2f side2;
};

class TriggerWatcher {
public:
	TriggerWatcher(GW2MumbleLink& mumble_link);
	void watch();
	int get_last_triggered();

	std::vector<std::shared_ptr<TriggerRegion>> regions;
private:
	GW2MumbleLink& mumble_link;
	int last_triggered = -1;
};

class TriggerEditor {
public:
	TriggerEditor(Translation& translation, GW2MumbleLink& mumble_link);
	void mod_options();
	void mod_imgui();
private:
	GW2MumbleLink& mumble_link;
	Translation& translation;

	bool is_open = false;
	int selected_line = -1;

	float sphere_radius = 1;
	std::array<float, 3> sphere_position;

	float plane_thickness = 1;
	float plane_z = 0;
	float plane_height = 2;
	std::array<float, 2> plane_position1;
	std::array<float, 2> plane_position2;

	std::vector<std::shared_ptr<TriggerRegion>> regions;
};
