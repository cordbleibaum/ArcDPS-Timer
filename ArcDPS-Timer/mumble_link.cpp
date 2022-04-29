#include "mumble_link.h"
#include "arcdps.h"

GW2MumbleLink::GW2MumbleLink() {
	hMumbleLink = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
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
