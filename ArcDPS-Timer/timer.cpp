#include "timer.h"

#include "hash-library/crc32.h"

Timer::Timer(Settings& settings, GW2MumbleLink& mumble_link, GroupTracker& group_tracker) :
	settings(settings),
	mumble_link(mumble_link),
	group_tracker(group_tracker)
{
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

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
	}

	std::thread status_sync_thread([&]() {
		sync();
	});
	status_sync_thread.detach();
}

cpr::Response Timer::post_serverapi(std::string method, const json& payload) {
	std::string id = get_id();
	if (id != "") {
		return cpr::Post(
			cpr::Url{ settings.server_url + "groups/" + id + "/" + method },
			cpr::Body{ payload.dump() },
			cpr::Header{ {"Content-Type", "application/json"} }
		);
	}
	return cpr::Response();
}

std::string Timer::format_time(std::chrono::system_clock::time_point time) {
	return std::format(
		"{:%FT%T}",
		std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clock_offset * 1000.0)))
	);
}

void Timer::start(std::chrono::system_clock::time_point time) {
	status = TimerStatus::running;
	start_time = time;
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	network_thread([&] {
		post_serverapi("stop", {
			{"time", format_time(start_time)},
			{"", format_time(update_time)}
		});
	});

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	if (segments.size() > 0) {
		segments[0].start = start_time;
	}
}

void Timer::stop(std::chrono::system_clock::time_point time) {
	if (status != TimerStatus::stopped || current_time > time) {
		segment();

		status = TimerStatus::stopped;
		current_time = time;
		update_time = std::chrono::system_clock::now();

		network_thread([&] {
			post_serverapi("stop", {
				{"time", format_time(current_time)},
				{"update_time", format_time(update_time)}
			});
		});
	}
}

void Timer::reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();

	network_thread([&]() {
		post_serverapi("reset", {{"update_time", format_time(update_time)}});
	});

	for (auto& segment : segments) {
		segment.is_set = false;
	}
}

void Timer::prepare() {
	status = TimerStatus::prepared;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(lastPosition));
	update_time = std::chrono::system_clock::now();

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	network_thread([&]() {
		post_serverapi("prepare", {{"update_time", format_time(update_time)}});
	});
}

std::chrono::system_clock::time_point parse_time(const std::string& source) {
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}

void Timer::sync() {
	while (true) {
		if (!settings.is_offline_mode && serverStatus == ServerStatus::online) {
			std::string id = get_id();
			if (id == "") {
				std::this_thread::sleep_for(std::chrono::seconds{ 1 });
			}
			else {
				auto response = post_serverapi("", { {"update_time", format_time(update_time)} });
				if (response.status_code != cpr::status::HTTP_OK) {
					log("timer: failed to sync with server");
					std::this_thread::sleep_for(std::chrono::seconds{ 5 });
				}
				else {
					auto data = json::parse(response.text);
					std::chrono::system_clock::time_point new_update_time = parse_time(data["update_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
					bool isNewer = new_update_time > update_time;

					if (isNewer) {
						start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
						update_time = new_update_time;

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
							std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(lastPosition));
						}
					}
				}
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds{ 2 });
		}
	}
}

std::chrono::system_clock::time_point calculate_ticktime(uint64_t boot_ticks) {
	auto ticks_diff = timeGetTime() - boot_ticks;
	return std::chrono::system_clock::now() - std::chrono::milliseconds(ticks_diff);
}

std::string Timer::get_id()
{
	std::string id = "";
	if (isInstanced) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);
		id = map_code;
	}
	else {
		id = group_tracker.get_group_id();
	}
	if (id != last_id) {
		last_id = id;
		using namespace std::chrono_literals;
		update_time = std::chrono::sys_days{ 1970y / 1 / 1 };
	}
	return id;
}

void Timer::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id) {
	if (ev) {
		if (settings.auto_stop) {
			if (src && src->prof > 9) {
				std::scoped_lock<std::mutex> guard(logagents_mutex);
				log_agents.insert(src->prof);
			}
			if (dst && dst->prof > 9) {
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
				auto ticks_now = timeGetTime();
				auto ticks_diff = ticks_now - ev->time;
				log_start_time = std::chrono::system_clock::now() - std::chrono::milliseconds{ ticks_diff };
			}
		}
	}
}

