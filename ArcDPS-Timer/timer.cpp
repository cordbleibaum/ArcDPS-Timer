#include "timer.h"
#include "util.h"

Timer::Timer(Settings& settings, GW2MumbleLink& mumble_link, GroupTracker& group_tracker, Translation& translation, MapTracker& map_tracker, std::string server_url)
:	settings(settings),
	mumble_link(mumble_link),
	group_tracker(group_tracker),
	translation(translation),
	map_tracker(map_tracker),
	server_url(server_url)
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
				cpr::Url{ server_url + "groups/" + id + "/" + method },
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
	start_signal(start_time);
}

void Timer::stop(std::chrono::system_clock::time_point time) {
	if (status != TimerStatus::stopped || current_time > time) {
		std::unique_lock lock(timerstatus_mutex);

		segment();

		status = TimerStatus::stopped;
		current_time = time;
		post_serverapi("stop", {{"time", format_time(current_time)}});
		stop_signal(current_time);
	}
}

void Timer::reset() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::stopped;
	start_time = current_time = std::chrono::system_clock::now();
	reset_segments();
	post_serverapi("reset");
	reset_signal(current_time);
}

void Timer::prepare() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::prepared;
	start_time = current_time = std::chrono::system_clock::now();
	std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
	reset_segments();
	post_serverapi("prepare");
	prepare_signal(current_time);
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

			json payload{{"update_id", update_id}};
			cpr::AsyncResponse response_future = cpr::PostAsync(
				cpr::Url{ server_url + "groups/" + id + "/" },
				cpr::Body{ payload.dump() },
				cpr::Header{{"Content-Type", "application/json"}},
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
						start_signal(start_time);
						status = TimerStatus::running;
					}
					else if (data["status"] == "stopped") {
						current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						stop_signal(current_time);
						status = TimerStatus::stopped;
					}
					else if (data["status"] == "prepared") {
						current_time = start_time = std::chrono::system_clock::now();
						prepare_signal(current_time);
						status = TimerStatus::prepared;
						std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
					}
				}

				{
					std::unique_lock lock(segmentstatus_mutex);

					segments.clear();
					int i = 0;
					for (json::iterator it = data["segments"].begin(); it != data["segments"].end(); ++it) {
						TimeSegment segment;
						segment.is_set = (*it)["is_set"];
						segment.start = parse_time((*it)["start"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						segment.end = parse_time((*it)["end"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						segment.shortest_time = std::chrono::milliseconds{ (*it)["shortest_time"] };
						segment.shortest_duration = std::chrono::milliseconds{ (*it)["shortest_duration"] };
						segment_signal(i++, segment.start);
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
		cpr::Url{ server_url },
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

std::string Timer::get_id() const {
	if (settings.use_custom_id) {
		return settings.custom_id + "_custom";
	}
	if (mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE) {
		return map_tracker.get_instance_id();
	}
	return group_tracker.get_group_id();
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
	if (settings.disable_outside_instances && mumble_link->getMumbleContext()->mapType != MapType::MAPTYPE_INSTANCE) {
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

	if (!settings.unified_window) {
		if (settings.show_timer) {
			if (ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
				timer_window_content(190);
			}
			ImGui::End();
		}

		if (settings.show_segments) {
			if (ImGui::Begin(translation.get("HeaderSegments").c_str(), &settings.show_segments, ImGuiWindowFlags_AlwaysAutoResize)) {
				segment_window_content();
			}
			ImGui::End();
		}
	}

	if (settings.unified_window && settings.show_timer) {
		ImGui::SetNextWindowSize(ImVec2(350, 0));
		if (ImGui::Begin("Timer+Segments", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
			timer_window_content(ImGui::GetWindowSize().x - 2*ImGui::GetStyle().WindowPadding.x);
			
			ImGui::Dummy(ImVec2(0.0f, 3.0f));

			if (ImGui::CollapsingHeader(translation.get("HeaderSegments").c_str())) {
				segment_window_content();
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

	segment_signal(segment_num, current_time);
}

void Timer::clear_segments() {
	std::unique_lock lock(segmentstatus_mutex);

	segments.clear();
	post_serverapi("clear_segment");
	segment_reset_signal();
}

void Timer::map_change(uint32_t map_id) {
	if (settings.auto_prepare && mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE) {
		defer([&]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			log_debug("timer: preparing on map change");
			prepare();
		});
	}
}

void Timer::timer_window_content(float width) {
	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	{
		std::shared_lock lock(timerstatus_mutex);

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
		ImGui::SetCursorPosX((width - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());
	}

	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	if (!settings.hide_buttons) {
		ImGui::PushStyleColor(ImGuiCol_Button, settings.prepare_button_color);
		if (ImGui::Button(translation.get("ButtonPrepare").c_str(), ImVec2(width, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: preparing manually");
			prepare();
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.start_button_color);
		if (ImGui::Button(translation.get("ButtonStart").c_str(), ImVec2((width-10)/3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: starting manually");
			start();
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.stop_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("TextStop").c_str(), ImVec2((width - 10) / 3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: stopping manually");
			stop();
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.reset_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("TextReset").c_str(), ImVec2((width - 10) / 3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: resetting manually");
			reset();
		}
		ImGui::PopStyleColor();
	}
	else {
		ImGui::Dummy(ImVec2(160, 0));
	}

	if (serverStatus == ServerStatus::outofdate) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), translation.get("TextOutOfDate").c_str());
	}
}

void Timer::segment_window_content() {
	{
		std::shared_lock lock(segmentstatus_mutex);

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
	}

	if (ImGui::Button(translation.get("ButtonSegment").c_str())) {
		segment();
	}

	ImGui::SameLine(0, 5);
	if (ImGui::Button(translation.get("ButtonClearSegments").c_str())) {
		clear_segments();
	}
}
