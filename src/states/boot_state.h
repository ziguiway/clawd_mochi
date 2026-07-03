#pragma once
#include "state.h"

class BootState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "BOOT"; }
};
