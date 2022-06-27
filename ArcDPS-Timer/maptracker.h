#pragma once

#include "mumble_link.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <boost/signals2.hpp>

enum class MapID {
	// Fractals
	MistlockObservatory = 872,
	UncategorizedFractal = 947,
	SnowblindFractal = 948,
	SwamplandFractal = 949,
	UrbanBattlegroundFractal = 950,
	AquaticRuinsFractal = 951,
	CliffsideFractal = 952,
	UndergroundFacilityFractal = 953,
	VolcanicFractal = 954,
	MoltenFurnace = 955,
	AetherbladeFractal = 956,
	ThaumanovaReactor = 957,
	SolidOceanFractal = 958,
	MoltenBoss = 959,
	CaptainMaiTrinBoss = 960,
	ChaosFractal = 1164,
	Nightmare = 1177,
	ShatteredObservatory = 1205,
	TwilightOasis = 1267,
	DeepstoneFractal = 1290,
	SunquaPeakFractal = 1384
};

enum class InstanceType {
	Raid,
	Fractal,
	Strike,
	Dungeon,
	Unknown
};

class MapTracker {
public:
	MapTracker(GW2MumbleLink& mumble_link);
	std::string get_instance_id();
	InstanceType get_instance_type() const;
	void watch();

	boost::signals2::signal<void(uint32_t)> map_change_signal;
private:
	GW2MumbleLink& mumble_link;
	std::string map_code;

	uint32_t map_id = 0;
	mutable std::shared_mutex mapcode_mutex;
};
