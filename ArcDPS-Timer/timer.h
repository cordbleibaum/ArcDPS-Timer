#pragma once

#include "arcdps.h"
#include "settings.h"
#include "mumble_link.h"
#include "grouptracker.h"
#include "maptracker.h"
#include "lang.h"
#include "api.h"

#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <shared_mutex>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cpr/cpr.h>
#include <boost/signals2.hpp>

enum class TimerStatus { stopped, prepared, running };

struct TimeSegment {
	bool is_set = false;
	std::string name = "";
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
	std::chrono::system_clock::duration shortest_duration = std::chrono::system_clock::duration::zero();
	std::chrono::system_clock::duration shortest_time = std::chrono::system_clock::duration::zero();
};

class Timer {
public:
	Timer(Settings& settings, GW2MumbleLink& mumble_link, Translation& translation, API& api);
	void start(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void stop(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void reset();
	void prepare();
	void segment(bool local = false, std::string name = "");
	void clear_segments();
	void map_change(uint32_t map_id);
	void bosskill(uint64_t time);

	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);
	void mod_imgui();

	double clock_offset = 0;

	boost::signals2::signal<void()> segment_reset_signal;
	boost::signals2::signal<void(std::chrono::system_clock::time_point)> start_signal;
	boost::signals2::signal<void(std::chrono::system_clock::time_point)> stop_signal;
	boost::signals2::signal<void(std::chrono::system_clock::time_point)> reset_signal;
	boost::signals2::signal<void(std::chrono::system_clock::time_point)> prepare_signal;
	boost::signals2::signal<void(int, std::chrono::system_clock::time_point)> segment_signal;
private:
	Settings& settings;
	GW2MumbleLink& mumble_link;
	Translation& translation;
	API& api;

	TimerStatus status;
	std::shared_mutex timerstatus_mutex;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	std::vector<TimeSegment> segments;
	std::shared_mutex segmentstatus_mutex;
	std::array<float, 3> last_position;

	void sync(nlohmann::json data);
	std::string format_time(std::chrono::system_clock::time_point time);
	void reset_segments();
	void timer_window_content(float width = ImGui::GetWindowSize().x);
	void segment_window_content();
};
