#include "mod.h"

#include <string>
#include <cmath>
#include <chrono>
#include <functional>

#include <boost/signals2/signal.hpp>
#include <boost/uuid/uuid.hpp>

#include "arcdps-extension/imgui_stdlib.h"
#include "arcdps-extension/KeyBindHandler.h"
#include "arcdps-extension/Singleton.h"

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
#include "api.h"
#include "bosskill_recognition.h"
#include "eventstore.h"

Translation translation;
KeyBindHandler keybind_handler;
GW2MumbleLink mumble_link;
NTPClient ntp("pool.ntp.org");
GroupTracker group_tracker;
MapTracker map_tracker(mumble_link);
Settings settings("addons/arcdps/timer.json", translation, keybind_handler, map_tracker, mumble_link);
TriggerWatcher trigger_watcher(mumble_link);
TriggerEditor trigger_editor(translation, mumble_link, trigger_watcher.regions);
API api(settings, mumble_link, map_tracker, group_tracker, "http://18.192.87.148:5001/");
EventStore store(api);
Timer timer(store, settings, mumble_link, translation, api, map_tracker);
Logger logger(mumble_link, settings, map_tracker);
BossKillRecognition bosskill(mumble_link, settings);

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

	map_tracker.map_change_signal.connect(std::bind(&Timer::map_change, std::ref(timer), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&Logger::map_change, std::ref(logger), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&TriggerWatcher::map_change, std::ref(trigger_watcher), std::placeholders::_1));
	map_tracker.map_change_signal.connect(std::bind(&TriggerEditor::map_change, std::ref(trigger_editor), std::placeholders::_1));

	map_tracker.map_change_signal.connect([&](uint32_t map_id) {
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::map_change, EventSource::other, map_tracker.get_map_name()));
	});

	mod_windows_signal.connect(std::bind(&Settings::mod_windows, std::ref(settings)));
	mod_windows_signal.connect(std::bind(&TriggerEditor::mod_windows, std::ref(trigger_editor)));
	mod_options_signal.connect(std::bind(&Settings::mod_options, std::ref(settings)));

	trigger_watcher.trigger_signal.connect([&](std::string name, boost::uuids::uuid uuid) {
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::segment, EventSource::combat, uuid));
	});

	bosskill.bosskill_signal.connect([&](const auto& time) {
		if (settings.should_autostop()) {
			log_debug("timer: boss kill signal received");
			store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::stop, EventSource::combat));
		}
	});

	KeyBindHandler::Subscriber start_subscriber;
	start_subscriber.Fun = [&](const KeyBinds::Key&) {
		log_debug("timer: starting manually");
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::start, EventSource::manual));
		return true;
	};
	start_subscriber.Key = settings.start_key;
	start_subscriber.Flags = KeyBindHandler::SubscriberFlags_None;
	settings.start_key_handler = keybind_handler.Subscribe(start_subscriber);

	KeyBindHandler::Subscriber stop_subscriber;
	stop_subscriber.Fun = [&](const KeyBinds::Key&) {
		log_debug("timer: stopping manually");
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::stop, EventSource::manual));
		return true;
	};
	stop_subscriber.Key = settings.stop_key;
	stop_subscriber.Flags = KeyBindHandler::SubscriberFlags_None;
	settings.stop_key_handler = keybind_handler.Subscribe(stop_subscriber);

	KeyBindHandler::Subscriber reset_subscriber;
	reset_subscriber.Fun = [&](const KeyBinds::Key&) {
		log_debug("timer: resetting manually");
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::reset, EventSource::manual));
		return true;
	};
	reset_subscriber.Key = settings.reset_key;
	reset_subscriber.Flags = KeyBindHandler::SubscriberFlags_None;
	settings.reset_key_handler = keybind_handler.Subscribe(reset_subscriber);

	KeyBindHandler::Subscriber prepare_subscriber;
	prepare_subscriber.Fun = [&](const KeyBinds::Key&) {
		log_debug("timer: preparing manually");
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::prepare, EventSource::manual));
		return true;
	};
	prepare_subscriber.Key = settings.prepare_key;
	prepare_subscriber.Flags = KeyBindHandler::SubscriberFlags_None;
	settings.prepare_key_handler = keybind_handler.Subscribe(prepare_subscriber);

	KeyBindHandler::Subscriber segment_subscriber;
	segment_subscriber.Fun = [&](const KeyBinds::Key&) {
		log_debug("timer: segment manually");
		store.dispatch_event(EventLogEntry(std::chrono::system_clock::now(), EventType::segment, EventSource::manual));
		return true;
	};
	segment_subscriber.Key = settings.segment_key;
	segment_subscriber.Flags = KeyBindHandler::SubscriberFlags_None;
	settings.segment_key_handler = keybind_handler.Subscribe(segment_subscriber);

	log_debug("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	g_singletonManagerInstance.Shutdown();
	settings.mod_release();
	logger.mod_release();
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
				store.clock_offset = ntp.get_time_delta();
				log_debug("timer: clock offset: " + std::to_string(timer.clock_offset));
			}
			catch (NTPException& ex) {
				log("timer: ntp sync failed");
				log(std::string(ex.what()));
			}
		});
	}

	map_tracker.watch();
	trigger_watcher.watch();

	timer.mod_imgui();
	trigger_editor.mod_imgui();

	return 0;
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision) {
	group_tracker.mod_combat(ev, src, dst, skillname, id);
	bosskill.mod_combat(ev, src, dst, skillname, id);
	timer.mod_combat(ev, src, dst, skillname, id);

	return 0;
}

uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (settings.mod_wnd(hWnd, uMsg, wParam, lParam)) {
		return uMsg;
	}

	if (keybind_handler.Wnd(hWnd, uMsg, wParam, lParam)) {
		return uMsg;
	}

	switch (uMsg) {
	case WM_INPUTLANGCHANGE:
		settings.current_hkl = (HKL) lParam;
		break;
	default:
		break;
	}

	return uMsg;
}
