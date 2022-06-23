#include "ntp.h"

#include <vector>
#include <chrono>
#include <array>
#include <cmath>
#include <future>

#include <boost/asio.hpp>

NTPClient::NTPClient(std::string host)
:	host(host) {
}

double NTPClient::get_time_delta() {
	std::vector<NTPInfo> samples;

	for (int i = 0; i < 3; ++i) {
		NTPInfo info = request_time_delta(5);
		if (std::abs(info.offset) > 0) {
			samples.push_back(info);
		}
	}

	NTPInfo best_bias = *(samples.begin());
	
	for (const auto& info : samples) {
		if (info.bias < best_bias.bias) {
			best_bias = info;
		}
	}

	const double offset = best_bias.offset;
	if (offset > 10) {
		throw NTPException("Time sync failed, offset too large to be plausible");
	}

	return offset;
}

NTPInfo NTPClient::request_time_delta(int retries) {
	NTPPacket packetRequest;
	memset(&packetRequest, 0, sizeof(packetRequest));
	packetRequest.li_vn_mode = 0x1b;

	long long t0, t3, t1, t2;
	bool ntp_success = false;
	int retry = 0;
	NTPInfo info;

	constexpr int retry_a = 500;
	constexpr int retry_b = 2;

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
			t1 = (ntohl(packetResponse->rxTm_s) - 2208988800U) * 1000LL + (((double)ntohl(packetResponse->rxTm_f) / std::numeric_limits<uint32_t>::max()) * 1000LL);
			t2 = (ntohl(packetResponse->txTm_s) - 2208988800U) * 1000LL + (((double)ntohl(packetResponse->txTm_f) / std::numeric_limits<uint32_t>::max()) * 1000LL);
		}).wait_for(std::chrono::milliseconds{ (int)(retry_a * std::pow(retry_b, retry)) });
		switch (status)
		{
		case std::future_status::deferred:
			break;
		case std::future_status::ready:
			ntp_success = true;
			break;
		case std::future_status::timeout:
			retry++;
			if (retry > retries) {
				return info;
			}
			break;
		}
	}

	info.offset = ((t1-t0) + (t2-t3))/(2000.0);
	info.bias = ((t3-t2) + (t1-t0))/(2000.0);

	return info;
}

NTPException::NTPException(const std::string& message) 
:	runtime_error(message) {
}
