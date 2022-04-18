#pragma once

#include <Eigen/Dense>

#include "lang.h"
#include "mumble_link.h"

class TriggerRegion {
public:
	virtual bool trigger(Eigen::Vector3f player_position) = 0;
	void reset();
protected:
	bool is_triggered;
};

class SphereTrigger : public TriggerRegion {
public:
	SphereTrigger(Eigen::Vector3f position, float radius);
	virtual bool trigger(Eigen::Vector3f player_position) override;
private:
	Eigen::Vector3f position;
	float radius;
};

class PlaneTrigger : public TriggerRegion {
public:
	PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z);
	virtual bool trigger(Eigen::Vector3f player_position) override;
private:
	float height, z;
	Eigen::Vector2f side1;
	Eigen::Vector2f side2;
};

class TriggerWatcher {
public:
	TriggerWatcher(GW2MumbleLink& mumble_link);
	void watch();
	int get_last_triggered();

	std::vector<TriggerRegion*> regions;
private:
	GW2MumbleLink& mumble_link;
	int last_triggered = -1;
};

class TriggerEditor {
public:
	TriggerEditor(Translation& translation);
	void mod_options();
	void mod_imgui();
private:
	Translation& translation;

	bool is_open = false;
};