bool checkDelta(float a, float b, float delta) {
	return std::abs(a - b) > delta;
}

void Timer::mod_imgui() {
	if (lastMapID != mumble_link->getMumbleContext()->mapId) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);

		map_code = CRC32()(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		lastMapID = mumble_link->getMumbleContext()->mapId;
		isInstanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;

		bool doAutoPrepare = settings.auto_prepare && (isInstanced || !settings.disable_outside_instances);
		if (doAutoPrepare) {
			log_debug("timer: preparing on map change");
			prepare();
		}
	}

	if (settings.disable_outside_instances && !isInstanced) {
		return;
	}

	if (status == TimerStatus::prepared) {
		if (checkDelta(lastPosition[0], mumble_link->fAvatarPosition[0], 0.1f) ||
			checkDelta(lastPosition[1], mumble_link->fAvatarPosition[1], 0.1f) ||
			checkDelta(lastPosition[2], mumble_link->fAvatarPosition[2], 0.1f)) {
			log_debug("timer: starting on movement");
			start();
		}
	}

	if (status == TimerStatus::running) {
		current_time = std::chrono::system_clock::now();
	}

	if (settings.show_timer) {
		ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		auto duration = std::chrono::round<std::chrono::milliseconds>(current_time - start_time);

		std::string time_string = "";
		try {
			time_string = std::format(settings.time_formatter, duration);
		}
		catch ([[maybe_unused]] const std::exception& e) {
			time_string = "INVALID";
		}

		ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + 3);
		switch (status) {
		case TimerStatus::stopped:
			ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "S");
			break;
		case TimerStatus::prepared:
			ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "P");
			break;
		case TimerStatus::running:
			ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "R");
			break;
		}

		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		if (!settings.hide_buttons) {
			if (ImGui::Button("Prepare", ImVec2(190, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: preparing manually");
				prepare();
			}

			if (ImGui::Button("Start", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: starting manually");
				start();
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Stop", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: stopping manually");
				stop();
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Reset", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: resetting manually");
				reset();
			}
		}
		else {
			ImGui::Dummy(ImVec2(160, 0));
		}

		if (serverStatus == ServerStatus::outofdate) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFFLINE, ADDON OUT OF DATE");
		}

		ImGui::End();
	}

	if (settings.show_segments) {
		ImGui::Begin("Segments", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::BeginTable("##segmenttable", 3);

		ImGui::TableSetupColumn("#");
		ImGui::TableSetupColumn("Last Time (Duration)");
		ImGui::TableSetupColumn("Best Time (Duration)");
		ImGui::TableHeadersRow();

		for (int i = 0; i < segments.size(); ++i) {
			const auto& segment = segments[i];

			if (segment.is_used) {

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
		}

		ImGui::EndTable();

		if (ImGui::Button("Segment")) {
			segment();
		}

		ImGui::SameLine(0, 5);

		if (ImGui::Button("Clear")) {
			clear_segments();
		}

		ImGui::End();
	}
}

void Timer::segment() {
	if (status != TimerStatus::running) return;

	if (segments.size() > 0) {
		for (int i = 0; i < segments.size(); ++i) {
			auto& segment = segments[i];
			if (!segment.is_set) {
				segment.is_set = true;
				segment.end = std::chrono::system_clock::now();

				auto time_total = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
				auto duration_segment = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);

				if (segment.is_used) {
					if (time_total < segment.shortest_time) {
						segment.shortest_time = time_total;
					}
					if (duration_segment < segment.shortest_duration) {
						segment.shortest_duration = duration_segment;
					}
				}
				else {
					segment.shortest_time = time_total;
					segment.shortest_duration = duration_segment;
					segment.is_used = true;
				}

				if (segments.size() == i + 1) {
					segments.emplace_back();
				}
				else {
					segments[i + 1].start = std::chrono::system_clock::now();
				}

				break;
			}
		}
	}
	else {
		TimeSegment start_segment;
		start_segment.start = start_time;
		start_segment.is_set = true;
		start_segment.is_used = true;
		start_segment.shortest_time = std::chrono::round<std::chrono::milliseconds>(start_segment.end - start_time);
		start_segment.shortest_duration = std::chrono::round<std::chrono::milliseconds>(start_segment.end - start_segment.start);
		segments.push_back(start_segment);
		segments.emplace_back();
	}
}

void Timer::clear_segments() {
	segments.clear();
}
