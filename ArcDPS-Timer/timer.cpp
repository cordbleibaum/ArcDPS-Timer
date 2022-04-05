#include "timer.h"

#include <cpr/cpr.h>
#include "hash-library/crc32.h"

using namespace std::chrono_literals;

Timer::Timer(Settings& settings, GW2MumbleLink& mumble_link) : 
	settings(settings),
	mumble_link(mumble_link)
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
		log("timer: failed to connect to timer api, enabling offline mode\n");
		settings.is_offline_mode = true;
	}
	else {
		auto data = json::parse(response.text);
		constexpr int server_version = 7;
		if (data["version"] != server_version) {
			log("timer: out of date version, going offline mode\n");
			outOfDate = true;
		}
	}

	std::thread status_sync_thread([&]() {
		sync_thread();
	});
	status_sync_thread.detach();
}

void Timer::post_serverapi(std::string url, const json& payload) {
	cpr::Post(
		cpr::Url{ settings.server_url + url },
		cpr::Body{ payload.dump() },
		cpr::Header{ {"Content-Type", "application/json"} }
	);
}

void Timer::sync_thread() {
	while (true) {
		if (!settings.is_offline_mode) {
			sync();
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds{ 2 });
		}
	}
}

std::string Timer::format_time(std::chrono::system_clock::time_point time) {
	return std::format(
		"{:%FT%T}",
		std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clock_offset * 1000.0)))
	);
}

void Timer::request_start() {
	json request;
	request["time"] = format_time(start_time);
	request["update_time"] = format_time(update_time);
	post_serverapi("groups/" + get_id() + "/start", request);
}


void Timer::start() {
	start_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::running;
	network_thread([&] {
		request_start();
	});

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	if (segments.size() > 0) {
		segments[0].start = start_time;
	}
}

void Timer::start(uint64_t time) {
	status = TimerStatus::running;
	std::chrono::milliseconds time_ms(time);
	start_time = std::chrono::time_point<std::chrono::system_clock>(time_ms);
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	network_thread([&] {
		request_start();
	});

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	if (segments.size() > 0) {
		segments[0].start = start_time;
	}
}

void Timer::request_stop() {
	json request;
	request["time"] = format_time(current_time);
	request["update_time"] = format_time(update_time);
	post_serverapi("groups/" + get_id() + "/stop", request);
}


void Timer::stop() {
	if (status != TimerStatus::stopped) {
		segment();

		status = TimerStatus::stopped;
		current_time = std::chrono::system_clock::now();
		update_time = std::chrono::system_clock::now();
		network_thread([&] {
			request_stop();
		});
	}
}

void Timer::stop(uint64_t time) {
	std::chrono::milliseconds time_ms(time);
	std::chrono::time_point<std::chrono::system_clock> new_stop_time = std::chrono::time_point<std::chrono::system_clock>(time_ms);

	if (status != TimerStatus::stopped || current_time > new_stop_time) {
		segment();

		status = TimerStatus::stopped;
		current_time = new_stop_time;
		update_time = std::chrono::system_clock::now();


		network_thread([&] {
			request_stop();
		});
	}
}

void Timer::reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();

	network_thread([&]() {
		json request;
		request["update_time"] = format_time(update_time);
		post_serverapi("groups/" + get_id() + "/reset", request);
	});

	for (auto& segment : segments) {
		segment.is_set = false;
	}
}

void Timer::prepare() {
	status = TimerStatus::prepared;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	lastPosition[0] = mumble_link->fAvatarPosition[0];
	lastPosition[1] = mumble_link->fAvatarPosition[1];
	lastPosition[2] = mumble_link->fAvatarPosition[2];
	update_time = std::chrono::system_clock::now();

	for (auto& segment : segments) {
		segment.is_set = false;
	}

	network_thread([&]() {
		json request;
		request["update_time"] = format_time(update_time);
		post_serverapi("groups/" + get_id() + "/prepare", request);
	});
}

std::chrono::system_clock::time_point parse_time(const std::string& source) {
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}

