#pragma once

#include <array>

class TriggerRegion {
public:
	virtual bool trigger(std::array<float, 3> player_position) = 0;
	void reset();
protected:
	bool is_triggered;
};

class SphereTrigger : public TriggerRegion {
public:
	std::array<float, 3> position;
	float radius;
	virtual bool trigger(std::array<float, 3> player_position) override;
};
