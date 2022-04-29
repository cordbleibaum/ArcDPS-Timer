#pragma once

#include "mumble_link.h"

#include <mutex>
#include <shared_mutex>
#include <string>

class MapTracker {
public:
	MapTracker(GW2MumbleLink& mumble_link);
	std::string get_instance_id();
	bool watch();
private:
	GW2MumbleLink& mumble_link;
	std::string map_code;

	uint32_t map_id = 0;
	mutable std::shared_mutex mapcode_mutex;
};
