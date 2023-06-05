#include "session.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

Session::Session(boost::asio::ip::tcp::socket socket)
:	socket(std::move(socket)) {
}

void Session::start() {
	group = Group::get_group("default");
	group.value()->join(shared_from_this());

	receive_command();
}

void Session::send_message(std::string message) {
    bool write_in_progress = !message_queue.empty();
    message_queue.push_back(message);
    if (!write_in_progress)
    {
        send_queued_messages();
    }
}

void Session::receive_command() {
	boost::asio::async_read_until(socket, buffer, '\n', 
		[this](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				std::istream is(&buffer);
				std::string command_string;
				std::getline(is, command_string);

				try {
					json command = json::parse(command_string);

					// TODO: check schema

					if (command["command"] == "join") {
						auto old_group = group.value();

						group = Group::get_group(command["group"]);
						group.value()->join(shared_from_this());

						if (old_group != nullptr) {						
							old_group->leave(shared_from_this());
						}
					}
					else if (command["command"] == "state") {
						if (group.has_value()) {
							group.value()->send_message(command["data"]);
						}
					}
					else if (command["command"] == "leave") {
						if (group.has_value()) {
							group.value()->leave(shared_from_this());
						}
						group = std::nullopt;
					}
					else if (command["command"] == "version") {
						json response = {
							{"status", "ok"},
							{"version", 10}
						};
						send_message(response.dump() + '\n');
					}
					else {
						send_message("Invalid command\n");
					
						if (group.has_value()) {
							group.value()->leave(shared_from_this());
						}
					}

				} catch (json::parse_error& e) {
					json response = {
						{"status", "error"},
						{"message", "Invalid JSON"}
					};
					send_message(response.dump() + '\n');
				}

				receive_command();
			}
			else {
				if (group.has_value()) {
					group.value()->leave(shared_from_this());
				}
			}
		}
	);
}

void Session::send_queued_messages() {
    boost::asio::async_write(
        socket,
        boost::asio::buffer(message_queue.front().data(), message_queue.front().length()),
        [this](boost::system::error_code ec, std::size_t) {
			if (!ec) {
				message_queue.pop_front();
				if (!message_queue.empty()) {
					send_queued_messages();
				}
			}
			else {
                if (group.has_value()) {
					group.value()->leave(shared_from_this());
                }
			}
		}
    );
}
