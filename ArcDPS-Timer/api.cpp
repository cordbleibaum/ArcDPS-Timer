  #include "api.h"

#include <thread>

#include "arcdps.h"

using json = nlohmann::json;

API::API(const Settings& settings, GW2MumbleLink& mumble_link, MapTracker& map_tracker, GroupTracker& group_tracker, std::string server_url)
:   server_url(server_url),
	settings(settings),
	mumble_link(mumble_link),
	map_tracker(map_tracker),
	group_tracker(group_tracker),
	id("default") {
}

void API::post_serverapi(std::string method, nlohmann::json payload) {
	if (server_status != ServerStatus::online || socket.get() == nullptr) {
		return;
	}

	std::thread thread([&, method, payload]() {
		boost::asio::streambuf request_buffer;
		std::ostream request_stream(&request_buffer);

		json request = {
			{"command", "state"},
			{"data", payload}
		};

		request_stream << request.dump() << '\n';
		boost::asio::write(*socket, request_buffer);
	});
	thread.detach();
}

void API::start_sync(std::function<void(const nlohmann::json&)> data_function) {
	try {
		log_debug("Timer: connecting to server");
		boost::asio::ip::tcp::resolver resolver(io_context);
		boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), server_url, "5000");
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
		socket = std::make_unique<boost::asio::ip::tcp::socket>(io_context);
		socket->connect(endpoint);
		log_debug("Timer: connected to server");

		boost::asio::streambuf request_buffer;
		std::ostream request_stream(&request_buffer);
		json request = {
			{"command", "version"}
		};
		request_stream << request.dump() << '\n';
		boost::asio::write(*socket, request_buffer);

		boost::asio::read_until(*socket, receive_buffer, '\n');
		std::istream response_stream(&receive_buffer);
		std::string response_string;
		std::getline(response_stream, response_string);

		try {
			json response = json::parse(response_string);
			if (response["version"] != 10) {
				server_status = ServerStatus::outofdate;
			}
			else {
				server_status = ServerStatus::online;
			}
		}
		catch ([[maybe_unused]] json::parse_error& e) {
			server_status = ServerStatus::offline;
			log("Timer: error getting server status");
		}
	}
	catch ([[maybe_unused]] boost::system::system_error& e) {
		server_status = ServerStatus::offline;
		log("Timer: could not connect to server");
	}

	sync(data_function);
}

std::string API::get_id() const {
	if (settings.use_custom_id) {
		return settings.custom_id + "_custom";
	}
	if (mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE) {
		return map_tracker.get_instance_id();
	}
	return group_tracker.get_group_id();
}

void API::mod_imgui() {
	if (server_status != ServerStatus::online || socket.get() == nullptr) {
		return;
	}

	try {
		std::string new_id = get_id();
		if (new_id == "") {
			new_id = "default";
		}

		if (id != new_id) {
			id = new_id;

			boost::asio::streambuf request_buffer;
			std::ostream request_stream(&request_buffer);
			json request = {
				{"command", "join"},
				{"group", id}
			};
			request_stream << request.dump() << '\n';
			boost::asio::write(*socket, request_buffer);
		}
	}
	catch ([[maybe_unused]] boost::system::system_error& e) {
		server_status = ServerStatus::offline;
		log("Timer: failed to update group");
	}
}

API::~API() {
	boost::asio::post(io_context, [this]() { 
		socket->close(); 
	});
}

void API::sync(std::function<void(const nlohmann::json&)> data_function) {
	if (server_status != ServerStatus::online || socket.get() == nullptr) {
		return;
	}

	boost::asio::async_read_until(*socket, receive_buffer, '\n',
		[this, data_function](boost::system::error_code ec, std::size_t length) {
			if (ec) {
				server_status = ServerStatus::offline;
				return;
			}

			std::istream response_stream(&receive_buffer);
			std::string response_string;
			std::getline(response_stream, response_string);
			try {
				json response = json::parse(response_string);
				if (response["status"] == "state") {
					data_function(response["data"]);
				}
				else if (response["status"] == "error") {
					server_status = ServerStatus::offline;
				}
			} catch ([[maybe_unused]] json::parse_error& e) {
				server_status = ServerStatus::offline;
				log("Timer: error reading server response");
			}
			sync(data_function);
		}
	);
}
