#pragma once

#include <Arduino.h>

// ============================================================
// 状态机框架
// ============================================================

typedef void (*StateEnterCallback)();
typedef void (*StateUpdateCallback)();
typedef void (*StateExitCallback)();

struct StateHandler {
    StateEnterCallback  onEnter;
    StateUpdateCallback onUpdate;
    StateExitCallback   onExit;
};

class StateMachine {
public:
    StateMachine();

    void addState(int stateId, StateHandler handler);
    void setInitialState(int stateId);
    void transitionTo(int newStateId);
    void update();

    int getCurrentState() const;
    int getPreviousState() const;
    bool justTransitioned() const;
    unsigned long getStateDuration() const;

private:
    int _currentState;
    int _previousState;
    bool _justTransitioned;
    unsigned long _stateEnterMs;

    static const int MAX_STATES = 16;
    StateHandler _handlers[MAX_STATES];
    bool _stateExists[MAX_STATES];
};
