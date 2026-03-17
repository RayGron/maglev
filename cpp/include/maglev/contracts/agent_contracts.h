#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "maglev/contracts/json_types.h"

namespace maglev {

struct PlanStep {
    std::string id;
    std::string title;
    std::string kind;
    bool requires_approval = false;
};

struct TaskPlanResponse {
    std::string summary;
    std::vector<PlanStep> steps;
};

struct EditProposal {
    std::string path;
    std::string content;
    std::string summary;
};

struct CommitMessageProposal {
    std::string title;
    std::optional<std::string> body;
};

struct DeployRequestProposal {
    std::string host;
    std::string repo_path;
    std::string branch;
    std::optional<std::string> restart_command;
};

struct DeployResult {
    bool success = false;
    std::string host;
    std::string branch;
    std::string health;
    std::vector<std::string> logs;
};

struct TerminalReply {
    std::string message;
};

enum class TaskMode {
    TaskPlan,
    EditPatch,
    CommitMessage,
    TerminalReply,
    DeployRequest,
};

struct AgentTaskRequest {
    TaskMode mode = TaskMode::TaskPlan;
    std::string run_id;
    std::string task;
    Json context;
    std::optional<Json> constraints;
    std::optional<std::map<std::string, Json>> metadata;
};

std::string to_string(TaskMode mode);
void to_json(Json& json, const PlanStep& value);
void from_json(const Json& json, PlanStep& value);
void to_json(Json& json, const TaskPlanResponse& value);
void from_json(const Json& json, TaskPlanResponse& value);
void to_json(Json& json, const EditProposal& value);
void from_json(const Json& json, EditProposal& value);
void to_json(Json& json, const CommitMessageProposal& value);
void from_json(const Json& json, CommitMessageProposal& value);
void to_json(Json& json, const DeployRequestProposal& value);
void from_json(const Json& json, DeployRequestProposal& value);
void to_json(Json& json, const DeployResult& value);
void from_json(const Json& json, DeployResult& value);
void to_json(Json& json, const TerminalReply& value);
void from_json(const Json& json, TerminalReply& value);

}  // namespace maglev
