#include "eventstore.h"
#include "util.h"

#include <set>

using json = nlohmann::json;

EventStore::EventStore(API& api) 
:	api(api) {
	api.check_serverstatus();

	std::thread status_sync_thread([&]() {
		api.sync(std::bind(&EventStore::sync, this, std::placeholders::_1));
	});
	status_sync_thread.detach();
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

void EventStore::reevaluate_state() {
	std::sort(entries.begin(), entries.end(), [](const EventEntry& a, const EventEntry& b) {
		return a.time < b.time;
	});

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
					current_segment.shortest_time = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
					current_segment.shortest_duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - segment);

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
					current_segment.shortest_time = std::chrono::duration_cast<std::chrono::milliseconds>(entry.time - start);
					current_segment.shortest_duration = std::chrono::duration_cast<std::chrono::milliseconds>(entry.time - segment);

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

	for (auto& entry : data) {
		entries.push_back(EventEntry(
			std::chrono::system_clock::time_point(std::chrono::milliseconds(entry["time"].get<int64_t>())),
			entry["type"].get<EventType>(),
			entry["source"].get<EventSource>(),
			entry["uuid"].get<boost::uuids::uuid>()
		));
	}

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