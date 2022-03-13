#include "timer.h"

#include <string>
#include <chrono>
#include <cmath>
#include <format>
#include <set>

#include <cpr/cpr.h>

#include "imgui_stdlib.h"
#include "json.hpp"
#include "mumble_link.h"
#include "hash-library/crc32.h"

#include "ntp.h"
#include "settings.h"

using json = nlohmann::json;
using namespace std::chrono_literals;

enum class TimerStatus { stopped, prepared, running };

std::string config_file = "addons/arcdps/timer.json";
constexpr int server_version = 6;
constexpr int settings_version = 8;

TimerStatus status;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;
std::chrono::system_clock::time_point update_time;
std::chrono::system_clock::time_point last_update;
std::chrono::system_clock::time_point last_ntp_sync;
std::chrono::system_clock::time_point log_start_time;

Settings settings(config_file);
GW2MumbleLink mumble_link;
NTPClient ntp("pool.ntp.org");

float lastPosition[3];
uint32_t lastMapID = 0;
std::set<uintptr_t> log_agents;
unsigned long long last_damage_ticks;

std::string map_code;
std::mutex mapcode_mutex;
std::mutex logagents_mutex;

bool outOfDate = false;
bool isInstanced = false;

double clockOffset = 0;

template<class F> void network_thread(F&& f)
{
	if (!settings.is_offline_mode && !outOfDate) {
		std::thread thread(std::forward<F>(f));
		thread.detach();
	}
}

arcdps_exports* mod_init() {
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.6";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;
	arc_exports.wnd_nofilter = mod_wnd;

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

	auto response = cpr::Get(
		cpr::Url{ settings.server_url + "version" },
		cpr::Timeout{ 1000 }
	);
	if (response.status_code != cpr::status::HTTP_OK) {
		log("timer: failed to connect to timer api, enabling offline mode\n");
		settings.is_offline_mode = true;
	}
	else {
		auto data = json::parse(response.text);
		if (data["major"] != server_version) {
			log("timer: out of date version, going offline mode\n");
			outOfDate = true;
		}
	}

	clockOffset = 0;
	network_thread([&]() {
		clockOffset = ntp.get_time_delta();
		log_debug("timer: clock offset: " + std::to_string(clockOffset));
	});

	log_debug("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	settings.save();
	return 0;
}

uintptr_t mod_options() {
	settings.show_options();
	return 0;
}

uintptr_t mod_windows(const char* windowname) {
	if (!windowname) {
		settings.show_windows();
	}
	return 0;
}

bool checkDelta(float a, float b, float delta) {
	return std::abs(a - b) > delta;
}

void update_lastposition() {
	lastPosition[0] = mumble_link->fAvatarPosition[0];
	lastPosition[1] = mumble_link->fAvatarPosition[1];
	lastPosition[2] = mumble_link->fAvatarPosition[2];
}

void post_serverapi(std::string url, const json& payload) {
	cpr::Post(
		cpr::Url{ settings.server_url + url },
		cpr::Body{ payload.dump() },
		cpr::Header{ {"Content-Type", "application/json"} }
	);
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading) {
	if (!not_charsel_or_loading) return 0;

	if (lastMapID != mumble_link->getMumbleContext()->mapId) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);

		lastMapID = mumble_link->getMumbleContext()->mapId;
		isInstanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;

		CRC32 crc32;
		map_code = crc32(mumble_link->getMumbleContext()->serverAddress, sizeof(sockaddr_in));
		update_time = std::chrono::sys_days{ 1970y / 1 / 1 };

		bool doAutoPrepare = settings.auto_prepare;
		doAutoPrepare &= isInstanced | !settings.disable_outside_instances;

		if (doAutoPrepare) {
			log_debug("timer: preparing on map change");
			timer_prepare();
		}
	}

	if (settings.disable_outside_instances && !isInstanced) {
		return 0;
	}

	if (status == TimerStatus::prepared) {
		if (checkDelta(lastPosition[0], mumble_link->fAvatarPosition[0], 0.1f) ||
			checkDelta(lastPosition[1], mumble_link->fAvatarPosition[1], 0.1f) ||
			checkDelta(lastPosition[2], mumble_link->fAvatarPosition[2], 0.1f)) {
			log_debug("timer: starting on movement");
			timer_start();
		}
	}

	if (status == TimerStatus::running) {
		current_time = std::chrono::system_clock::now();
	}

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_update).count() > settings.sync_interval) {
		last_update = std::chrono::system_clock::now();
		network_thread(sync_timer);
	}

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_ntp_sync).count() > 128) {
		last_ntp_sync = std::chrono::system_clock::now();
		network_thread([&]() {
			clockOffset = ntp.get_time_delta();
			last_ntp_sync = std::chrono::system_clock::now();
			log_debug("timer: clock offset: " + std::to_string(clockOffset));
		});
	}

	if (settings.show_timer) {
		ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		
		auto duration = std::chrono::round<std::chrono::milliseconds>(current_time - start_time);

		std::string time_string = "";
		try {
			time_string = std::format(settings.time_formatter, duration);
		}
		catch([[maybe_unused]] const std::exception& e) {
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
				timer_prepare();
			}

			if (ImGui::Button("Start", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: starting manually");
				timer_start();
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Stop", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: stopping manually");
				timer_stop();
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Reset", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: resetting manually");
				timer_reset();
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

	return 0;
}

