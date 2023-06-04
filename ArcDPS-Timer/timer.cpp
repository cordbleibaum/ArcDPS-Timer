#include "timer.h"
#include "util.h"

Timer::Timer(EventStore & store, Settings& settings, GW2MumbleLink& mumble_link, const Translation& translation, MapTracker& map_tracker)
:	settings(settings),
	mumble_link(mumble_link),
	translation(translation),
	map_tracker(map_tracker),
	store(store)
{
	last_position = { 0 };
} 

void Timer::mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id) {
	TimerState state = store.get_timer_state();

	if (ev) {
		if (ev->is_activation) {
			const std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - state.start_time;
			const double duration = duration_dbl.count();

			store.dispatch_event(EventEntry(calculate_ticktime(ev->time), EventType::start, EventSource::combat));
		}
	}
}

void Timer::mod_imgui() {
	TimerState state = store.get_timer_state();

	if (settings.show_history) {
		if (ImGui::Begin("History", &settings.show_history)) {
			history_window_content();
		}
		ImGui::End();
	}

	if (!settings.is_enabled()) {
		return;
	}

	if (state.status == TimerStatus::prepared) {
		if (checkDelta(last_position[0], mumble_link->fAvatarPosition[0], 0.1f) ||
			checkDelta(last_position[1], mumble_link->fAvatarPosition[1], 0.1f) ||
			checkDelta(last_position[2], mumble_link->fAvatarPosition[2], 0.1f)) {
			log_debug("timer: starting on movement");
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::start, EventSource::movement));
		}
	}

	if (!settings.unified_window) {
		if (settings.show_timer) {
			if (ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
				timer_window_content(190);
			}
			ImGui::End();
		}

		if (settings.show_segments) {
			ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;

			if (!settings.segment_window_border) {
				flags |= ImGuiWindowFlags_NoDecoration;
			}

			if (ImGui::Begin(translation.get("HeaderSegments").c_str(), &settings.show_segments, flags)) {
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

void Timer::map_change(uint32_t map_id) {
	if (settings.should_autoprepare()) {
		defer([&]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			log_debug("timer: preparing on map change");
			std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::prepare, EventSource::other));
		});
	}
}

void Timer::timer_window_content(float width) {
	const TimerState state = store.get_timer_state();

	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	const auto duration = std::chrono::round<std::chrono::milliseconds>(state.current_time - state.start_time);
	std::string time_string = "";
	try {
		time_string = std::vformat(settings.time_formatter, std::make_format_args(duration)); //
	}
	catch ([[maybe_unused]] const std::exception& e) {
		time_string = translation.get("TimeFormatterInvalid");
	}

	ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + 3);
	switch (state.status) {
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

	const auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;
	ImGui::SameLine(0, 0);
	ImGui::SetCursorPosX((width - textWidth) * 0.5f);
	ImGui::Text(time_string.c_str());

	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	if (!settings.hide_timer_buttons) {
		ImGui::PushStyleColor(ImGuiCol_Button, settings.prepare_button_color);
		if (ImGui::Button(translation.get("ButtonPrepare").c_str(), ImVec2(width, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: preparing manually");
			std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::prepare, EventSource::manual));
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.start_button_color);
		if (ImGui::Button(translation.get("ButtonStart").c_str(), ImVec2((width-10)/3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: starting manually");
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::start, EventSource::manual));
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.stop_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("TextStop").c_str(), ImVec2((width - 10) / 3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: stopping manually");
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::stop, EventSource::manual));
		}
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Button, settings.reset_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("TextReset").c_str(), ImVec2((width - 10) / 3, ImGui::GetFontSize() * 1.5f))) {
			log_debug("timer: resetting manually");
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::reset, EventSource::manual));
		}
		ImGui::PopStyleColor();
	}
	else {
		ImGui::Dummy(ImVec2(160, 0));
	}

	/*if (api.server_status == ServerStatus::outofdate) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), translation.get("TextOutOfDate").c_str());
	} TODO: Server status display*/ 
}

void Timer::segment_window_content() {
	TimerState state = store.get_timer_state();
	std::vector<TimeSegment> segments = store.get_segments();

	ImGui::BeginTable("##segmenttable", 4, ImGuiTableFlags_Hideable);
	ImGui::TableSetupColumn(translation.get("HeaderNumColumn").c_str());
	ImGui::TableSetupColumn(translation.get("HeaderNameColumn").c_str(), ImGuiTableColumnFlags_DefaultHide);
	ImGui::TableSetupColumn(translation.get("HeaderLastColumn").c_str());
	ImGui::TableSetupColumn(translation.get("HeaderBestColumn").c_str());
	ImGui::TableHeadersRow();

	for (size_t i = 0; i < segments.size(); ++i) {
		const auto& segment = segments[i];

		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text(std::to_string(i).c_str());

		ImGui::TableNextColumn();
		ImGui::Text(segment.name.c_str());

		ImGui::TableNextColumn();
		if (segment.is_set) {
			const auto time_total = std::chrono::round<std::chrono::milliseconds>(segment.end - state.start_time);
			const auto duration_segment = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
				
			std::string text = "";
			try {
				const std::string total_string = std::vformat(settings.time_formatter, std::make_format_args(time_total));
				const std::string duration_string = std::vformat(settings.time_formatter, std::make_format_args(duration_segment));
				text = total_string + " (" + duration_string + ")";
			}
			catch ([[maybe_unused]] const std::exception& e) {
				text = translation.get("TimeFormatterInvalid");
			}

			ImGui::Text(text.c_str());
		}

		ImGui::TableNextColumn();
		const auto shortest_time = std::chrono::round<std::chrono::milliseconds>(segment.shortest_time);
		const auto shortest_duration = std::chrono::round<std::chrono::milliseconds>(segment.shortest_duration);
			
		std::string text = "";
		try {
			const std::string total_string = std::vformat(settings.time_formatter, std::make_format_args(shortest_time));
			const std::string duration_string = std::vformat(settings.time_formatter, std::make_format_args(shortest_duration));
			text = total_string + " (" + duration_string + ")";
		}
		catch ([[maybe_unused]] const std::exception& e) {
			text = translation.get("TimeFormatterInvalid");
		}

		ImGui::Text(text.c_str());
	}

	ImGui::EndTable();

	if (!settings.hide_segment_buttons) {
		ImGui::PushStyleColor(ImGuiCol_Button, settings.segment_button_color);
		if (ImGui::Button(translation.get("ButtonSegment").c_str())) {
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::segment, EventSource::manual));
		}
		ImGui::PopStyleColor();
		
		ImGui::PushStyleColor(ImGuiCol_Button, settings.clear_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("ButtonClearSegments").c_str())) {
			store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::segment_clear, EventSource::manual));
		}
		ImGui::PopStyleColor();
	}
}

void Timer::history_window_content() {
	const std::vector<HistoryEntry> history = store.get_history();

	if (ImGui::Button(translation.get("ButtonClearHistory").c_str())) {
		store.dispatch_event(EventEntry(std::chrono::system_clock::now(), EventType::history_clear, EventSource::manual));
	}

	ImGui::BeginChild("history_entries");
	for (const auto& entry : history) {
		const auto duration = std::chrono::round<std::chrono::milliseconds>(entry.end - entry.start);
		std::string time_string = "";
		try {
			time_string = std::vformat(settings.time_formatter, std::make_format_args(duration)); //
		}
		catch ([[maybe_unused]] const std::exception& e) {
			time_string = translation.get("TimeFormatterInvalid");
		}

		std::string entry_string = entry.name + " " + time_string;
		ImGui::Text(entry_string.c_str());
	}
	ImGui::EndChild();
}
