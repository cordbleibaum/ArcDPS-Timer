#include "group.h"

void Group::join(std::shared_ptr<Receiver> receiver) {
	receivers.insert(receiver);
}

void Group::leave(std::shared_ptr<Receiver> receiver) {
	receivers.erase(receiver);
}

void Group::send_message(std::string message) {
	for (auto receiver : receivers) {
		receiver->send_message(message);
	}
}
