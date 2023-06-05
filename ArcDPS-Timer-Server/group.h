#pragma once

#include <set>
#include <memory>
#include <string>

#include "receiver.h"

class Group {
public:
	void join(std::shared_ptr<Receiver> receiver);
	void leave(std::shared_ptr<Receiver> receiver);
	void send_message(std::string message);
private:
	std::set<std::shared_ptr<Receiver>> receivers;
};