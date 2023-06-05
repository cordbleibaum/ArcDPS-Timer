#include "group.h"

std::map<std::string, std::shared_ptr<Group>> Group::groups;

void Group::join(std::shared_ptr<Receiver> receiver) {
	receivers.insert(receiver);
}

void Group::leave(std::shared_ptr<Receiver> receiver) {
	receivers.erase(receiver);

	if (receivers.empty()) {
		groups.erase(name);
	}
}

void Group::send_message(std::string message) {
	for (auto receiver : receivers) {
		receiver->send_message(message);
	}
}

std::shared_ptr<Group> Group::get_group(std::string name) {
	if (groups.find(name) == groups.end()) {
		groups[name] = std::make_shared<Group>();
	}
	return groups[name];
}
