#include "grouptracker.h"

#include "hash-library/crc32.h"

const std::set<std::string>& GroupTracker::get_players() {
	return group_players;
}

std::string GroupTracker::get_group_id()
{
	std::scoped_lock<std::mutex> guard(groupcode_mutex);
	return group_code;
}

void GroupTracker::calculate_groupcode() {
	std::string playersConcat = "";

	for (auto it = group_players.begin(); it != group_players.end(); ++it) {
		playersConcat = playersConcat + (*it);
	}

	group_code = CRC32()(playersConcat);
}

void GroupTracker::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id)
{
	if (!ev) {
		if (!src->elite) {
			if (src->name != nullptr && src->name[0] != '\0' && dst->name != nullptr && dst->name[0] != '\0') {
				std::string username(dst->name);
				if (username.at(0) == ':') {
					username.erase(0, 1);
				}

				if (src->prof) {
					std::scoped_lock<std::mutex> guard(groupcode_mutex);
					if (selfAccountName.empty() && dst->self) {
						selfAccountName = username;
					}

					group_players.insert(username);
					calculate_groupcode();
				}
				else {
					if (username != selfAccountName) {
						std::scoped_lock<std::mutex> guard(groupcode_mutex);
						group_players.erase(username);
						calculate_groupcode();
					}
				}
			}
		}
	}
}
