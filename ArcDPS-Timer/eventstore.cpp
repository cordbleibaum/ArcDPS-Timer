#include "eventstore.h"
#include "util.h"

#include <set>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

EventStore::EventStore(API& api, const Settings& settings)
:	api(api),
	settings(settings) {
	if (!std::filesystem::exists(logs_directory)) {
		std::filesystem::create_directory(logs_directory);
	}
}

void EventStore::dispatch_event(EventEntry entry) {
	if (entry.type == EventType::start && entry.source == EventSource::combat) {
		TimerState state = get_timer_state();

		const std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - state.start_time;
		const double duration = duration_dbl.count();

		if (state.status == TimerStatus::running && duration < 3 && state.was_prepared) {
			add_event(entry);
		}
		else if (state.status == TimerStatus::prepared) {
			add_event(entry);
		}
	}
	else if (entry.type == EventType::map_change) {
		save_map_log();
		add_event(entry);
	}
	else {
		add_event(entry);
	}
}

TimerState EventStore::get_timer_state() {
	if (state.status == TimerStatus::running) {
		state.current_time = std::chrono::system_clock::now();
	}

	return state;
}

std::vector<HistoryEntry> EventStore::get_history() {
	return history;
}

std::vector<TimeSegment> EventStore::get_segments() {
	return segments;
}

void EventStore::save_log_thread(std::vector<EventEntry> entries) {
	std::sort(entries.begin(), entries.end());

	std::string map_name;
	std::vector<EventEntry> log_entries;

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const EventEntry& entry = *it;
		if (entry.type == EventType::map_change) {
			map_name = entry.name.value_or("Unknown");
			break;
		}
		else {
			log_entries.push_back(*it);
		}
	}

	std::sort(log_entries.begin(), log_entries.end());

	if (log_entries.size() < 1 || !settings.save_logs) {
		return;
	}

	const std::string log_path = logs_directory + map_name + "/";
	if (!std::filesystem::exists(log_path)) {
		std::filesystem::create_directory(log_path);
	}

	const json events_json = log_entries;
	std::string name = std::format("{:%FT%H-%M-%S}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
	std::ofstream o(log_path + name + ".json");
	o << std::setw(4) << events_json << std::endl;
}

void EventStore::save_map_log() {
	std::vector<EventEntry> current_events(entries);
	defer([&, current_events]() {
		save_log_thread(current_events);
	});
}

void EventStore::mod_release() {
	save_log_thread(entries);
}

void EventStore::start_sync() {
	api.start_sync(std::bind(&EventStore::sync, this, std::placeholders::_1));
}

