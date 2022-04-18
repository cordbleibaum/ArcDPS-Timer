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
	PlaneTrigger(Eigen::Vector3f center, Eigen::Vector3f normal, float width, float height);
	virtual bool trigger(Eigen::Vector3f player_position) override;
private:
	float width, height;
	Eigen::Vector3f center;
	Eigen::Vector3f normal;
};

class TriggerWatcher {
public:
	TriggerWatcher(GW2MumbleLink& mumble_link);
	void watch();
private:
	GW2MumbleLink& mumble_link;
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
