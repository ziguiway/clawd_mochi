#pragma once
#include "state.h"

class LANWorkingState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "LAN_WORKING"; }
};
