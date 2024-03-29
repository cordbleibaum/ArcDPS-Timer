#pragma once

#include "eventstore.h"
#include "arcdps.h"
#include "settings.h"
#include "mumble_link.h"
#include "grouptracker.h"
#include "maptracker.h"
#include "lang.h"

#include <chrono>
#include <array>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cpr/cpr.h>
#include <boost/signals2.hpp>

class Timer {
public:
	Timer(EventStore& store, Settings& settings, GW2MumbleLink& mumble_link, const Translation& translation, MapTracker& map_tracker);
	void map_change(uint32_t map_id);

	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);
	void mod_imgui();

	double clock_offset = 0;
private:
	Settings& settings;
	GW2MumbleLink& mumble_link;
	const Translation& translation;
	MapTracker& map_tracker;
	EventStore& store;

	std::array<float, 3> last_position;

	void timer_window_content(float width = ImGui::GetWindowSize().x);
	void segment_window_content();
	void history_window_content();
};
