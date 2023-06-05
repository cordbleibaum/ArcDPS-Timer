#include "server.h"
#include <iostream>

Server::Server(boost::asio::io_context& io_context, boost::asio::ip::tcp::endpoint endpoint)
:	acceptor(io_context, endpoint) {
    std::cout << "Server now accepting connections" << std::endl;
	accept_connection();
}

void Server::accept_connection() {
	acceptor.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<Session>(std::move(socket));
                session->start();
            }

            accept_connection();
        }
    );
}