void EventStore::reevaluate_state() {
	std::sort(entries.begin(), entries.end());

	TimerStatus status = TimerStatus::stopped;
	auto start = std::chrono::system_clock::time_point::max();
	auto stop = std::chrono::system_clock::time_point::max();
	auto segment = std::chrono::system_clock::time_point::max();
	std::vector<TimeSegment>::size_type segment_index = 0;
	bool was_prepared = false;

	std::set<boost::uuids::uuid> processed_uuids;
	history.clear();
	segments.clear();

	std::string current_map = "Unknown";

	// Event log evaluation FSM
	for (auto& entry : entries) {
		if (processed_uuids.contains(entry.uuid)) {
			entry.is_relevant = false;
			continue;
		}

		if (entry.type == EventType::map_change) {
			current_map = entry.name.value_or("Unknown");
			segment_index = 0;

			start = entry.time;
			stop = entry.time;
			status = TimerStatus::stopped;
		}
		else if (entry.type == EventType::reset) {
			start = entry.time;
			stop = entry.time;
			status = TimerStatus::stopped;

			for (auto& current_segment : segments) {
				current_segment.is_set = false;
			}
		}
		else if (entry.type == EventType::history_clear) {
			history.clear();
		}
		else if (entry.type == EventType::segment_clear) {
			segments.clear();
			segment_index = 0;
		}
		else if (entry.type == EventType::none) {
			entry.is_relevant = false;
		}
		else {
			switch (status) {
			case TimerStatus::running:
				if (entry.type == EventType::stop) {
					stop = entry.time;
					status = TimerStatus::stopped;
					history.emplace_back(start, stop, current_map);

					if (segments.size() == segment_index) {
						segments.emplace_back();
					}

					auto& current_segment = segments[segment_index++];
					current_segment.is_set = true;
					current_segment.start = segment;
					current_segment.end = stop;

					std::chrono::system_clock::duration time = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
					std::chrono::system_clock::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - segment);
					current_segment.shortest_time = std::min(current_segment.shortest_time, time);
					current_segment.shortest_duration = std::min(current_segment.shortest_duration, duration);

					segment = entry.time;
				}
				else if (entry.type == EventType::segment) {
					if (segments.size() == segment_index) {
						segments.emplace_back();
					}

					auto& current_segment = segments[segment_index++];
					current_segment.is_set = true;
					current_segment.start = segment;
					current_segment.end = entry.time;

					std::chrono::system_clock::duration time = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
					std::chrono::system_clock::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - segment);
					current_segment.shortest_time = std::min(current_segment.shortest_time, time);
					current_segment.shortest_duration = std::min(current_segment.shortest_duration, duration);

					current_segment.name = entry.name.value_or("");

					segment = entry.time;
				}
				else {
					entry.is_relevant = false;
				}
				break;
			case TimerStatus::stopped:
				if (entry.type == EventType::start) {
					start = entry.time;
					segment = entry.time;
					segment_index = 0;
					status = TimerStatus::running;
					was_prepared = false;

					for (auto& current_segment : segments) {
						current_segment.is_set = false;
					}
				}
				else if (entry.type == EventType::prepare) {
					start = entry.time;
					stop = entry.time;
					status = TimerStatus::prepared;
				}
				else {
					entry.is_relevant = false;
				}
				break;
			case TimerStatus::prepared:
				if (entry.type == EventType::start) {
					start = entry.time;
					segment = entry.time;
					segment_index = 0;
					status = TimerStatus::running;
					was_prepared = true;

					for (auto& current_segment : segments) {
						current_segment.is_set = false;
					}
				}
				else if (entry.type == EventType::stop) {
					stop = entry.time;
					status = TimerStatus::stopped;
				}
				else {
					entry.is_relevant = false;
				}
				break;
			}
		}

		if (entry.is_relevant) {
			processed_uuids.insert(entry.uuid);
		}
	}

	std::erase_if(entries, [&](const auto& entry) {
		return !entry.is_relevant;
	});

	state.status = status;
	state.start_time = start;
	state.current_time = stop;
	state.was_prepared = was_prepared;
}

void EventStore::sync(const nlohmann::json& data) {
	std::lock_guard<std::shared_mutex> lock(log_mutex);

	entries.push_back(EventEntry(
		std::chrono::system_clock::time_point(std::chrono::milliseconds(data["time"].get<int64_t>())),
		data["type"].get<EventType>(),
		data["source"].get<EventSource>(),
		data["uuid"].get<boost::uuids::uuid>()
	));

	reevaluate_state();
}

void EventStore::add_event(EventEntry entry) {
	std::lock_guard<std::shared_mutex> lock(log_mutex);

	entries.push_back(entry);
	reevaluate_state();

	api.post_serverapi("event", {
		{ "time", format_time(entry.time)},
		{ "type", entry.type },
		{ "source", entry.source },
		{ "uuid", entry.uuid }
	});
}

std::string EventStore::format_time(std::chrono::system_clock::time_point time) {
	return std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clock_offset * 1000.0))));
}

EventEntry::EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source) 
: time(time),
	type(type),
	source(source) {
	uuid = boost::uuids::random_generator()();
}

EventEntry::EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid) 
:	time(time),
	type(type),
	source(source),
	uuid(uuid) {
}

EventEntry::EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid, std::string name)
:   time(time),
	type(type),
	source(source),
	uuid(uuid),
	name(name) {
}

EventEntry::EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, std::string name)
:	time(time),
	type(type),
	source(source),
	name(name) {
	uuid = boost::uuids::random_generator()();
}

HistoryEntry::HistoryEntry(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end, std::string name)
:	start(start),
	end(end),
	name(name) {
}
