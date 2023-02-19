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
	SunquaPeakFractal = 1384,

	// Raids
	SpiritVale = 1062,
	SalvationPass = 1149,
	StrongholdOfTheFaithful = 1156,
	BastionOfThePenitent = 1188,
	HallOfChains = 1264,
	MythwrightGambit = 1303,
	TheKeyOfAhdashim = 1323,

	// Dungeons
	AscalonianCatacombs = 36,
	AscalonianCatacombsStory = 33,
	CaudecusManor = 76,
	CaudecusManorStory = 75,
	TwilightArbor = 67,
	TwilightArborStory = 68,
	SorrowsEmbrace = 64,
	SorrowsEmbraceStory = 63,
	CitadelOfFlame = 69,
	CitadelOfFlameStory = 66,
	HonorOfTheWaves = 71,
	HonorOfTheWavesStory = 70,
	CrucibleOfEternity = 82,
	CrucibleOfEternityStory = 81,
	TheRuinedCityOfArah = 112,

	// Strikes
	XunlaiJadeJunkyard = 1450,
	KainengOverlook = 1451,
	HarvestTemple = 1437,
	AetherbladeHideout = 1432,
	ShiverpeaksPass = 1332,
	ColdWar = 1374,
	WhisperOfJormag = 1359,
	Boneskinner = 1339,
	VoiceOfthFallenAndClawOfTheFallen = 1346,
	FraenirOfJormag = 1341,
	ForgingSteel = 1368,
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
	std::string get_map_name();
	std::string get_map_name(uint32_t current_map_id);
	InstanceType get_instance_type() const;
	void watch();

	boost::signals2::signal<void(uint32_t)> map_change_signal;
private:
	GW2MumbleLink& mumble_link;
	std::string map_code;

	uint32_t map_id = 0;
	mutable std::shared_mutex mapcode_mutex;
};
