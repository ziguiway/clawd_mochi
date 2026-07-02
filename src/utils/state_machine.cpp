#include "state_machine.h"

StateMachine::StateMachine()
    : _currentState(-1)
    , _previousState(-1)
    , _justTransitioned(false)
    , _stateEnterMs(0)
{
    for (int i = 0; i < MAX_STATES; i++) {
        _stateExists[i] = false;
        _handlers[i] = {nullptr, nullptr, nullptr};
    }
}

void StateMachine::addState(int stateId, StateHandler handler) {
    if (stateId >= 0 && stateId < MAX_STATES) {
        _handlers[stateId] = handler;
        _stateExists[stateId] = true;
    }
}

void StateMachine::setInitialState(int stateId) {
    if (stateId >= 0 && stateId < MAX_STATES && _stateExists[stateId]) {
        _currentState = stateId;
        _stateEnterMs = millis();
        _justTransitioned = true;
        if (_handlers[stateId].onEnter) {
            _handlers[stateId].onEnter();
        }
    }
}

void StateMachine::transitionTo(int newStateId) {
    if (newStateId == _currentState) return;
    if (newStateId < 0 || newStateId >= MAX_STATES) return;
    if (!_stateExists[newStateId]) return;

    if (_currentState >= 0 && _handlers[_currentState].onExit) {
        _handlers[_currentState].onExit();
    }

    _previousState = _currentState;
    _currentState = newStateId;
    _stateEnterMs = millis();
    _justTransitioned = true;

    if (_handlers[_currentState].onEnter) {
        _handlers[_currentState].onEnter();
    }
}

void StateMachine::update() {
    if (_currentState < 0) return;
    _justTransitioned = false;
    if (_handlers[_currentState].onUpdate) {
        _handlers[_currentState].onUpdate();
    }
}

int StateMachine::getCurrentState() const {
    return _currentState;
}

int StateMachine::getPreviousState() const {
    return _previousState;
}

bool StateMachine::justTransitioned() const {
    return _justTransitioned;
}

unsigned long StateMachine::getStateDuration() const {
    return millis() - _stateEnterMs;
}
