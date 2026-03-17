#include "maglev/runtime/config.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "maglev/runtime/util.h"

namespace maglev {

namespace {

std::string default_chat_system_prompt() {
    return "You are the terminal chat mode of a coding CLI. Reply directly to the user message. Do not propose a plan unless the user explicitly asks to execute a task. Keep the answer short, natural, and terminal-friendly.";
}

std::string default_task_plan_system_prompt() {
    return "Return JSON only. Schema: {\"summary\":string,\"steps\":[{\"id\":string,\"title\":string,\"kind\":string,\"requiresApproval\":boolean}]}. No markdown.";
}

std::string default_edit_system_prompt() {
    return "Return JSON only. Schema: [{\"path\":string,\"content\":string,\"summary\":string}]. Prefer the smallest possible set of edits. Do not create extra files unless the user explicitly asks for them. No markdown fences.";
}

std::string default_commit_system_prompt() {
    return "Return JSON only. Schema: {\"title\":string,\"body\":string|null}. Keep it short and conventional.";
}

std::string default_deploy_system_prompt() {
    return "Return JSON only. Schema: {\"host\":string,\"repoPath\":string,\"branch\":string,\"restartCommand\":string|null}. Extract the target host from the instruction if present, otherwise keep the current repository path and branch.";
}

std::string default_status_system_prompt() {
    return "Return JSON only. Schema: {\"message\":string}. The message must be short and terminal-friendly.";
}

std::string default_repair_system_prompt() {
    return "You repair malformed model output into valid JSON. Return JSON only.";
}

std::optional<std::string> env_optional(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr || std::string(value).empty()) {
        return std::nullopt;
    }
    return std::string(value);
}

std::filesystem::path default_private_key_path() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".ai-cvsc" / "id_ed25519";
    }
    if (const char* profile = std::getenv("USERPROFILE")) {
        return std::filesystem::path(profile) / ".ai-cvsc" / "id_ed25519";
    }
    return std::filesystem::path(".") / ".ai-cvsc" / "id_ed25519";
}

BackendMode parse_backend_mode(const std::optional<std::string>& value) {
    if (value && *value == "secure_gateway") {
        return BackendMode::SecureGateway;
    }
    return BackendMode::OpenAiCompat;
}

std::vector<std::uint32_t> normalize_max_tokens(std::vector<std::uint32_t> values) {
    values.erase(std::remove(values.begin(), values.end(), 0), values.end());
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    if (values.empty()) {
        return {512, 1024};
    }
    return values;
}

std::string infer_native_chat_api_url(const std::string& api_base_url) {
    if (api_base_url.empty()) {
        return "http://127.0.0.1:1234/api/v1/chat";
    }

    auto derived = api_base_url;
    if (derived.ends_with("/v1")) {
        derived.resize(derived.size() - 3);
    } else if (derived.ends_with("/v1/")) {
        derived.resize(derived.size() - 4);
    }
    if (!derived.empty() && derived.back() == '/') {
        derived.pop_back();
    }
    return derived + "/api/v1/chat";
}

}  // namespace

bool env_flag(const std::string& name) {
    const auto value = to_lower(getenv_or_empty(name));
    return value == "1" || value == "true" || value == "yes";
}

std::string getenv_or_empty(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value == nullptr ? std::string() : std::string(value);
}

