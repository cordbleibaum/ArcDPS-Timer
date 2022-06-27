#include "maptracker.h"

#include <utility>

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

InstanceType MapTracker::get_instance_type() {
	switch (map_id)
	{
	case std::to_underlying(MapID::AetherbladeRetreat):
	case std::to_underlying(MapID::AquaticRuinsFractal):
	case std::to_underlying(MapID::ChaosFractal):
	case std::to_underlying(MapID::CliffsideFractal):
	case std::to_underlying(MapID::DeepstoneFractal):
	case std::to_underlying(MapID::MistlockObservatory):
	case std::to_underlying(MapID::MoltenFurnace):
	case std::to_underlying(MapID::VolcanicFractal):
	case std::to_underlying(MapID::UncategorizedFractal):
	case std::to_underlying(MapID::ShatteredObservatory):
	case std::to_underlying(MapID::SnowblindFractal):
	case std::to_underlying(MapID::UrbanBattlegroundFractal):
	case std::to_underlying(MapID::UndergroundFacilityFractal):
	case std::to_underlying(MapID::TwilightOasis):
	case std::to_underlying(MapID::SunquaPeakFractal):
	case std::to_underlying(MapID::SwamplandFractal):
	case std::to_underlying(MapID::SolidOceanFractal):
	case std::to_underlying(MapID::ThaumanovaReactor):
		return InstanceType::Fractal;
		break;
	default:
		return InstanceType::Unknown;
		break;
	}
}

void MapTracker::watch() {
	if (map_id != mumble_link->getMumbleContext()->mapId) {
		map_id = mumble_link->getMumbleContext()->mapId;
		
		map_change_signal(map_id);

		std::unique_lock lock(mapcode_mutex);

		const sockaddr_in* address = (sockaddr_in*) &(mumble_link->getMumbleContext()->serverAddress[0]);
		if (address->sin_family == AF_INET) {
			map_code = CRC32()(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		}
		else {
			log_arc("timer: error: ipv6 is unsupported");
		}
	}
}
