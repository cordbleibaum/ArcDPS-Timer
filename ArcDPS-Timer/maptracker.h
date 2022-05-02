#pragma once

#include "mumble_link.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <boost/signals2.hpp>


class MapTracker {
public:
	MapTracker(GW2MumbleLink& mumble_link);
	std::string get_instance_id();
	void watch();

	boost::signals2::signal<void(uint32_t)> map_change_signal;
private:
	GW2MumbleLink& mumble_link;
	std::string map_code;

	uint32_t map_id = 0;
	mutable std::shared_mutex mapcode_mutex;
};
