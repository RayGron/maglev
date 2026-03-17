#include "maglev/gateway/secure_gateway.h"

#include <utility>

#include "maglev/gateway/gateway_internal.h"
#include "maglev/runtime/util.h"

namespace maglev {

SecureGateway::SecureGateway(GatewayConfig config)
    : config_(std::move(config)), signer_(SigningIdentity::load(config_.private_key_path.string())) {}

AgentTaskRequest SecureGateway::build_payload(
    TaskMode mode,
    const std::string& run_id,
    const std::string& task,
    const Json& context) const {
    AgentTaskRequest payload;
    payload.mode = mode;
    payload.run_id = run_id;
    payload.task = task;
    payload.context = context;
    payload.metadata = std::map<std::string, Json>{{"model", config_.model}};
    return payload;
}

Json SecureGateway::post(const std::string& path, const AgentTaskRequest& payload) const {
    Json metadata = Json::object();
    if (payload.metadata) {
        for (const auto& [key, value] : *payload.metadata) {
            metadata[key] = value;
        }
    }

    Json request_json{
        {"mode", to_string(payload.mode)},
        {"runId", payload.run_id},
        {"task", payload.task},
        {"context", payload.context},
        {"metadata", metadata},
    };
    if (payload.constraints) {
        request_json["constraints"] = *payload.constraints;
    }

    const auto timestamp = unix_time_millis();
    const auto nonce = std::to_string(timestamp) + "-" + random_hex(8);
    const auto canonical_request = signer_.build_canonical_request("POST", path, timestamp, nonce, request_json.dump());
    const auto signature = signer_.sign(canonical_request);

    return gateway_detail::http_post_json(
        config_.api_base_url + path,
        request_json,
        {
            "Content-Type: application/json",
            "x-ai-cvsc-key-id: " + signer_.identity().key_id(),
            "x-ai-cvsc-timestamp: " + std::to_string(timestamp),
            "x-ai-cvsc-nonce: " + nonce,
            "x-ai-cvsc-signature: " + signature,
            "x-ai-cvsc-public-key: " + signer_.identity().public_key_payload(),
        },
        config_.request_timeout_ms);
}

TaskPlanResponse SecureGateway::create_task_plan(
    const std::string& run_id,
    const std::string& task,
    const RepositoryContext& repository) {
    return post("/agent/plan", build_payload(TaskMode::TaskPlan, run_id, task, Json(repository))).get<TaskPlanResponse>();
}

std::vector<EditProposal> SecureGateway::request_edits(
    const std::string& run_id,
    const std::string& task,
    const RepositoryContext& repository) {
    return post("/agent/edits", build_payload(TaskMode::EditPatch, run_id, task, Json(repository))).get<std::vector<EditProposal>>();
}

CommitMessageProposal SecureGateway::request_commit_message(
    const std::string& run_id,
    const std::string& task,
    const std::string& diff_summary) {
    return post(
               "/agent/commit-message",
               build_payload(TaskMode::CommitMessage, run_id, task, Json{{"diffSummary", diff_summary}}))
        .get<CommitMessageProposal>();
}

DeployRequestProposal SecureGateway::request_deploy(
    const std::string& run_id,
    const std::string& instruction,
    const RepositoryContext& repository) {
    return post("/agent/deploy-request", build_payload(TaskMode::DeployRequest, run_id, instruction, Json(repository)))
        .get<DeployRequestProposal>();
}

TerminalReply SecureGateway::terminal_reply(const std::string& run_id, const std::string& task, const Json& details) {
    return post("/agent/terminal-reply", build_payload(TaskMode::TerminalReply, run_id, task, details)).get<TerminalReply>();
}

}  // namespace maglev
