#include "maptracker.h"

MapTracker::MapTracker(GW2MumbleLink& mumble_link)
: mumble_link(mumble_link) {
}

bool MapTracker::watch() {
	if (last_map_id != mumble_link->getMumbleContext()->mapId) {
		last_map_id = mumble_link->getMumbleContext()->mapId;
		return true;
	}
	return false;
}
