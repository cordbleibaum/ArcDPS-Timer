#pragma once

#include <deque>
#include <string>
#include <memory>
#include <optional>
#include <boost/asio.hpp>

#include "receiver.h"
#include "group.h"

class Session : public Receiver, public std::enable_shared_from_this<Session> {
public:
	Session(boost::asio::ip::tcp::socket socket);
	void send_message(std::string message);
private:
	boost::asio::ip::tcp::socket socket;
	std::deque<std::string> message_queue;
	std::optional<std::weak_ptr<Group>> group;

	void receive_command();
	void send_queued_messages();
};