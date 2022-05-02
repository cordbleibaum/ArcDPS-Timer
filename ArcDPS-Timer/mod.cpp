#include "mod.h"

#include <string>
#include <cmath>
#include <chrono>
#include <functional>
#include <boost/signals2/signal.hpp>

#include "arcdps-extension/imgui_stdlib.h"

#include "mumble_link.h"
#include "ntp.h"
#include "settings.h"
#include "timer.h"
#include "grouptracker.h"
#include "trigger_region.h"
#include "lang.h"
#include "maptracker.h"
#include "util.h"
#include "log.h"

Translation translation;
Settings settings("addons/arcdps/timer.json", translation);
GW2MumbleLink mumble_link;
NTPClient ntp("pool.ntp.org");
GroupTracker group_tracker;
MapTracker map_tracker(mumble_link);
TriggerWatcher trigger_watcher(mumble_link);
TriggerEditor trigger_editor(translation, mumble_link, trigger_watcher.regions);
Timer timer(settings, mumble_link, group_tracker, translation, map_tracker, "http://3.72.94.166:5001/");
Logger logger(mumble_link, settings);

std::chrono::system_clock::time_point last_ntp_sync;

boost::signals2::signal<void(void)> mod_windows_signal;
boost::signals2::signal<void(void)> mod_options_signal;

arcdps_exports* mod_init() {
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.8";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;
	arc_exports.wnd_nofilter = mod_wnd;

	timer.clock_offset = 0;
	defer([&]() {
		try {
			timer.clock_offset = ntp.get_time_delta();
			log_debug("timer: clock offset: " + std::to_string(timer.clock_offset));
		}
		catch (NTPException& ex) {
			log("timer: ntp sync failed");
			log(std::string(ex.what()));
		}
	});

	timer.segment_reset_signal.connect(std::bind(&TriggerWatcher::reset, std::ref(trigger_watcher)));
	timer.start_signal.connect(std::bind(&Logger::start, std::ref(logger), std::placeholders::_1));
	timer.stop_signal.connect(std::bind(&Logger::stop, std::ref(logger), std::placeholders::_1));
	timer.reset_signal.connect(std::bind(&Logger::reset, std::ref(logger), std::placeholders::_1));
	timer.segment_signal.connect(std::bind(&Logger::segment, std::ref(logger), std::placeholders::_1, std::placeholders::_2));

	map_tracker.map_change_signal.connect(std::bind(&Timer::map_change, std::ref(timer), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&Logger::map_change, std::ref(logger), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&TriggerWatcher::map_change, std::ref(trigger_watcher), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&TriggerEditor::map_change, std::ref(trigger_editor), std::placeholders::_1));

	mod_windows_signal.connect(std::bind(&Settings::mod_windows, std::ref(settings)));
	mod_windows_signal.connect(std::bind(&TriggerEditor::mod_windows, std::ref(trigger_editor)));
	mod_options_signal.connect(std::bind(&Settings::mod_options, std::ref(settings)));

	log_debug("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	settings.save();
	return 0;
}

uintptr_t mod_options() {
	mod_options_signal();

	return 0;
}

uintptr_t mod_windows(const char* windowname) {
	if (!windowname) {
		mod_windows_signal();
	}
	return 0;
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading) {
	if (!not_charsel_or_loading) return 0;

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_ntp_sync).count() > 128) {
		last_ntp_sync = std::chrono::system_clock::now();
		defer([&]() {
			last_ntp_sync = std::chrono::system_clock::now();
			try {
				timer.clock_offset = ntp.get_time_delta();
				log_debug("timer: clock offset: " + std::to_string(timer.clock_offset));
			}
			catch (NTPException& ex) {
				log("timer: ntp sync failed");
				log(std::string(ex.what()));
			}
		});
	}

	map_tracker.watch();

	if (trigger_watcher.watch()) {
		timer.segment();
	}

	timer.mod_imgui();
	trigger_editor.mod_imgui();

	return 0;
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision) {
	group_tracker.mod_combat(ev, src, dst, skillname, id);
	timer.mod_combat(ev, src, dst, skillname, id);

	return 0;
}

uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (settings.mod_wnd(hWnd, uMsg, wParam, lParam)) {
		return uMsg;
	}

	const int vkey = (int)wParam;

	switch (uMsg) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		if (vkey == settings.start_key.Code) {
			log_debug("timer: starting manually");
			timer.start();
		}
		else if (vkey == settings.stop_key.Code) {
			log_debug("timer: stopping manually");
			timer.stop();
		}
		else if (vkey == settings.reset_key.Code) {
			log_debug("timer: resetting manually");
			timer.reset();
		}
		else if (vkey == settings.prepare_key.Code) {
			log_debug("timer: preparing manually");
			timer.prepare();
		}
		else if (vkey == settings.segment_key.Code) {
			log_debug("timer: segment manually");
			timer.segment();
		}

		break;
	case WM_INPUTLANGCHANGE:
		settings.current_hkl = (HKL) lParam;
		break;
	}
	default:
		break;
	}

	return uMsg;
}
