#include "maglev/contracts/agent_contracts.h"

namespace maglev {

std::string to_string(TaskMode mode) {
    switch (mode) {
        case TaskMode::TaskPlan:
            return "task_plan";
        case TaskMode::EditPatch:
            return "edit_patch";
        case TaskMode::CommitMessage:
            return "commit_message";
        case TaskMode::TerminalReply:
            return "terminal_reply";
        case TaskMode::DeployRequest:
            return "deploy_request";
    }

    return "task_plan";
}

void to_json(Json& json, const PlanStep& value) {
    json = Json{{"id", value.id}, {"title", value.title}, {"kind", value.kind}, {"requiresApproval", value.requires_approval}};
}

void from_json(const Json& json, PlanStep& value) {
    value.id = json.value("id", std::string{});
    value.title = json.value("title", std::string{});
    value.kind = json.value("kind", std::string{});
    value.requires_approval = json.value("requiresApproval", json.value("requires_approval", false));
}

void to_json(Json& json, const TaskPlanResponse& value) {
    json = Json{{"summary", value.summary}, {"steps", value.steps}};
}

void from_json(const Json& json, TaskPlanResponse& value) {
    value.summary = json.value("summary", std::string{});
    value.steps = json.value("steps", std::vector<PlanStep>{});
}

void to_json(Json& json, const EditProposal& value) {
    json = Json{{"path", value.path}, {"content", value.content}, {"summary", value.summary}};
}

void from_json(const Json& json, EditProposal& value) {
    value.path = json.value("path", std::string{});
    value.content = json.value("content", std::string{});
    value.summary = json.value("summary", std::string{});
}

void to_json(Json& json, const CommitMessageProposal& value) {
    json = Json{{"title", value.title}, {"body", value.body}};
}

void from_json(const Json& json, CommitMessageProposal& value) {
    value.title = json.value("title", std::string{});
    if (json.contains("body") && !json["body"].is_null()) {
        value.body = json["body"].get<std::string>();
    } else {
        value.body.reset();
    }
}

void to_json(Json& json, const DeployRequestProposal& value) {
    json = Json{{"host", value.host}, {"repoPath", value.repo_path}, {"branch", value.branch}, {"restartCommand", value.restart_command}};
}

void from_json(const Json& json, DeployRequestProposal& value) {
    value.host = json.value("host", std::string{});
    value.repo_path = json.value("repoPath", json.value("repo_path", std::string{}));
    value.branch = json.value("branch", std::string{});
    if (json.contains("restartCommand") && !json["restartCommand"].is_null()) {
        value.restart_command = json["restartCommand"].get<std::string>();
    } else if (json.contains("restart_command") && !json["restart_command"].is_null()) {
        value.restart_command = json["restart_command"].get<std::string>();
    } else {
        value.restart_command.reset();
    }
}

void to_json(Json& json, const DeployResult& value) {
    json = Json{
        {"success", value.success},
        {"host", value.host},
        {"branch", value.branch},
        {"health", value.health},
        {"logs", value.logs},
    };
}

void from_json(const Json& json, DeployResult& value) {
    value.success = json.value("success", false);
    value.host = json.value("host", std::string{});
    value.branch = json.value("branch", std::string{});
    value.health = json.value("health", std::string{});
    value.logs = json.value("logs", std::vector<std::string>{});
}

void to_json(Json& json, const TerminalReply& value) {
    json = Json{{"message", value.message}};
}

void from_json(const Json& json, TerminalReply& value) {
    value.message = json.value("message", std::string{});
}

}  // namespace maglev
