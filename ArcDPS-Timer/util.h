#pragma once

#include <Windows.h>
#include <chrono>
#include <string>

std::chrono::system_clock::time_point calculate_ticktime(uint64_t boot_ticks) {
	auto ticks_diff = timeGetTime() - boot_ticks;
	return std::chrono::system_clock::now() - std::chrono::milliseconds(ticks_diff);
}

bool checkDelta(float a, float b, float delta) {
	return std::abs(a - b) > delta;
}

std::chrono::system_clock::time_point parse_time(const std::string& source) {
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}