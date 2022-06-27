#include "timer.h"
#include "util.h"

Timer::Timer(Settings& settings, GW2MumbleLink& mumble_link, const Translation& translation, API& api)
:	settings(settings),
	mumble_link(mumble_link),
	translation(translation),
	api(api)
{
	start_time = current_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;
	last_position = { 0 };
	api.check_serverstatus();

	std::thread status_sync_thread([&]() {
		api.sync(std::bind(&Timer::sync, this, std::placeholders::_1));
	});
	status_sync_thread.detach();
}

std::string Timer::format_time(std::chrono::system_clock::time_point time) {
	return std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clock_offset * 1000.0))));
}

void Timer::start(std::chrono::system_clock::time_point time) {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::running;
	start_time = time;
	current_time = std::chrono::system_clock::now();
	api.post_serverapi("start", {{"time", format_time(start_time)}});
	reset_segments();
	start_signal(start_time);
}

void Timer::stop(std::chrono::system_clock::time_point time) {
	if (status != TimerStatus::stopped || current_time > time) {
		std::unique_lock lock(timerstatus_mutex);

		segment(true);
		status = TimerStatus::stopped;
		current_time = time;
		api.post_serverapi("stop", {{"time", format_time(current_time)}});
		stop_signal(current_time);
	}
}

void Timer::reset() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::stopped;
	start_time = current_time = std::chrono::system_clock::now();
	reset_segments();
	api.post_serverapi("reset");
	reset_signal(current_time);
}

void Timer::prepare() {
	std::unique_lock lock(timerstatus_mutex);

	status = TimerStatus::prepared;
	start_time = current_time = std::chrono::system_clock::now();
	std::copy(std::begin(mumble_link->fAvatarPosition), std::end(mumble_link->fAvatarPosition), std::begin(last_position));
	reset_segments();
	api.post_serverapi("prepare");
	prepare_signal(current_time);
}

void Timer::sync(const nlohmann::json& data) {
	{
		std::unique_lock lock(timerstatus_mutex);
					
		start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
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
		for (json::const_iterator it = data["segments"].begin(); it != data["segments"].end(); ++it) {
			TimeSegment segment;
			segment.is_set = (*it)["is_set"];
			segment.start = parse_time((*it)["start"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
			segment.end = parse_time((*it)["end"]) - std::chrono::milliseconds((int)(clock_offset * 1000.0));
			segment.shortest_time = std::chrono::milliseconds{ (*it)["shortest_time"] };
			segment.shortest_duration = std::chrono::milliseconds{ (*it)["shortest_duration"] };
			segment.name = (*it)["name"];
			segment_signal(i++, segment.start);
			segments.push_back(segment);
		}
	}
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
		if (ev->is_activation) {
			const std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - start_time;
			const double duration = duration_dbl.count();

			if (status == TimerStatus::prepared || (status == TimerStatus::running && duration < 3)) {
				log_debug("timer: starting on skill");
				start(calculate_ticktime(ev->time));
			}
		}
	}
}

void Timer::mod_imgui() {
	if (!settings.is_enabled()) {
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

void Timer::segment(bool local, std::string name) {
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
	segment.name = name;

	const auto time = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
	const auto duration = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
	if (is_new || time < segment.shortest_time) {
		segment.shortest_time = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
	}
	if (is_new || duration < segment.shortest_duration) {
		segment.shortest_duration = std::chrono::round<std::chrono::milliseconds>(segment.end - segment.start);
	}

	if (!local) {
		api.post_serverapi("segment", {
			{"segment_num", segment_num},
			{"time", format_time(segment.end)},
			{"name", name}
		});
	}

	segment_signal(segment_num, current_time);
}

void Timer::clear_segments() {
	std::unique_lock lock(segmentstatus_mutex);

	segments.clear();
	api.post_serverapi("clear_segment");
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

void Timer::bosskill(std::chrono::system_clock::time_point time) {
	if (settings.auto_stop) {
		log_debug("timer: boss kill signal received");
		stop(time);
	}
}

void Timer::timer_window_content(float width) {
	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	{
		std::shared_lock lock(timerstatus_mutex);

		const auto duration = std::chrono::round<std::chrono::milliseconds>(current_time - start_time);
		std::string time_string = "";
		try {
			time_string = std::vformat(settings.time_formatter, std::make_format_args(duration)); //
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

		const auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX((width - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());
	}

	ImGui::Dummy(ImVec2(0.0f, 3.0f));

	if (!settings.hide_timer_buttons) {
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

	if (api.server_status == ServerStatus::outofdate) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), translation.get("TextOutOfDate").c_str());
	}
}

void Timer::segment_window_content() {
	{
		std::shared_lock lock(segmentstatus_mutex);

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
				const auto time_total = std::chrono::round<std::chrono::milliseconds>(segment.end - start_time);
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
	}

	if (!settings.hide_segment_buttons) {
		ImGui::PushStyleColor(ImGuiCol_Button, settings.segment_button_color);
		if (ImGui::Button(translation.get("ButtonSegment").c_str())) {
			segment();
		}
		ImGui::PopStyleColor();
		
		ImGui::PushStyleColor(ImGuiCol_Button, settings.clear_button_color);
		ImGui::SameLine(0, 5);
		if (ImGui::Button(translation.get("ButtonClearSegments").c_str())) {
			clear_segments();
		}
		ImGui::PopStyleColor();
	}
}
