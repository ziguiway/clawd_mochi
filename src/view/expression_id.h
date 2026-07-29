#pragma once

#include <Arduino.h>

enum class ExpressionId : uint8_t {
    NORMAL = 0,
    HAPPY,
    SLEEPY,
    SLEEPING,
    CURIOUS,
    SURPRISED,
    GRUMPY,
    LOVE
};

enum class ExpressionMode : uint8_t {
    MANUAL = 0,
    AUTO
};

constexpr uint8_t EXPRESSION_COUNT = 8;

const char* expressionIdToName(ExpressionId expression);
const char* expressionIdToLabel(ExpressionId expression);
bool expressionIdFromName(const String& name, ExpressionId& expression);
