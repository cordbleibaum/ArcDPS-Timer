#pragma once

#include <vector>
#include <map>
#include <set>
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
	std::chrono::system_clock::time_point last_hit;
};

struct EncounterData {
	std::map<uintptr_t, AgentData> log_agents;
	uintptr_t log_species_id;
	uint32_t map_id;

	void archive() noexcept;
};

struct BossDescription {
	BossDescription(std::function<std::chrono::system_clock::time_point(EncounterData&)> timing, std::vector<std::function<bool(EncounterData&)>> conditions);
	const std::vector<std::function<bool(EncounterData&)>> conditions;
	const std::function<std::chrono::system_clock::time_point(EncounterData&)> timing;
};

class BossKillRecognition {
public:
	BossKillRecognition(GW2MumbleLink& mumble_link, const Settings& settings);
	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);

	boost::signals2::signal<void(std::chrono::system_clock::time_point time)> bosskill_signal;
private:
	GW2MumbleLink& mumble_link;
	const Settings& settings;

	std::vector<BossDescription> bosses;

	std::mutex logagents_mutex;
	std::chrono::system_clock::time_point log_start_time;

	EncounterData data;

	void add_defaults();
	void emplace_conditions(std::function<std::chrono::system_clock::time_point(EncounterData&)> timing, std::initializer_list<std::function<bool(EncounterData&)>> initializer);
};

std::function<bool(EncounterData&)> condition_npc_id(uintptr_t npc_id);
std::function<bool(EncounterData&)> condition_npc_damage_taken(uintptr_t npc_id, long damage);
std::function<bool(EncounterData&)> condition_npc_id_at_least_one(std::set<uintptr_t> npc_ids);
std::function<bool(EncounterData&)> condition_npc_last_damage_time_distance(uintptr_t npc_id_a, uintptr_t npc_id_b, std::chrono::system_clock::duration distance);
std::function<bool(EncounterData&)> condition_map_id(uint32_t map_id);

std::function<std::chrono::system_clock::time_point(EncounterData&)> timing_last_hit_npc();
std::function<std::chrono::system_clock::time_point(EncounterData&)> timing_last_hit_npc_id(uintptr_t npc_id);
