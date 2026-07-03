#pragma once

#include <Arduino.h>
#include "app_context.h"

class State {
public:
    virtual ~State() = default;

    virtual void onEnter() {}
    virtual void onUpdate() {}
    virtual void onExit() {}

    virtual const char* getName() const = 0;

    virtual void requestNextState() {}

protected:
    IAppContext* _ctx = nullptr;
    friend class AppStateMachine;
};
