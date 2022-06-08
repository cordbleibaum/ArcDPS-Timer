#pragma once

#include <vector>
#include <set>
#include <boost/signals2.hpp>
#include <mutex>
#include <chrono>
#include <functional>

#include "arcdps.h"
#include "mumble_link.h"
#include "settings.h"

struct EncounterData {
	std::set<uintptr_t> log_agents;
	uintptr_t log_species_id;
};

class BossKillRecognition {
public:
	BossKillRecognition(GW2MumbleLink& mumble_link, Settings& settings);
	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);

	boost::signals2::signal<void(uint64_t)> bosskill_signal;
private:
	GW2MumbleLink& mumble_link;
	Settings& settings;

	std::vector<std::vector<std::function<bool(EncounterData&)>>> conditions;

	std::mutex logagents_mutex;
	std::chrono::system_clock::time_point last_damage_ticks;
	std::chrono::system_clock::time_point log_start_time;

	EncounterData data;

	void add_defaults();
	void emplace_conditions(std::initializer_list<std::function<bool(EncounterData&)>> initializer);
};

std::function<bool(EncounterData&)> condition_npc_id(uintptr_t boss_id);
