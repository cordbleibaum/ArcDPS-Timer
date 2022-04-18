#include "trigger_region.h"

void TriggerRegion::reset()
{
    is_triggered = false;
}

bool SphereTrigger::trigger(std::array<float, 3> player_position)
{
    if (is_triggered) return false;



    return false;
}
