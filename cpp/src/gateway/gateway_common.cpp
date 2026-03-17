#include "maglev/gateway/gateway_internal.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "maglev/runtime/util.h"

namespace maglev::gateway_detail {

namespace {

void append_non_empty_candidate(std::vector<std::string>& candidates, const std::string& value) {
    const auto normalized = trim(value);
    if (!normalized.empty()) {
        candidates.push_back(normalized);
    }
}

void append_json_content_candidates(std::vector<std::string>& candidates, const Json& value) {
    if (value.is_string()) {
        append_non_empty_candidate(candidates, value.get<std::string>());
        return;
    }

    if (!value.is_array()) {
        return;
    }

    std::vector<std::string> parts;
    for (const auto& item : value) {
        if (item.is_string()) {
            parts.push_back(item.get<std::string>());
            continue;
        }
        if (item.is_object()) {
            if (item.contains("text") && item["text"].is_string()) {
                parts.push_back(item["text"].get<std::string>());
                continue;
            }
            if (item.contains("content") && item["content"].is_string()) {
                parts.push_back(item["content"].get<std::string>());
            }
        }
    }

    append_non_empty_candidate(candidates, join_strings(parts, "\n"));
}

std::string extract_braced_json(const std::string& content) {
    const auto first_object = content.find('{');
    const auto last_object = content.rfind('}');
    if (first_object != std::string::npos && last_object != std::string::npos && last_object >= first_object) {
        return content.substr(first_object, last_object - first_object + 1);
    }

    const auto first_array = content.find('[');
    const auto last_array = content.rfind(']');
    if (first_array != std::string::npos && last_array != std::string::npos && last_array >= first_array) {
        return content.substr(first_array, last_array - first_array + 1);
    }

    throw std::runtime_error("failed to locate JSON object in model output");
}

Json parse_json_from_model_output(const std::string& raw_content) {
    auto content = trim(raw_content);
    if (content.rfind("```json", 0) == 0 && content.size() >= 10) {
        content = trim(content.substr(7));
        if (content.size() >= 3 && content.ends_with("```")) {
            content = trim(content.substr(0, content.size() - 3));
        }
    } else if (content.rfind("```", 0) == 0 && content.size() >= 6) {
        content = trim(content.substr(3));
        if (content.size() >= 3 && content.ends_with("```")) {
            content = trim(content.substr(0, content.size() - 3));
        }
    }

    auto value = Json::parse(content, nullptr, false);
    if (!value.is_discarded()) {
        return value;
    }

    value = Json::parse(extract_braced_json(content), nullptr, false);
    if (value.is_discarded()) {
        throw std::runtime_error("failed to decode JSON from model output");
    }
    return value;
}

}  // namespace

Json http_post_json(
    const std::string& url,
    const Json& payload,
    const std::vector<std::string>& headers,
    std::uint64_t timeout_ms) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto request_path = temp_dir / ("maglev-request-" + random_hex(8) + ".json");
    const auto cleanup = [&]() {
        std::error_code ec;
        std::filesystem::remove(request_path, ec);
    };

    std::string error;
    if (!write_text_file(request_path, payload.dump(), error)) {
        throw std::runtime_error(error);
    }

    std::ostringstream script;
    script << "curl -sS --fail-with-body -X POST";
    for (const auto& header : headers) {
        script << " -H " << quote_shell_argument(header);
    }
    const auto timeout_seconds = std::max<std::uint64_t>(1, (timeout_ms + 999) / 1000);
    script << " --max-time " << timeout_seconds;
    script << " --data-binary @" << quote_shell_argument(request_path.string());
    script << ' ' << quote_shell_argument(url);

    const auto result = run_script(script.str(), std::filesystem::current_path());
    cleanup();
    if (result.exit_code != 0) {
        throw std::runtime_error("HTTP request failed: " + trim(result.stdout_text + "\n" + result.stderr_text));
    }

    const auto json = Json::parse(result.stdout_text, nullptr, false);
    if (json.is_discarded()) {
        throw std::runtime_error("endpoint returned invalid JSON");
    }

    return json;
}

bool is_greeting(const std::string& task) {
    const auto trimmed = trim(task);
    const auto lowered = to_lower(trimmed);
    return lowered == "hi" || lowered == "hello" || lowered == "hey" || trimmed == "привет" || trimmed == "Привет" ||
           trimmed == "здравствуй" || trimmed == "Здравствуй" || trimmed == "здравствуйте" || trimmed == "Здравствуйте";
}

bool is_model_question(const std::string& task) {
    const auto trimmed = trim(task);
    const auto lowered = to_lower(trimmed);
    return trimmed.find("какая модель") != std::string::npos || trimmed.find("Какая модель") != std::string::npos ||
           trimmed.find("что за модель") != std::string::npos || trimmed.find("Что за модель") != std::string::npos ||
           lowered.find("what model") != std::string::npos || lowered.find("which model") != std::string::npos;
}

