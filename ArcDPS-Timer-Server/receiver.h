#pragma once

class Receiver {
public:
	virtual void send_message(std::string message) = 0;
};