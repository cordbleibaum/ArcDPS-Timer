#include "maptracker.h"

#include "hash-library/crc32.h"
#include "arcdps.h"

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

		sockaddr_in* address = (sockaddr_in*) &(mumble_link->getMumbleContext()->serverAddress[0]);
		if (address->sin_family == AF_INET) {
			map_code = CRC32()(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		}
		else {
			log_arc("timer: error: ipv6 is unsupported");
			return false;
		}
		return true;
	}
	return false;
}
