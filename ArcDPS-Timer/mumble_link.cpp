#include "mumble_link.h"
#include "arcdps.h"

#include <processenv.h>
#include <regex>

GW2MumbleLink::GW2MumbleLink() {
	std::string mumble_link_file = "MumbleLink";

	const std::string commandline = GetCommandLineA();
	const std::regex mumble_regex("-mumble ([[:alnum:]]+)");
	std::smatch cmd_match;
	if (std::regex_search(commandline, cmd_match, mumble_regex)) {
		mumble_link_file = cmd_match[1];
	}

	hMumbleLink = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), mumble_link_file.c_str());
	if (hMumbleLink == NULL) {
		log("timer: could not create mumble link file mapping object\n");
	}
	else {
		pMumbleLink = (LinkedMem*)MapViewOfFile(hMumbleLink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
		if (pMumbleLink == NULL) {
			log("timer: failed to open mumble link file\n");
		}
	}
}

GW2MumbleLink::~GW2MumbleLink() {
	UnmapViewOfFile(pMumbleLink);
	CloseHandle(hMumbleLink);
}

LinkedMem* GW2MumbleLink::operator->() const {
	return pMumbleLink;
}
