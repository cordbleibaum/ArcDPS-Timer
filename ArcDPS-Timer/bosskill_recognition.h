#pragma once

#include <vector>
#include <map>
#include <boost/signals2.hpp>
#include <mutex>
#include <chrono>
#include <functional>

#include "arcdps.h"
#include "mumble_link.h"
#include "settings.h"

struct AgentData {
	uintptr_t species_id = 0;
	long damage_taken = 0;
};

struct EncounterData {
	std::map<uintptr_t, AgentData> log_agents;
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

std::function<bool(EncounterData&)> condition_npc_id(uintptr_t npc_id);
std::function<bool(EncounterData&)> condition_npc_damage_taken(uintptr_t npc_id, long damage);