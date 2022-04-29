#include "timer.h"
#include "util.h"

#include "hash-library/crc32.h"

Timer::Timer(Settings& settings, GW2MumbleLink& mumble_link, GroupTracker& group_tracker, Translation& translation)
:	settings(settings),
	mumble_link(mumble_link),
	group_tracker(group_tracker),
	translation(translation)
{
	start_time = current_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;
	check_serverstatus();

	std::thread status_sync_thread(&Timer::sync, this);
	status_sync_thread.detach();
}

void Timer::post_serverapi(std::string method, json payload) {
	std::thread thread([&, method, payload]() {
		std::string id = get_id();
		if (id != "") {
			cpr::Post(
				cpr::Url{ settings.server_url + "groups/" + id + "/" + method },
				cpr::Body{ payload.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		}
	});
	thread.detach();
}

std::string Timer::format_time(std::chrono::system_clock::time_point time) {
	return std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clock_offset * 1000.0))));
}

void Timer::start(std::chrono::system_clock::time_point time) {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::running;
	start_time = time;
	current_time = std::chrono::system_clock::now();
	post_serverapi("start", {{"time", format_time(start_time)}});
	reset_segments();
}

void Timer::stop(std::chrono::system_clock::time_point time) {
	if (status != TimerStatus::stopped || current_time > time) {
		std::unique_lock lock(timerstatus_mutex);

		segment();

		status = TimerStatus::stopped;
		current_time = time;
		post_serverapi("stop", {{"time", format_time(current_time)}});
	}
}

void Timer::reset() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::stopped;
	start_time = current_time = std::chrono::system_clock::now();
	reset_segments();
	post_serverapi("reset");
}

void Timer::prepare() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::prepared;
	start_time = current_time = std::chrono::system_clock::now();
	std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
	reset_segments();
	post_serverapi("prepare");
}

