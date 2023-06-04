  #include "api.h"

#include <cpr/cpr.h>
#include <thread>

#include "arcdps.h"

API::API(const Settings& settings, GW2MumbleLink& mumble_link, MapTracker& map_tracker, GroupTracker& group_tracker, std::string server_url)
:   server_url(server_url),
	settings(settings),
	mumble_link(mumble_link),
	map_tracker(map_tracker),
	group_tracker(group_tracker) {
}

void API::post_serverapi(std::string method, nlohmann::json payload) {
	std::thread thread([&, method, payload]() {
		// TODO
	});
	thread.detach();
}

void API::check_serverstatus() {
	// TODO
}

std::string API::get_id() const {
	if (settings.use_custom_id) {
		return settings.custom_id + "_custom";
	}
	if (mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE) {
		return map_tracker.get_instance_id();
	}
	return group_tracker.get_group_id();
}

void API::sync(std::function<void(const nlohmann::json&)> data_function) {
	while (true) {
		// TODO
	}
}
