#pragma once
#include "state.h"

class SerialWorkingState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "SERIAL_WORKING"; }
};