void Timer::sync() {
	while (true) {
		if (serverStatus == ServerStatus::outofdate) {
			return;
		}

		if (serverStatus == ServerStatus::online) {
			std::string id = get_id();
			if (id == "") {
				std::this_thread::sleep_for(std::chrono::seconds{ 1 });
				continue;
			}

			std::string url = settings.server_url;
			json payload{{"update_id", update_id}};
			cpr::AsyncResponse response_future = cpr::PostAsync(
				cpr::Url{ url + "groups/" + id + "/" },
				cpr::Body{ payload.dump() },
				cpr::Header{{"Content-Type", "application/json"}},
				cpr::ProgressCallback([&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) {
					return id == get_id() && url == settings.server_url;
				})
			);

			auto response = response_future.get();
			if (response.error.code == cpr::ErrorCode::REQUEST_CANCELLED) {
				continue;
			}
			if (response.status_code != cpr::status::HTTP_OK) {
				log("timer: failed to sync with server");
				serverStatus = ServerStatus::offline;
				continue;
			}

			try {
				auto data = json::parse(response.text);

				{
					std::unique_lock lock(timerstatus_mutex);

					start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
					update_id = data["update_id"];
					if (data["status"] == "running") {
						status = TimerStatus::running;
					}
					else if (data["status"] == "stopped") {
						status = TimerStatus::stopped;
						current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
					}
					else if (data["status"] == "prepared") {
						status = TimerStatus::prepared;
						current_time = std::chrono::system_clock::now();
						std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
					}
				}

				{
					std::unique_lock lock(segmentstatus_mutex);

					segments.clear();
					for (json::iterator it = data["segments"].begin(); it != data["segments"].end(); ++it) {
						TimeSegment segment;
						segment.is_set = (*it)["is_set"];
						segment.start = parse_time((*it)["start"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						segment.end = parse_time((*it)["end"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						segment.shortest_time = std::chrono::milliseconds{ (*it)["shortest_time"] };
						segment.shortest_duration = std::chrono::milliseconds{ (*it)["shortest_duration"] };
						segments.push_back(segment);
					}
				}
			}
			catch ([[maybe_unused]] const json::parse_error& e) {
				log("timer: received no json response from server");
				serverStatus = ServerStatus::offline;
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds{ 3 });
			if (serverStatus == ServerStatus::offline) {
				check_serverstatus();
			}
		}
	}
}

void Timer::check_serverstatus() {
	auto response = cpr::Get(
		cpr::Url{ settings.server_url },
		cpr::Timeout{ 1000 }
	);
	if (response.status_code != cpr::status::HTTP_OK) {
		log("timer: failed to connect to timer api, forcing offline mode\n");
		serverStatus = ServerStatus::offline;
	}
	else {
		auto data = json::parse(response.text);
		constexpr int server_version = 8;
		if (data["version"] != server_version) {
			log("timer: out of date version, going offline mode\n");
			serverStatus = ServerStatus::outofdate;
		}
		else {
			serverStatus = ServerStatus::online;
		}
	}
}

std::string Timer::get_id() {
	std::string id = "";
	if (settings.use_custom_id) {
		id = settings.custom_id + "_custom";
	}
	else if (isInstanced) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);
		id = map_code;
	}
	else {
		id = group_tracker.get_group_id();
	}
	return id;
}

void Timer::reset_segments() {
	std::unique_lock lock(segmentstatus_mutex);

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	segment_reset_signal();
}

void Timer::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id) {
	if (ev) {
		if (settings.auto_stop) {
			if (src && src->prof > 9) {
				std::scoped_lock<std::mutex> guard(logagents_mutex);
				log_agents.insert(src->prof);
			}
			if (dst && dst->prof > 9) { // TODO: make sure minions, pets, etc get excluded
				std::scoped_lock<std::mutex> guard(logagents_mutex);
				log_agents.insert(dst->prof);

				if ((!ev->is_buffremove && !ev->is_activation && !ev->is_statechange) || (ev->buff && ev->buff_dmg)) {
					last_damage_ticks = calculate_ticktime(ev->time);
				}
			}
		}

		if (ev->is_activation) {
			std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - start_time;
			double duration = duration_dbl.count();

			if (status == TimerStatus::prepared || (status == TimerStatus::running && duration < 3)) {
				log_debug("timer: starting on skill");
				start(calculate_ticktime(ev->time));
			}
		}
		else if (settings.auto_stop) {
			if (ev->is_statechange == cbtstatechange::CBTS_LOGEND) {
				std::scoped_lock<std::mutex> guard(logagents_mutex);
				uintptr_t log_species_id = ev->src_agent;

				auto log_duration = std::chrono::system_clock::now() - log_start_time;
				if (log_duration > std::chrono::seconds(settings.early_gg_threshold)) {
					std::set<uintptr_t> last_bosses = {
						11265, // Swampland - Bloomhunger 
						11239, // Underground Facility - Dredge
						11240, // Underground Facility - Elemental
						11485, // Volcanic - Imbued Shaman
						11296, // Cliffside - Archdiviner
						19697, // Mai Trin Boss Fractal - Mai Trin
						12906, // Thaumanova - Thaumanova Anomaly
						11333, // Snowblind - Shaman
						11402, // Aquatic Ruins - Jellyfish Beast
						16617, // Chaos - Gladiator
						20497, // Deepstone - The Voice
						12900, // Molten Furnace - Engineer
						17051, // Nightmare - Ensolyss
						16948, // Nightmare CM - Ensolyss
						17830, // Shattered Observatory - Arkk
						17759, // Shattered Observatory - Arkk CM
						11408, // Urban Battleground - Captain Ashym
						19664, // Twilight Oasis - Amala
						21421, // Sirens Reef - Captain Crowe
						11328, // Uncategorized - Asura
						12898, // Molten Boss - Berserker
						12267, // Aetherblade - Frizz
					};

					bool is_bosslog_end = std::find(std::begin(last_bosses), std::end(last_bosses), log_species_id) != std::end(last_bosses);
					bool is_ooc_end = false;

					for (const auto& agent_species_id : log_agents) {
						is_ooc_end |= std::find(std::begin(last_bosses), std::end(last_bosses), agent_species_id) != std::end(last_bosses);
					}

					log_agents.clear();

					if (is_bosslog_end) {
						log_debug("timer: stopping on log end");
						stop(calculate_ticktime(ev->time));
					}
					else if (is_ooc_end) {
						log_debug("timer: stopping on ooc after boss");
						stop(last_damage_ticks);
					}
				}
			}
			else if (ev->is_statechange == cbtstatechange::CBTS_LOGSTART) {
				log_start_time = std::chrono::system_clock::now() - std::chrono::milliseconds{ timeGetTime() - ev->time };
			}
		}
	}
}

void Timer::mod_imgui() {
	if (settings.disable_outside_instances && !isInstanced) {
		return;
	}

	if (status == TimerStatus::prepared) {
		if (checkDelta(last_position[0], mumble_link->fAvatarPosition[0], 0.1f) ||
			checkDelta(last_position[1], mumble_link->fAvatarPosition[1], 0.1f) ||
			checkDelta(last_position[2], mumble_link->fAvatarPosition[2], 0.1f)) {
			log_debug("timer: starting on movement");
			start();
		}
	}

	if (status == TimerStatus::running) {
		current_time = std::chrono::system_clock::now();
	}

	if (settings.show_timer) {
		std::shared_lock lock(timerstatus_mutex);

		ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		auto duration = std::chrono::round<std::chrono::milliseconds>(current_time - start_time);
		std::string time_string = "";
		try {
			time_string = std::format(settings.time_formatter, duration);
		}
		catch ([[maybe_unused]] const std::exception& e) {
			time_string = translation.get("TimeFormatterInvalid");
		}

		ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + 3);
		switch (status) {
		case TimerStatus::stopped:
			ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), translation.get("MarkerStopped").c_str());
			break;
		case TimerStatus::prepared:
			ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), translation.get("MarkerPrepared").c_str());
			break;
		case TimerStatus::running:
			ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), translation.get("MarkerRunning").c_str());
			break;
		}

		auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		if (!settings.hide_buttons) {
			if (ImGui::Button(translation.get("ButtonPrepare").c_str(), ImVec2(190, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: preparing manually");
				defer([&]() {
					prepare();
				});
			}

			if (ImGui::Button(translation.get("ButtonStart").c_str(), ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: starting manually");
				defer([&]() {
					start();
				});
			}

			ImGui::SameLine(0, 5);
			if (ImGui::Button(translation.get("TextStop").c_str(), ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: stopping manually");
				defer([&]() {
					stop();
				});
			}

			ImGui::SameLine(0, 5);
			if (ImGui::Button(translation.get("TextReset").c_str(), ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: resetting manually");
				defer([&]() {
					reset();
				});
			}
		}
		else {
			ImGui::Dummy(ImVec2(160, 0));
		}

		if (serverStatus == ServerStatus::outofdate) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), translation.get("TextOutOfDate").c_str());
		}

		ImGui::End();
	}

	if (settings.show_segments) {
		bool is_visible = ImGui::Begin(translation.get("HeaderSegments").c_str(), &settings.show_segments, ImGuiWindowFlags_AlwaysAutoResize);

		std::shared_lock lock(segmentstatus_mutex);

		if (is_visible) {
			ImGui::BeginTable("##segmenttable", 3);
			ImGui::TableSetupColumn(translation.get("HeaderNumColumn").c_str());
			ImGui::TableSetupColumn(translation.get("HeaderLastColumn").c_str());
			ImGui::TableSetupColumn(translation.get("HeaderBestColumn").c_str());
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < segments.size(); ++i) {
				const auto& segment = segments[i];

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text(std::to_string(i).c_str());

				ImGui::TableNextColumn();
				if (segment.is_set) {
					auto time_total = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
					auto duration_segment = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
					std::string text = std::format("{0:%M:%S}", time_total) + std::format(" ({0:%M:%S})", duration_segment);
					ImGui::Text(text.c_str());
				}

				ImGui::TableNextColumn();
				auto shortest_time = std::chrono::round<std::chrono::milliseconds>(segment.shortest_time);
				auto shortest_duration = std::chrono::round<std::chrono::milliseconds>(segment.shortest_duration);
				std::string text = std::format("{0:%M:%S}", shortest_time) + std::format(" ({0:%M:%S})", shortest_duration);
				ImGui::Text(text.c_str());
			}

			ImGui::EndTable();

			if (ImGui::Button(translation.get("ButtonSegment").c_str())) {
				defer([&]() {
					segment();
				});
			}

			ImGui::SameLine(0, 5);
			if (ImGui::Button(translation.get("ButtonClearSegments").c_str())) {
				defer([&]() {
					clear_segments();
				});
			}
		}

		ImGui::End();
	}
}

