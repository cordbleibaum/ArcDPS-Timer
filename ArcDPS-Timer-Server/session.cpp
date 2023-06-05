#include "session.h"

Session::Session(boost::asio::ip::tcp::socket socket)
:	socket(std::move(socket)) {
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
                    auto group_ptr = group.value().lock();
                    group_ptr->leave(shared_from_this());
                }
			}
		}
    );
}
