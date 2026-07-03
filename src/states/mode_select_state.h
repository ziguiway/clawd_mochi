#pragma once
#include "state.h"

class ModeSelectState : public State {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* getName() const override { return "MODE_SELECT"; }

private:
    unsigned long _startMs = 0;
    static constexpr uint32_t DETECT_MS = 3000;
};
