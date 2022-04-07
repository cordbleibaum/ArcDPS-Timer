#pragma once

#include "arcdps.h"

#include <string>
#include <set>
#include <mutex>

class GroupTracker {
public:
	const std::set<std::string>& get_players();
	std::string get_group_id();
	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);
private:
	void calculate_groupcode();

	std::string selfAccountName;
	std::set<std::string> group_players;

	std::string group_code;
	std::mutex groupcode_mutex;
};