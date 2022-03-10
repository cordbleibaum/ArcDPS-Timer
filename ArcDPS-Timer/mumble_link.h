#pragma once

#include <stdint.h>
#include <Windows.h>

#include "arcdps-extension/MumbleLink.h"

class GW2MumbleLink {
public:
    GW2MumbleLink();
    ~GW2MumbleLink();
    LinkedMem* operator->();
private:
    LinkedMem* pMumbleLink;
    HANDLE hMumbleLink;
};

enum MapType {
    MAPTYPE_INSTANCE = 4
};
