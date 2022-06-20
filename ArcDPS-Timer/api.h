#pragma once

#include "settings.h"
#include "mumble_link.h"
#include "maptracker.h"
#include "grouptracker.h"

#include <string>
#include <nlohmann/json.hpp>
#include <functional>

enum class ServerStatus { online, offline, outofdate };

class API {
public:
	API(const Settings& settings, GW2MumbleLink& mumble_link, MapTracker& map_tracker, GroupTracker& group_tracker, std::string server_url);
	void post_serverapi(std::string method, nlohmann::json payload = nlohmann::json::object());
	void check_serverstatus();
	void sync(std::function<void(nlohmann::json)> data_function);

	ServerStatus server_status = ServerStatus::online;
private:
	const Settings& settings;
	GW2MumbleLink& mumble_link;
	MapTracker& map_tracker;
	GroupTracker& group_tracker;

	const std::string server_url;
	int update_id = -2;

	std::string get_id() const;
};