void Timer::sync() {
	std::string mapcode_copy = "";

	std::string id = get_id();
	if (id == "") {
		return;
	}

	json request;
	request["update_time"] = format_time(update_time);
	auto response =  cpr::Post(
		cpr::Url{ settings.server_url + "groups/" + id },
		cpr::Body{ request.dump() },
		cpr::Header{ {"Content-Type", "application/json"} }
	);

	if (response.status_code != cpr::status::HTTP_OK) {
		log("timer: failed to sync with server");
		std::this_thread::sleep_for(std::chrono::seconds{ 5 });
		return;
	}

	auto data = json::parse(response.text);

	bool isNewer = true;
	if (data.find("update_time") != data.end()) {
		std::chrono::system_clock::time_point new_update_time = parse_time(data["update_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
		isNewer = new_update_time > update_time;
		update_time = new_update_time;
	}

	if (isNewer && data.find("status") != data.end()) {
		if (data["status"] == "running") {
			log_debug("timer: starting on server");
			status = TimerStatus::running;
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
		}
		else if (data["status"] == "stopped") {
			log_debug("timer: stopping on server");
			status = TimerStatus::stopped;
			current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
		}
		else if (data["status"] == "resetted") {
			log_debug("timer: resetting on server");
			status = TimerStatus::stopped;
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
		}
		else if (data["status"] == "prepared") {
			log_debug("timer: preparing on server");
			status = TimerStatus::prepared;
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
			lastPosition[0] = mumble_link->fAvatarPosition[0];
			lastPosition[1] = mumble_link->fAvatarPosition[1];
			lastPosition[2] = mumble_link->fAvatarPosition[2];
		}
	}
}

unsigned long long calculate_ticktime(uint64_t boot_ticks) {
	auto ticks_now = timeGetTime();
	auto ticks_diff = ticks_now - boot_ticks;
	auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	return now_ms.time_since_epoch().count() - ticks_diff;
}

void Timer::calculate_groupcode() {
	std::string playersConcat = "";

	for (auto it = group_players.begin(); it != group_players.end(); ++it) {
		playersConcat = playersConcat + (*it);
	}

	CRC32 crc32;
	std::string group_code_new = crc32(playersConcat);
	if (group_code != group_code_new) {
		using namespace std::chrono_literals;
		update_time = std::chrono::sys_days{ 1970y / 1 / 1 };
	}
	group_code = group_code_new;
}

std::string Timer::get_id()
{
	if (isInstanced) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);
		return map_code;
	}
	else {
		std::scoped_lock<std::mutex> guard(groupcode_mutex);
		return group_code;
	}
}

void Timer::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision) {
	if (!ev) {
		if (!src->elite) {
			if (src->name != nullptr && src->name[0] != '\0' && dst->name != nullptr && dst->name[0] != '\0') {
				std::string username(dst->name);
				if (username.at(0) == ':') {
					username.erase(0, 1);
				}

				if (src->prof) {
					std::scoped_lock<std::mutex> guard(groupcode_mutex);
					if (selfAccountName.empty() && dst->self) {
						selfAccountName = username;
					}

					group_players.insert(username);
					calculate_groupcode();
				}
				else {
					if (username != selfAccountName) {
						std::scoped_lock<std::mutex> guard(groupcode_mutex);
						group_players.erase(username);
						calculate_groupcode();
					}
				}
			}
		}
	}
	else {
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

		CRC32 crc32;
		map_code = crc32(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		update_time = std::chrono::sys_days{ 1970y / 1 / 1 };
		lastMapID = mumble_link->getMumbleContext()->mapId;
		isInstanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;

		bool doAutoPrepare = settings.auto_prepare;
		doAutoPrepare &= isInstanced | !settings.disable_outside_instances;

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

		if (outOfDate) {
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
					TimeSegment next_segment;
					next_segment.start = std::chrono::system_clock::now();
					segments.push_back(next_segment);
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
		start_segment.end = std::chrono::system_clock::now();
		start_segment.is_set = true;
		start_segment.is_used = true;

		auto time_total = std::chrono::round<std::chrono::milliseconds>(start_segment.end - start_time);
		auto duration_segment = std::chrono::round<std::chrono::milliseconds>(start_segment.end - start_segment.start);
		start_segment.shortest_time = time_total;
		start_segment.shortest_duration = duration_segment;

		segments.push_back(start_segment);

		TimeSegment next_segment;
		next_segment.start = std::chrono::system_clock::now();
		segments.push_back(next_segment);
	}
}

void Timer::clear_segments() {
	segments.clear();
}
