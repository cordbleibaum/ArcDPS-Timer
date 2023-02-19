#include "maptracker.h"

#include <utility>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "hash-library/crc32.h"
#include "arcdps.h"

using json = nlohmann::json;

MapTracker::MapTracker(GW2MumbleLink& mumble_link)
: mumble_link(mumble_link) {
}

std::string MapTracker::get_instance_id() {
	watch();

	std::shared_lock lock(mapcode_mutex);
	return map_code;
}

std::string MapTracker::get_map_name() {
	return get_map_name(map_id);
}

std::string MapTracker::get_map_name(uint32_t current_map_id) {
	watch();

	const std::map<uint32_t, std::string> map_name_overrides = {
	{872, "Mistlock Observatory"},
	{947, "Uncategorized Fractal"},
	{948, "Snowblind Fractal"},
	{949, "Swampland Fractal"},
	{950, "Urban Battleground Fractal"},
	{951, "Aquatic Ruins Fractal"},
	{952, "Cliffside Fractal"},
	{953, "Underground Facility Fractal"},
	{954, "Volcanic Fractal"},
	{955, "Molten Furnace"},
	{956, "Aetherblade"},
	{957, "Thaumanova Reactor"},
	{958, "Solid Ocean Fractal"},
	{959, "Molten Boss"},
	{960, "Captain Mai Trin Boss"},
	{1164, "Chaos Fractal"},
	{1177, "Nightmare"},
	{1205, "Shattered Observatory"},
	{1267, "Twilight Oasis"},
	{1290, "Deepstone Fractal"},
	{1384, "Sunqua Peak Fractal"}
	};

	std::string map_name = "Unknown";
	if (map_name_overrides.find(current_map_id) != map_name_overrides.end()) {
		map_name = map_name_overrides.at(current_map_id);
	}
	else {
		const auto map_response = cpr::Get(
			cpr::Url{ "https://api.guildwars2.com/v2/maps/" + std::to_string(current_map_id) },
			cpr::Timeout{ 8000 }
		);

		if (map_response.status_code == cpr::status::HTTP_OK) {
			const json map_response_json = json::parse(map_response.text);
			map_name = map_response_json["name"];
		}
	}

	return map_name;
}

InstanceType MapTracker::get_instance_type() const {
	switch (map_id)
	{
	case std::to_underlying(MapID::AetherbladeFractal):
	case std::to_underlying(MapID::CaptainMaiTrinBoss):
	case std::to_underlying(MapID::AquaticRuinsFractal):
	case std::to_underlying(MapID::ChaosFractal):
	case std::to_underlying(MapID::CliffsideFractal):
	case std::to_underlying(MapID::DeepstoneFractal):
	case std::to_underlying(MapID::MistlockObservatory):
	case std::to_underlying(MapID::MoltenFurnace):
	case std::to_underlying(MapID::MoltenBoss):
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
	case std::to_underlying(MapID::SpiritVale):
	case std::to_underlying(MapID::StrongholdOfTheFaithful):
	case std::to_underlying(MapID::SalvationPass):
	case std::to_underlying(MapID::BastionOfThePenitent):
	case std::to_underlying(MapID::HallOfChains):
	case std::to_underlying(MapID::MythwrightGambit):
	case std::to_underlying(MapID::TheKeyOfAhdashim):
		return InstanceType::Raid;
	case std::to_underlying(MapID::AscalonianCatacombs):
	case std::to_underlying(MapID::AscalonianCatacombsStory):
	case std::to_underlying(MapID::CaudecusManor):
	case std::to_underlying(MapID::CaudecusManorStory):
	case std::to_underlying(MapID::TwilightArbor):
	case std::to_underlying(MapID::TwilightArborStory):
	case std::to_underlying(MapID::SorrowsEmbrace):
	case std::to_underlying(MapID::SorrowsEmbraceStory):
	case std::to_underlying(MapID::CitadelOfFlame):
	case std::to_underlying(MapID::CitadelOfFlameStory):
	case std::to_underlying(MapID::HonorOfTheWaves):
	case std::to_underlying(MapID::HonorOfTheWavesStory):
	case std::to_underlying(MapID::CrucibleOfEternity):
	case std::to_underlying(MapID::CrucibleOfEternityStory):
	case std::to_underlying(MapID::TheRuinedCityOfArah):
		return InstanceType::Dungeon;
	case std::to_underlying(MapID::XunlaiJadeJunkyard):
	case std::to_underlying(MapID::KainengOverlook):
	case std::to_underlying(MapID::HarvestTemple):
	case std::to_underlying(MapID::AetherbladeHideout):
	case std::to_underlying(MapID::ShiverpeaksPass):
	case std::to_underlying(MapID::ColdWar):
	case std::to_underlying(MapID::WhisperOfJormag):
	case std::to_underlying(MapID::Boneskinner):
	case std::to_underlying(MapID::VoiceOfthFallenAndClawOfTheFallen):
	case std::to_underlying(MapID::FraenirOfJormag):
	case std::to_underlying(MapID::ForgingSteel):
		return InstanceType::Strike;
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
