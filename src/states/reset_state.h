#pragma once
#include "state.h"

class ResetState : public State {
public:
    void onEnter() override;
    const char* getName() const override { return "RESET"; }
};
