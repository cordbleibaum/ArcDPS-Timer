#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <mutex>
#include <variant>
#include <optional>
#include <shared_mutex>

#include <boost/signals2.hpp>
#include <nlohmann/json.hpp>

#include "uuid.h"

#include "api.h"

enum class EventType {
	start,
	stop,
	reset,
	prepare,
	segment,
	segment_clear,
	map_change,
	history_clear
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
	{EventType::start, "start"},
	{EventType::stop, "stop"},
	{EventType::reset, "reset"},
	{EventType::prepare, "prepare"},
	{EventType::segment, "segment"},
	{EventType::segment_clear, "segment_clear"},
	{EventType::map_change, "map_change"},
	{EventType::history_clear, "history_clear"}
});

enum class EventSource {
	manual,
	combat,
	movement,
	other
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventSource, {
	{EventSource::manual, "manual"},
	{EventSource::combat, "combat"},
	{EventSource::movement, "movement"},
	{EventSource::other, "other"}
});

enum class TimerStatus {
	stopped,
	prepared,
	running
};

struct TimerState {
	TimerStatus status;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	bool was_prepared;
};

struct TimeSegment {
	bool is_set = false;
	std::string name = "";
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
	std::chrono::system_clock::duration shortest_duration = std::chrono::system_clock::duration::zero();
	std::chrono::system_clock::duration shortest_time = std::chrono::system_clock::duration::zero();
};

struct HistoryEntry {
	HistoryEntry(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end, std::string name);

	std::string name = "";
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
};

struct EventLogEntry {
	EventLogEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source);
	EventLogEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid);
	EventLogEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid, std::string name);
	EventLogEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, std::string name);

	std::chrono::system_clock::time_point time;
	EventType type;
	EventSource source;
	boost::uuids::uuid uuid;
	std::optional<std::string> name;

	bool is_relevant = true;
};

class EventStore {
public:
	EventStore(API& api);
	void dispatch_event(EventLogEntry entry);
	TimerState get_timer_state();
	std::vector<HistoryEntry> get_history();

	double clock_offset = 0;
private:
	API& api;

	std::shared_mutex log_mutex;
	std::vector<EventLogEntry> entries;
	std::vector<HistoryEntry> history;

	TimerState state;

	void reevaluate_state();
	void sync(const nlohmann::json& data);
	void add_event(EventLogEntry entry);
	std::string format_time(std::chrono::system_clock::time_point time);
};