void Timer::segment() {
	if (status != TimerStatus::running) return;

	std::unique_lock lock(segmentstatus_mutex);

	int segment_num = 0;
	for (segment_num = 0; segment_num < segments.size() && segments[segment_num].is_set; ++segment_num) {}

	bool is_new = false;
	if (segment_num == segments.size()) {
		segments.emplace_back();
		is_new = true;
	}

	auto& segment = segments[segment_num];
	segment.is_set = true;
	segment.end = current_time;
	segment.start = segment_num > 0 ? segments[segment_num - 1].end : start_time;

	auto time = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
	auto duration = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
	if (is_new || time < segment.shortest_time) {
		segment.shortest_time = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
	}
	if (is_new || duration < segment.shortest_duration) {
		segment.shortest_duration = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
	}

	post_serverapi("segment", {
		{"segment_num", segment_num},
		{"time", format_time(segment.end)}
	});
}

void Timer::clear_segments() {
	{
		std::unique_lock lock(segmentstatus_mutex);

		segments.clear();
		post_serverapi("clear_segment");
	}

	reset_segments();
}

void Timer::map_change() {
	std::scoped_lock<std::mutex> guard(mapcode_mutex);

	map_code = CRC32()(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
	isInstanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;

	if (settings.auto_prepare && isInstanced) {
		log_debug("timer: preparing on map change");
		prepare();
	}
}
