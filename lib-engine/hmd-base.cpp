#include "hmd-base.hpp"

#include "openvr/include/openvr.h"

using namespace polymer;

void update_button_state(vr_button_state & state, const bool value)
{
    state.prev_down = state.down;
    state.down = value;
    state.pressed = !state.prev_down && value;
    state.released = state.prev_down && !value;
}

vr_button get_button_id_for_vendor(const uint32_t which_button, const vr_input_vendor vendor)
{
    switch(vendor)
    {
        case vr_input_vendor::vive_wand:
        {
            if      (which_button == vr::k_EButton_System)           return { vr_button::system };
            else if (which_button == vr::k_EButton_ApplicationMenu)  return { vr_button::menu };
            else if (which_button == vr::k_EButton_Grip)             return { vr_button::grip };
            else if (which_button == vr::k_EButton_SteamVR_Touchpad) return { vr_button::xy };
            else if (which_button == vr::k_EButton_SteamVR_Trigger)  return { vr_button::trigger };
            break;
        }
    }
    return {};
}
