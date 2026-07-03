#pragma once
#include "state.h"

class LANIdleState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "LAN_IDLE"; }
};
