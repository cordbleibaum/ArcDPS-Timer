#include "lang.h"

#include <filesystem>
#include <fstream>

#include "json.hpp"
using json = nlohmann::json;

Translation::Translation() {
	const std::string translation_file = "timer_translation.json";
	if (std::filesystem::exists(translation_file)) {
		std::ifstream input(translation_file);
		json translations_in;
		input >> translations_in;

		for (auto& [key, value] : translations_in.items()) {
			translations[key] = value;
		}
	}
}

const std::string& Translation::get(std::string name) {
	return translations[name];
}
