#include "ntp.h"

#include <vector>
#include <chrono>
#include <array>
#include <cmath>
#include <future>

#include <boost/asio.hpp>

NTPClient::NTPClient(std::string host) {
	this->host = host;
}

double NTPClient::request_time_delta() {
	NTPPacket packetRequest;
	memset(&packetRequest, 0, sizeof(packetRequest));
	packetRequest.li_vn_mode = 0x1b;

	unsigned long t0, t3, t1, t2;
	bool ntp_success = false;
	int retry = 0;

	while (!ntp_success) {
		auto status = std::async(std::launch::async, [&]() {
			boost::asio::io_service io_service;
			boost::asio::ip::udp::resolver resolver(io_service);
			boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), this->host, "ntp");

			boost::asio::ip::udp::endpoint receiver_endpoint = *resolver.resolve(query);
			boost::asio::ip::udp::socket socket(io_service);
			socket.open(boost::asio::ip::udp::v4());

			t0 = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
			std::vector<NTPPacket> sendBuffer;
			sendBuffer.push_back(packetRequest);
			socket.send_to(boost::asio::buffer(sendBuffer), receiver_endpoint);

			std::array<unsigned long, 1024> recvBuffer;
			boost::asio::ip::udp::endpoint sender_endpoint;
			size_t len = socket.receive_from(boost::asio::buffer(recvBuffer), sender_endpoint);
			t3 = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);

			NTPPacket* packetResponse = (NTPPacket*)recvBuffer.data();
			t1 = (ntohl(packetResponse->rxTm_s) - 2208988800U) * 1000UL + (((double)ntohl(packetResponse->rxTm_f) / std::numeric_limits<uint32_t>::max()) * 1000UL);
			t2 = (ntohl(packetResponse->txTm_s) - 2208988800U) * 1000UL + (((double)ntohl(packetResponse->txTm_f) / std::numeric_limits<uint32_t>::max()) * 1000UL);
		}).wait_for(std::chrono::milliseconds{ 500 });
		switch (status)
		{
		case std::future_status::deferred:
			break;
		case std::future_status::ready:
			ntp_success = true;
			break;
		case std::future_status::timeout:
			retry++;
			if (retry > 10) {
				return 0.0;
			}
			break;
		}
	}
	double offset = ((t1-t0)+(t2-t3))/(2000.0);

	return offset;
}
