#include "maglev/gateway/openai_compat_gateway.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "maglev/runtime/config.h"
#include "maglev/runtime/util.h"
#include "maglev/gateway/gateway_internal.h"

namespace maglev {

namespace {

bool timings_enabled() {
    return env_flag("AI_CVSC_DEBUG_TIMINGS");
}

void log_timing(const std::string& stage, std::chrono::steady_clock::duration elapsed) {
    if (!timings_enabled()) {
        return;
    }
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "[timing] " << stage << ": " << milliseconds << " ms\n";
}

void append_unique_candidates(std::vector<std::string>& destination, const std::vector<std::string>& source) {
    for (const auto& candidate : source) {
        if (trim(candidate).empty()) {
            continue;
        }
        if (std::find(destination.begin(), destination.end(), candidate) == destination.end()) {
            destination.push_back(candidate);
        }
    }
}

template <typename Value, typename Validator>
Value parse_structured_payload(const Json& json, Validator&& validator, const std::string& label) {
    const auto value = json.get<Value>();
    if (!validator(value)) {
        throw std::runtime_error("decoded " + label + " payload was incomplete");
    }
    return value;
}

bool valid_task_plan(const TaskPlanResponse& response) {
    return !trim(response.summary).empty() && !response.steps.empty();
}

bool valid_edit_proposal(const EditProposal& proposal) {
    return !trim(proposal.path).empty() && !trim(proposal.content).empty();
}

bool valid_edits(const std::vector<EditProposal>& proposals) {
    return !proposals.empty() &&
           std::all_of(proposals.begin(), proposals.end(), [](const EditProposal& proposal) { return valid_edit_proposal(proposal); });
}

bool valid_commit_message(const CommitMessageProposal& proposal) {
    return !trim(proposal.title).empty();
}

bool valid_deploy_request(const DeployRequestProposal& proposal) {
    return !trim(proposal.host).empty() && !trim(proposal.repo_path).empty() && !trim(proposal.branch).empty();
}

bool valid_terminal_reply(const TerminalReply& reply) {
    return !trim(reply.message).empty();
}

}  // namespace

OpenAiCompatGateway::OpenAiCompatGateway(GatewayConfig config) : config_(std::move(config)) {}

Json OpenAiCompatGateway::chat_json(const std::string& system_prompt, const std::string& user_prompt) const {
    std::vector<std::string> raw_candidates;
    std::runtime_error last_error("failed to decode OpenAI-compatible response");

    if (!config_.native_chat_api_url.empty()) {
        try {
            return request_native_chat_json(system_prompt, user_prompt);
        } catch (const std::exception& error) {
            last_error = std::runtime_error(error.what());
        }
    }

    for (const auto max_tokens : config_.openai_json_max_tokens) {
        const auto started_at = std::chrono::steady_clock::now();
        const auto response = gateway_detail::http_post_json(
            config_.api_base_url + "/chat/completions",
            gateway_detail::build_openai_request(
                config_.model, system_prompt, user_prompt, config_.openai_json_temperature, max_tokens, config_.openai_json_reasoning_effort),
            {"Content-Type: application/json"},
            config_.structured_request_timeout_ms);
        log_timing("gateway.openai.json_http max_tokens=" + std::to_string(max_tokens), std::chrono::steady_clock::now() - started_at);

        try {
            return gateway_detail::parse_json_from_openai_response(response);
        } catch (const std::exception& error) {
            last_error = std::runtime_error(error.what());
            append_unique_candidates(raw_candidates, gateway_detail::extract_text_candidates_from_openai_response(response));
        }
    }

    if (!raw_candidates.empty()) {
        try {
            return repair_json(system_prompt, user_prompt, raw_candidates);
        } catch (const std::exception& error) {
            last_error = std::runtime_error(error.what());
        }
    }

    throw last_error;
}

std::string OpenAiCompatGateway::chat_text(const std::string& system_prompt, const std::string& user_prompt) const {
    if (!config_.native_chat_api_url.empty()) {
        try {
            const auto started_at = std::chrono::steady_clock::now();
            const auto response = gateway_detail::http_post_json(
                config_.native_chat_api_url,
                Json{{"model", config_.chat_model}, {"system_prompt", system_prompt}, {"input", user_prompt}},
                {"Content-Type: application/json"},
                config_.request_timeout_ms);
            log_timing("gateway.native_chat_http", std::chrono::steady_clock::now() - started_at);
            return gateway_detail::parse_text_from_native_chat_response(response);
        } catch (const std::exception&) {
        }
    }

    const auto started_at = std::chrono::steady_clock::now();
    const auto response = gateway_detail::http_post_json(
        config_.api_base_url + "/chat/completions",
        gateway_detail::build_openai_request(
            config_.chat_model,
            system_prompt,
            user_prompt,
            config_.openai_chat_temperature,
            config_.openai_chat_max_tokens,
            config_.openai_chat_reasoning_effort),
        {"Content-Type: application/json"},
        config_.request_timeout_ms);
    log_timing("gateway.openai.chat_http", std::chrono::steady_clock::now() - started_at);
    return gateway_detail::parse_text_from_openai_response(response);
}

Json OpenAiCompatGateway::request_native_chat_json(const std::string& system_prompt, const std::string& user_prompt) const {
    const auto started_at = std::chrono::steady_clock::now();
    const auto response = gateway_detail::http_post_json(
        config_.native_chat_api_url,
        Json{{"model", config_.model}, {"system_prompt", system_prompt}, {"input", user_prompt}},
        {"Content-Type: application/json"},
        config_.structured_request_timeout_ms);
    log_timing("gateway.native_chat_json_http", std::chrono::steady_clock::now() - started_at);
    return gateway_detail::parse_json_from_native_chat_response(response);
}

Json OpenAiCompatGateway::repair_json(
    const std::string& schema_prompt,
    const std::string& user_prompt,
    const std::vector<std::string>& raw_candidates) const {
    const auto repair_prompt = std::string(
        "Repair the following model output into valid JSON.\n"
        "Return JSON only. Do not add markdown, commentary, or code fences.\n"
        "Target schema instruction:\n") +
        schema_prompt +
        "\n\nOriginal task prompt:\n" + user_prompt +
        "\n\nCandidate outputs to repair:\n" + join_strings(raw_candidates, "\n\n---\n\n");

    if (!config_.native_chat_api_url.empty()) {
        try {
            return request_native_chat_json(
                "You repair malformed model output into valid JSON. Return JSON only.",
                repair_prompt);
        } catch (const std::exception&) {
        }
    }

    const auto max_tokens =
        config_.openai_json_max_tokens.empty() ? 1024u : *std::max_element(config_.openai_json_max_tokens.begin(), config_.openai_json_max_tokens.end());
    const auto started_at = std::chrono::steady_clock::now();
    const auto response = gateway_detail::http_post_json(
        config_.api_base_url + "/chat/completions",
        gateway_detail::build_openai_request(
            config_.model,
            config_.prompt_profiles.repair_system_prompt,
            repair_prompt,
            0.0,
            max_tokens,
            config_.openai_json_reasoning_effort),
        {"Content-Type: application/json"},
        config_.structured_request_timeout_ms);
    log_timing("gateway.openai.json_repair_http", std::chrono::steady_clock::now() - started_at);
    return gateway_detail::parse_json_from_openai_response(response);
}

TaskPlanResponse OpenAiCompatGateway::create_task_plan(
    const std::string&,
    const std::string& task,
    const RepositoryContext& repository) {
    return parse_structured_payload<TaskPlanResponse>(
        chat_json(
            config_.prompt_profiles.task_plan_system_prompt,
            "Create an agent task plan for this task.\nTask: " + task + "\nRepository context:\n" +
                gateway_detail::repository_context_prompt(repository)),
        valid_task_plan,
        "task plan");
}

std::vector<EditProposal> OpenAiCompatGateway::request_edits(
    const std::string&,
    const std::string& task,
    const RepositoryContext& repository) {
    return parse_structured_payload<std::vector<EditProposal>>(
        chat_json(
            config_.prompt_profiles.edit_system_prompt,
            "Propose local file edits for this task.\nTask: " + task + "\nRepository context:\n" +
                gateway_detail::repository_context_prompt(repository)),
        valid_edits,
        "edit proposal list");
}

CommitMessageProposal OpenAiCompatGateway::request_commit_message(
    const std::string&,
    const std::string& task,
    const std::string& diff_summary) {
    return parse_structured_payload<CommitMessageProposal>(
        chat_json(
            config_.prompt_profiles.commit_system_prompt,
            "Generate a commit message.\nTask: " + task + "\nDiff summary:\n" + diff_summary),
        valid_commit_message,
        "commit message");
}

DeployRequestProposal OpenAiCompatGateway::request_deploy(
    const std::string&,
    const std::string& instruction,
    const RepositoryContext& repository) {
    return parse_structured_payload<DeployRequestProposal>(
        chat_json(
            config_.prompt_profiles.deploy_system_prompt,
            "Create a deploy request.\nInstruction: " + instruction + "\nRepository context:\n" +
                gateway_detail::repository_context_prompt(repository)),
        valid_deploy_request,
        "deploy request");
}

TerminalReply OpenAiCompatGateway::terminal_reply(const std::string&, const std::string& task, const Json& details) {
    const auto mode = details.value("mode", std::string("status"));
    if (mode == "chat" && gateway_detail::is_model_question(task)) {
        return TerminalReply{"Сейчас с вами взаимодействует модель " + config_.model + "."};
    }

    if (mode == "chat") {
        const auto message = chat_text(
            config_.prompt_profiles.chat_system_prompt,
            "Reply to the user in chat mode.\nUser message: " + task + "\nDetails:\n" + details.dump(2));
        return TerminalReply{message};
    }

    const auto message = parse_structured_payload<TerminalReply>(
        chat_json(
            config_.prompt_profiles.status_system_prompt,
            "Summarize the current run state for the terminal.\nTask: " + task + "\nDetails:\n" + details.dump(2)),
        valid_terminal_reply,
        "terminal reply");
    return message;
}

}  // namespace maglev