std::string repository_context_prompt(const RepositoryContext& repository) {
    std::vector<std::string> sections;
    sections.push_back("Repository metadata:\n" + Json(repository).dump(2));

    if (!repository.attached_files.empty()) {
        std::vector<std::string> file_sections;
        for (const auto& file : repository.attached_files) {
            const auto truncation_note = file.truncated ? " (truncated)" : "";
            file_sections.push_back("Attached file: " + file.path + truncation_note + "\nContent:\n" + file.content);
        }
        sections.push_back("Attached file contents:\n" + join_strings(file_sections, "\n\n"));
    }

    return join_strings(sections, "\n\n");
}

Json parse_json_from_openai_response(const Json& response) {
    const auto candidates = extract_text_candidates_from_openai_response(response);
    for (const auto& candidate : candidates) {
        try {
            return parse_json_from_model_output(candidate);
        } catch (...) {
        }
    }

    const auto choices = response.value("choices", Json::array());
    const auto finish_reason =
        choices.is_array() && !choices.empty() ? choices.front().value("finish_reason", std::string("unknown")) : std::string("unknown");
    throw std::runtime_error("failed to decode JSON from content or reasoning_content (finish_reason: " + finish_reason + ")");
}

Json parse_json_from_native_chat_response(const Json& response) {
    const auto candidates = extract_text_candidates_from_native_chat_response(response);
    for (const auto& candidate : candidates) {
        try {
            return parse_json_from_model_output(candidate);
        } catch (...) {
        }
    }

    throw std::runtime_error("native chat response did not contain a valid JSON payload");
}

std::vector<std::string> extract_text_candidates_from_openai_response(const Json& response) {
    const auto choices = response.value("choices", Json::array());
    if (!choices.is_array() || choices.empty()) {
        return {};
    }

    const auto& choice = choices.front();
    const auto& message = choice.value("message", Json::object());

    std::vector<std::string> candidates;
    if (message.contains("content")) {
        append_json_content_candidates(candidates, message["content"]);
    }
    if (message.contains("reasoning_content")) {
        append_json_content_candidates(candidates, message["reasoning_content"]);
    }
    return candidates;
}

std::vector<std::string> extract_text_candidates_from_native_chat_response(const Json& response) {
    const auto outputs = response.value("output", Json::array());
    if (!outputs.is_array() || outputs.empty()) {
        return {};
    }

    std::vector<std::string> message_candidates;
    std::vector<std::string> fallback_candidates;
    for (const auto& item : outputs) {
        const auto type = item.value("type", std::string());
        const auto content = item.contains("content") ? item["content"] : Json{};
        if (type == "message") {
            append_json_content_candidates(message_candidates, content);
            continue;
        }
        if (type == "reasoning") {
            append_json_content_candidates(fallback_candidates, content);
            continue;
        }
        append_json_content_candidates(fallback_candidates, content);
    }

    message_candidates.insert(message_candidates.end(), fallback_candidates.begin(), fallback_candidates.end());
    return message_candidates;
}

std::string parse_text_from_openai_response(const Json& response) {
    const auto choices = response.value("choices", Json::array());
    if (!choices.is_array() || choices.empty()) {
        throw std::runtime_error("model response did not contain any choices");
    }

    const auto& choice = choices.front();
    const auto& message = choice.value("message", Json::object());
    const auto content = trim(message.value("content", std::string()));
    if (!content.empty()) {
        return content;
    }

    throw std::runtime_error(
        "failed to decode text from content (finish_reason: " + choice.value("finish_reason", std::string("unknown")) + ")");
}

std::string parse_text_from_native_chat_response(const Json& response) {
    for (const auto& candidate : extract_text_candidates_from_native_chat_response(response)) {
        if (!candidate.empty()) {
            return candidate;
        }
    }

    throw std::runtime_error("native chat response did not contain a terminal message");
}

Json build_openai_request(
    const std::string& model,
    const std::string& system_prompt,
    const std::string& user_prompt,
    double temperature,
    std::uint32_t max_tokens,
    const std::optional<std::string>& reasoning_effort) {
    Json payload = Json{
        {"model", model},
        {"messages",
         Json::array(
             {Json{{"role", "system"}, {"content", system_prompt}}, Json{{"role", "user"}, {"content", user_prompt}}})},
        {"temperature", temperature},
        {"max_tokens", max_tokens},
        {"stream", false},
    };
    if (reasoning_effort && !reasoning_effort->empty()) {
        payload["reasoning_effort"] = *reasoning_effort;
    }
    return payload;
}

}  // namespace maglev::gateway_detail
