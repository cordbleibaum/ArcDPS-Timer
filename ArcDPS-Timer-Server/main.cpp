#include <iostream>
#include <boost/asio.hpp>

#include "server.h"

void signal_handler(const boost::system::error_code& error, int signal_number) {
    std::cout << "Shutting down because of signal " << signal_number << std::endl;
    exit(1);
}

int main() {
    std::cout << "Server starting" << std::endl;

    try {
        boost::asio::io_context io_context;

        boost::asio::signal_set signals(io_context, SIGINT);
        signals.async_wait(signal_handler);

        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 5000);
        Server server(io_context, endpoint);

        io_context.run();
    }
    catch (std::exception& e) {
		std::cerr << "Unhandled exception: " << e.what() << std::endl;
	}
}
