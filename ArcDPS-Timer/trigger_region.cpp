#include "trigger_region.h"

#include <cmath>

void TriggerRegion::reset()
{
    is_triggered = false;
}

SphereTrigger::SphereTrigger(std::array<float, 3> position, float radius)
 :  position(position),
    radius(radius) {
}

bool SphereTrigger::trigger(std::array<float, 3> player_position)
{
    if (is_triggered) return false;

    float dx = position[0] - player_position[0];
    float dy = position[1] - player_position[1];
    float dz = position[2] - player_position[2];
    float distance = std::sqrtf(dx * dx + dy * dy + dz * dz);
    if (distance < radius) {
        is_triggered = true;
        return true;
    }

    return false;
}

bool BoxTrigger::trigger(std::array<float, 3> player_position)
{
    if (is_triggered) return false;


    return false;
}

TriggerWatcher::TriggerWatcher(GW2MumbleLink& mumble_link)
    : mumble_link(mumble_link)
{
}
