#include "expression_id.h"

namespace {
struct ExpressionMetadata {
    const char* name;
    const char* label;
};

constexpr ExpressionMetadata EXPRESSIONS[EXPRESSION_COUNT] = {
    {"normal", "Normal"},
    {"happy", "Happy"},
    {"thinking", "Thinking"},
    {"sleeping", "Sleeping"},
    {"curious", "Curious"},
    {"surprised", "Surprised"},
    {"grumpy", "Grumpy"},
    {"love", "Love"},
};
}

const char* expressionIdToName(ExpressionId expression) {
    const uint8_t index = static_cast<uint8_t>(expression);
    return index < EXPRESSION_COUNT ? EXPRESSIONS[index].name : EXPRESSIONS[0].name;
}

const char* expressionIdToLabel(ExpressionId expression) {
    const uint8_t index = static_cast<uint8_t>(expression);
    return index < EXPRESSION_COUNT ? EXPRESSIONS[index].label : EXPRESSIONS[0].label;
}

bool expressionIdFromName(const String& name, ExpressionId& expression) {
    // 兼容升级前已保存的默认表情配置。
    if (name.equalsIgnoreCase("sleepy")) {
        expression = ExpressionId::THINKING;
        return true;
    }
    for (uint8_t index = 0; index < EXPRESSION_COUNT; index++) {
        if (name.equalsIgnoreCase(EXPRESSIONS[index].name)) {
            expression = static_cast<ExpressionId>(index);
            return true;
        }
    }
    return false;
}
