#pragma once

#include <set>
#include <memory>
#include <string>
#include <map>

#include "receiver.h"

class Group {
public:
	void join(std::shared_ptr<Receiver> receiver);
	void leave(std::shared_ptr<Receiver> receiver);
	void send_message(std::string message);

	static std::shared_ptr<Group> get_group(std::string group_name);
private:
	std::string name;
	std::set<std::shared_ptr<Receiver>> receivers;

	static std::map<std::string, std::shared_ptr<Group>> groups;
};