#pragma once

#include <boost/asio.hpp>

class Server {
public:
	Server(boost::asio::io_context& io_context, boost::asio::ip::tcp::endpoint endpoint);

private:
	boost::asio::ip::tcp::acceptor acceptor;

	void accept_connection();
};