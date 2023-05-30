#pragma once

#include <nlohmann/json.hpp>

#include "unofficial_extras/KeyBindStructs.h"
#include "arcdps-extension/KeyBindHandler.h"

namespace nlohmann {
	template <>
	struct adl_serializer<KeyBinds::Key> {
		static void to_json(json& j, const KeyBinds::Key& key) {
			j["code"] = key.Code;
			j["device_type"] = key.DeviceType;
			j["modifier"] = key.Modifier;
		}

		static void from_json(const json& j, KeyBinds::Key& key) {
			if (j.is_null()) {
				key = KeyBinds::Key();
			}
			else {
				j.at("code").get_to(key.Code);
				j.at("device_type").get_to(key.DeviceType);
				j.at("modifier").get_to(key.Modifier);
			}
		}
	};
};
