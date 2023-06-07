#pragma once

class Receiver {
public:
	virtual ~Receiver() = default;
	virtual void send_message(std::string message) = 0;
};