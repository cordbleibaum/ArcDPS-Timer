#include "maptracker.h"

#include "hash-library/crc32.h"

MapTracker::MapTracker(GW2MumbleLink& mumble_link)
: mumble_link(mumble_link) {
}

std::string MapTracker::get_instance_id() {
	watch();

	std::shared_lock lock(mapcode_mutex);
	return map_code;
}

bool MapTracker::watch() {
	if (map_id != mumble_link->getMumbleContext()->mapId) {
		map_id = mumble_link->getMumbleContext()->mapId;
		
		std::unique_lock lock(mapcode_mutex);
		map_code = CRC32()(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		return true;
	}
	return false;
}
