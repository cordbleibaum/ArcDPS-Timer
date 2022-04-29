#pragma once

#include <Windows.h>
#include <chrono>
#include <string>
#include <thread>
#include <type_traits>

std::chrono::system_clock::time_point calculate_ticktime(uint64_t boot_ticks);
bool checkDelta(float a, float b, float delta);
std::chrono::system_clock::time_point parse_time(const std::string& source);

template<class F> void defer(F&& f) {
	std::thread thread(std::forward<F>(f));
	thread.detach();
}
