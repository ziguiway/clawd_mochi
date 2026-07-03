#pragma once
#include "state.h"

class SerialIdleState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "SERIAL_IDLE"; }
};
