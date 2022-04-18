#pragma once

#include <array>

#include "lang.h"
#include "mumble_link.h"

class TriggerRegion {
public:
	virtual bool trigger(std::array<float, 3> player_position) = 0;
	void reset();
protected:
	bool is_triggered;
};

class SphereTrigger : public TriggerRegion {
public:
	SphereTrigger(std::array<float, 3> position, float radius);
	virtual bool trigger(std::array<float, 3> player_position) override;
private:
	std::array<float, 3> position;
	float radius;
};

class BoxTrigger : public TriggerRegion {
public:
	virtual bool trigger(std::array<float, 3> player_position) override;
private:

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