std::string format_time(std::chrono::system_clock::time_point time) {
	return std::format(
		"{:%FT%T}",
		std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clockOffset * 1000.0)))
	);
}

void request_start() {
	json request;
	request["time"] = format_time(start_time);
	request["update_time"] = format_time(update_time);
	post_serverapi("groups/" + map_code + "/start", request);
}

void timer_start() {
	start_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::running;
	network_thread(request_start);
}

void timer_start(uint64_t time) {
	status = TimerStatus::running;
	std::chrono::milliseconds time_ms(time);
	start_time = std::chrono::time_point<std::chrono::system_clock>(time_ms);
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	network_thread(request_start);
}

void request_stop() {
	json request;
	request["time"] = format_time(current_time);
	request["update_time"] = format_time(update_time);
	post_serverapi("groups/" + map_code + "/stop", request);
}

void timer_stop() {
	if (status != TimerStatus::stopped) {
		status = TimerStatus::stopped;
		current_time = std::chrono::system_clock::now();
		update_time = std::chrono::system_clock::now();
		network_thread(request_stop);
	}
}

void timer_stop(uint64_t time) {
	std::chrono::milliseconds time_ms(time);
	std::chrono::time_point<std::chrono::system_clock> new_stop_time = std::chrono::time_point<std::chrono::system_clock>(time_ms);

	if (status != TimerStatus::stopped || current_time > new_stop_time) {
		status = TimerStatus::stopped;
		current_time = new_stop_time;
		update_time = std::chrono::system_clock::now();
		network_thread(request_stop);
	}
}

void timer_prepare() {
	status = TimerStatus::prepared;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_lastposition();
	update_time = std::chrono::system_clock::now();

	network_thread([&]() {
		json request;
		request["update_time"] = format_time(update_time);
		post_serverapi("groups/" + map_code + "/prepare", request);
	});
}

unsigned long long calculate_ticktime(uint64_t boot_ticks) {
	auto ticks_now = timeGetTime();
	auto ticks_diff = ticks_now - boot_ticks;
	auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	return now_ms.time_since_epoch().count() - ticks_diff;
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision) {
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
				timer_start(calculate_ticktime(ev->time));
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
						timer_stop(calculate_ticktime(ev->time));
					}
					else if (is_ooc_end) {
						log_debug("timer: stopping on ooc after boss");
						timer_stop(last_damage_ticks);
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

	return 0;
}

void timer_reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();

	network_thread([&]() {
		json request;
		request["update_time"] = format_time(update_time);
		post_serverapi("groups/" + map_code + "/reset", request);
	});
}

std::chrono::system_clock::time_point parse_time(const std::string& source) {
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}

void sync_timer() {
	std::string mapcode_copy = "";

	{
		std::scoped_lock<std::mutex> guard(mapcode_mutex);
		if (map_code == "") {
			return;
		}
		mapcode_copy = map_code;
	}

	auto response = cpr::Get(cpr::Url{ settings.server_url + "groups/" + mapcode_copy });

	if (response.status_code != cpr::status::HTTP_OK) {
		log("timer: failed to sync with server");
		return;
	}

	auto data = json::parse(response.text);

	bool isNewer = true;
	if (data.find("update_time") != data.end()) {
		std::chrono::system_clock::time_point new_update_time = parse_time(data["update_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
		isNewer = new_update_time > update_time;
	}

	if (isNewer && data.find("status") != data.end()) {
		if (data["status"] == "running") {
			log_debug("timer: starting on server");
			status = TimerStatus::running;
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
		}
		else if (data["status"] == "stopped") {
			log_debug("timer: stopping on server");
			status = TimerStatus::stopped;
			current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
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
			update_lastposition();
		}
	}
}

uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		const int vkey = (int)wParam;

		if (vkey == settings.start_key) {
			log_debug("timer: starting manually");
			timer_start();
		}
		else if (vkey == settings.stop_key) {
			log_debug("timer: stopping manually");
			timer_stop();
		}
		else if (vkey == settings.reset_key) {
			log_debug("timer: resetting manually");
			timer_reset();
		}
		else if (vkey == settings.prepare_key) {
			log_debug("timer: preparing manually");
			timer_prepare();
		}

		break;
	}
	default:
		break;
	}

	return uMsg;
}
