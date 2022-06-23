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
		const auto id = get_id();
		if (!id.empty()) {
			cpr::Post(
				cpr::Url{ server_url + "groups/" + id + "/" + method },
				cpr::Body{ payload.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		}
	});
	thread.detach();
}

void API::check_serverstatus() {
	auto response = cpr::Get(
		cpr::Url{ server_url },
		cpr::Timeout{ 1000 }
	);
	if (response.status_code != cpr::status::HTTP_OK) {
		log("timer: failed to connect to timer api, forcing offline mode\n");
		server_status = ServerStatus::offline;
	}
	else {
		const auto data = nlohmann::json::parse(response.text);

		constexpr int server_version = 8;
		if (data["version"] != server_version) {
			log("timer: out of date version, going offline mode\n");
			server_status = ServerStatus::outofdate;
		}
		else {
			server_status = ServerStatus::online;
		}
	}
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
		if (server_status == ServerStatus::outofdate) {
			return;
		}

		if (server_status == ServerStatus::online) {
			const std::string id = get_id();
			if (id.empty()) {
				std::this_thread::sleep_for(std::chrono::seconds{ 1 });
				continue;
			}

			const nlohmann::json payload{ {"update_id", update_id} };
			cpr::AsyncResponse response_future = cpr::PostAsync(
				cpr::Url{ server_url + "groups/" + id + "/" },
				cpr::Body{ payload.dump() },
				cpr::Header{ {"Content-Type", "application/json"} },
				cpr::ProgressCallback([&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) {
					return id == get_id();
				})
			);

			auto response = response_future.get();
			if (response.error.code == cpr::ErrorCode::REQUEST_CANCELLED) {
				continue;
			}
			if (response.status_code != cpr::status::HTTP_OK) {
				log("timer: failed to sync with server");
				server_status = ServerStatus::offline;
				continue;
			}

			try {
				auto data = nlohmann::json::parse(response.text);
				update_id = data["update_id"];

				data_function(data);
			}
			catch ([[maybe_unused]] const nlohmann::json::parse_error& e) {
				log("timer: received no json response from server");
				server_status = ServerStatus::offline;
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds{ 3 });
			if (server_status == ServerStatus::offline) {
				check_serverstatus();
			}
		}
	}
}
