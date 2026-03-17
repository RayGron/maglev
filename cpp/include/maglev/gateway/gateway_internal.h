#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "maglev/contracts/contracts.h"

namespace maglev::gateway_detail {

Json http_post_json(
    const std::string& url,
    const Json& payload,
    const std::vector<std::string>& headers,
    std::uint64_t timeout_ms);
bool is_greeting(const std::string& task);
bool is_model_question(const std::string& task);
std::string repository_context_prompt(const RepositoryContext& repository);
Json parse_json_from_openai_response(const Json& response);
Json parse_json_from_native_chat_response(const Json& response);
std::string parse_text_from_openai_response(const Json& response);
std::string parse_text_from_native_chat_response(const Json& response);
std::vector<std::string> extract_text_candidates_from_openai_response(const Json& response);
std::vector<std::string> extract_text_candidates_from_native_chat_response(const Json& response);
Json build_openai_request(
    const std::string& model,
    const std::string& system_prompt,
    const std::string& user_prompt,
    double temperature,
    std::uint32_t max_tokens,
    const std::optional<std::string>& reasoning_effort = std::nullopt);

}  // namespace maglev::gateway_detail
