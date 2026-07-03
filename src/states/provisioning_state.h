#pragma once
#include "state.h"

class ProvisioningState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* getName() const override { return "PROVISIONING"; }
};
