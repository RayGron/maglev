#include "maglev/contracts/openai_contracts.h"

namespace maglev {

void to_json(Json& json, const ChatMessage& value) {
    json = Json{{"role", value.role}, {"content", value.content}};
}

void from_json(const Json& json, ChatMessage& value) {
    value.role = json.value("role", std::string{});
    value.content = json.value("content", std::string{});
}

void to_json(Json& json, const OpenAiAssistantMessage& value) {
    json = Json{{"role", value.role}, {"content", value.content}, {"reasoning_content", value.reasoning_content}};
}

void from_json(const Json& json, OpenAiAssistantMessage& value) {
    value.role = json.value("role", std::string{});
    value.content = json.value("content", std::string{});
    if (json.contains("reasoning_content") && !json["reasoning_content"].is_null()) {
        value.reasoning_content = json["reasoning_content"].get<std::string>();
    } else if (json.contains("reasoningContent") && !json["reasoningContent"].is_null()) {
        value.reasoning_content = json["reasoningContent"].get<std::string>();
    } else {
        value.reasoning_content.reset();
    }
}

void to_json(Json& json, const OpenAiChoice& value) {
    json = Json{{"message", value.message}, {"finish_reason", value.finish_reason}};
}

void from_json(const Json& json, OpenAiChoice& value) {
    value.message = json.at("message").get<OpenAiAssistantMessage>();
    if (json.contains("finish_reason") && !json["finish_reason"].is_null()) {
        value.finish_reason = json["finish_reason"].get<std::string>();
    } else if (json.contains("finishReason") && !json["finishReason"].is_null()) {
        value.finish_reason = json["finishReason"].get<std::string>();
    } else {
        value.finish_reason.reset();
    }
}

void to_json(Json& json, const OpenAiChatResponse& value) {
    json = Json{{"choices", value.choices}};
}

void from_json(const Json& json, OpenAiChatResponse& value) {
    value.choices = json.value("choices", std::vector<OpenAiChoice>{});
}

}  // namespace maglev
