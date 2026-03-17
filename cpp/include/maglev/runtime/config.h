#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace maglev {

enum class BackendMode {
    OpenAiCompat,
    SecureGateway,
};

struct PromptProfiles {
    std::string chat_system_prompt;
    std::string task_plan_system_prompt;
    std::string edit_system_prompt;
    std::string commit_system_prompt;
    std::string deploy_system_prompt;
    std::string status_system_prompt;
    std::string repair_system_prompt;
};

struct GatewayConfig {
    BackendMode backend_mode = BackendMode::OpenAiCompat;
    std::string api_base_url;
    std::string model;
    std::string chat_model;
    std::string native_chat_api_url;
    std::uint64_t request_timeout_ms = 30000;
    std::uint64_t structured_request_timeout_ms = 90000;
    double openai_chat_temperature = 0.2;
    std::optional<std::string> openai_chat_reasoning_effort;
    double openai_json_temperature = 0.2;
    std::optional<std::string> openai_json_reasoning_effort;
    std::uint32_t openai_chat_max_tokens = 128;
    std::vector<std::uint32_t> openai_json_max_tokens;
    PromptProfiles prompt_profiles;
    std::filesystem::path private_key_path;
    std::filesystem::path public_key_path;
    bool has_public_key_path = false;
    bool use_mock_gateway = false;
    std::filesystem::path workspace_root;

    static GatewayConfig from_environment(const std::optional<std::filesystem::path>& config_path = std::nullopt);
};

bool env_flag(const std::string& name);
std::string getenv_or_empty(const std::string& name);

}  // namespace maglev
