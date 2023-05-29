#pragma once

#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

namespace nlohmann {
	template <>
	struct adl_serializer<boost::uuids::uuid> {
		static void to_json(json& j, const boost::uuids::uuid& data) {
			j = boost::uuids::to_string(data);
		}

		static void from_json(const json& j, boost::uuids::uuid& data) {
			if (j.is_null()) {
				data = boost::uuids::nil_uuid();
			}
			else {
				std::string uuid_string;
				j.get_to(uuid_string);
				data = boost::lexical_cast<boost::uuids::uuid>(uuid_string);
			}
		}
	};
};