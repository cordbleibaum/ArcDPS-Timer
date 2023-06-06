#include <iostream>
#include <boost/asio.hpp>

#include "server.h"

int main() {
    std::cout << "Server starting" << std::endl;

    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 5000);
        
        Server server(io_context, endpoint);
        io_context.run();
    }
    catch (std::exception& e) {
		std::cerr << "Unhandled exception: " << e.what() << std::endl;
	}
}
