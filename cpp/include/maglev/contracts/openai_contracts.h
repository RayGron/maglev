#pragma once

#include <optional>
#include <string>
#include <vector>

#include "maglev/contracts/json_types.h"

namespace maglev {

struct ChatMessage {
    std::string role;
    std::string content;
};

struct OpenAiAssistantMessage {
    std::string role;
    std::string content;
    std::optional<std::string> reasoning_content;
};

struct OpenAiChoice {
    OpenAiAssistantMessage message;
    std::optional<std::string> finish_reason;
};

struct OpenAiChatResponse {
    std::vector<OpenAiChoice> choices;
};

void to_json(Json& json, const ChatMessage& value);
void from_json(const Json& json, ChatMessage& value);
void to_json(Json& json, const OpenAiAssistantMessage& value);
void from_json(const Json& json, OpenAiAssistantMessage& value);
void to_json(Json& json, const OpenAiChoice& value);
void from_json(const Json& json, OpenAiChoice& value);
void to_json(Json& json, const OpenAiChatResponse& value);
void from_json(const Json& json, OpenAiChatResponse& value);

}  // namespace maglev
