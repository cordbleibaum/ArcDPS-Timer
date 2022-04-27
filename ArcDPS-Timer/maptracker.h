#pragma once

#include "mumble_link.h"

class MapTracker {
public:
	MapTracker(GW2MumbleLink& mumble_link);
	bool watch();
private:
	GW2MumbleLink& mumble_link;
	uint32_t last_map_id = 0;
};