GatewayConfig GatewayConfig::from_environment() {
    GatewayConfig config;
    config.workspace_root = std::filesystem::current_path();

    nlohmann::json runtime = nlohmann::json::object();
    auto config_path = config.workspace_root / "config" / "model-endpoints.json";
    if (const auto runtime_path = env_optional("AI_CVSC_RUNTIME_CONFIG_PATH")) {
        config_path = std::filesystem::path(*runtime_path);
    }
    if (std::filesystem::exists(config_path)) {
        runtime = nlohmann::json::parse(read_text_file(config_path), nullptr, false);
        if (runtime.is_discarded()) {
            runtime = nlohmann::json::object();
        }
    }

    auto backend_mode_value = env_optional("AI_CVSC_BACKEND_MODE");
    if (!backend_mode_value && runtime.contains("defaultBackendMode")) {
        backend_mode_value = runtime["defaultBackendMode"].get<std::string>();
    }
    config.backend_mode = parse_backend_mode(backend_mode_value);

    const auto backend_key = config.backend_mode == BackendMode::OpenAiCompat ? "openaiCompat" : "secureGateway";
    const auto backend = runtime.value(backend_key, nlohmann::json::object());
    const auto json_profile = backend.value("jsonResponseProfile", nlohmann::json::object());
    const auto chat_profile = backend.value("chatResponseProfile", nlohmann::json::object());
    const auto prompt_profiles = backend.value("promptProfiles", nlohmann::json::object());

    config.api_base_url = env_optional("AI_CVSC_API_BASE_URL").value_or(backend.value("apiBaseUrl", std::string("http://127.0.0.1:1234/v1")));
    config.model = env_optional("AI_CVSC_MODEL").value_or(backend.value("model", std::string("qwen/qwen3.5-35b-a3b")));
    config.chat_model = env_optional("AI_CVSC_CHAT_MODEL").value_or(backend.value("chatModel", config.model));
    config.native_chat_api_url =
        env_optional("AI_CVSC_NATIVE_CHAT_API_URL").value_or(backend.value("chatApiUrl", infer_native_chat_api_url(config.api_base_url)));
    config.request_timeout_ms = backend.value("requestTimeoutMs", static_cast<std::uint64_t>(30000));
    if (const auto timeout_value = env_optional("AI_CVSC_REQUEST_TIMEOUT_MS")) {
        config.request_timeout_ms = static_cast<std::uint64_t>(std::stoull(*timeout_value));
    }
    config.structured_request_timeout_ms = backend.value("structuredRequestTimeoutMs", static_cast<std::uint64_t>(90000));
    if (const auto timeout_value = env_optional("AI_CVSC_STRUCTURED_REQUEST_TIMEOUT_MS")) {
        config.structured_request_timeout_ms = static_cast<std::uint64_t>(std::stoull(*timeout_value));
    }
    config.openai_chat_temperature = chat_profile.value("temperature", 0.2);
    if (const auto temperature = env_optional("AI_CVSC_OPENAI_CHAT_TEMPERATURE")) {
        config.openai_chat_temperature = std::stod(*temperature);
    }
    config.openai_json_temperature = json_profile.value("temperature", 0.2);
    if (const auto temperature = env_optional("AI_CVSC_OPENAI_JSON_TEMPERATURE")) {
        config.openai_json_temperature = std::stod(*temperature);
    }
    config.openai_chat_max_tokens = chat_profile.value("maxTokens", static_cast<std::uint32_t>(128));
    if (const auto chat_max_tokens = env_optional("AI_CVSC_OPENAI_CHAT_MAX_TOKENS")) {
        config.openai_chat_max_tokens = static_cast<std::uint32_t>(std::stoul(*chat_max_tokens));
    }
    config.openai_json_max_tokens = normalize_max_tokens(json_profile.value("maxTokens", std::vector<std::uint32_t>{512, 1024}));
    config.openai_chat_reasoning_effort =
        env_optional("AI_CVSC_OPENAI_CHAT_REASONING_EFFORT").value_or(chat_profile.value("reasoningEffort", std::string()));
    if (config.openai_chat_reasoning_effort && config.openai_chat_reasoning_effort->empty()) {
        config.openai_chat_reasoning_effort.reset();
    }
    config.openai_json_reasoning_effort =
        env_optional("AI_CVSC_OPENAI_JSON_REASONING_EFFORT").value_or(json_profile.value("reasoningEffort", std::string()));
    if (config.openai_json_reasoning_effort && config.openai_json_reasoning_effort->empty()) {
        config.openai_json_reasoning_effort.reset();
    }
    config.prompt_profiles.chat_system_prompt =
        prompt_profiles.value("chat", nlohmann::json::object()).value("systemPrompt", default_chat_system_prompt());
    config.prompt_profiles.task_plan_system_prompt =
        prompt_profiles.value("taskPlan", nlohmann::json::object()).value("systemPrompt", default_task_plan_system_prompt());
    config.prompt_profiles.edit_system_prompt =
        prompt_profiles.value("edit", nlohmann::json::object()).value("systemPrompt", default_edit_system_prompt());
    config.prompt_profiles.commit_system_prompt =
        prompt_profiles.value("commit", nlohmann::json::object()).value("systemPrompt", default_commit_system_prompt());
    config.prompt_profiles.deploy_system_prompt =
        prompt_profiles.value("deploy", nlohmann::json::object()).value("systemPrompt", default_deploy_system_prompt());
    config.prompt_profiles.status_system_prompt =
        prompt_profiles.value("status", nlohmann::json::object()).value("systemPrompt", default_status_system_prompt());
    config.prompt_profiles.repair_system_prompt =
        prompt_profiles.value("repair", nlohmann::json::object()).value("systemPrompt", default_repair_system_prompt());
    config.private_key_path = default_private_key_path();
    if (const auto private_key_path = env_optional("AI_CVSC_PRIVATE_KEY_PATH")) {
        config.private_key_path = std::filesystem::path(*private_key_path);
    }

    if (auto public_key = env_optional("AI_CVSC_PUBLIC_KEY_PATH")) {
        config.public_key_path = *public_key;
        config.has_public_key_path = true;
    }

    config.use_mock_gateway = env_flag("AI_CVSC_USE_MOCK_GATEWAY");
    return config;
}

}  // namespace maglev
