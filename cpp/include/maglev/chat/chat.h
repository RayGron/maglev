#pragma once

#include <string>
#include <vector>

#include "maglev/gateway/agent_gateway.h"
#include "maglev/runtime/logging.h"
#include "maglev/runtime/session.h"

namespace maglev {

struct TaskInput {
    std::string task;
    std::vector<std::string> attached_files;
};

void run_chat_session(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const TaskInput& input,
    bool auto_approve,
    SessionLogger* logger = nullptr,
    SessionState* session = nullptr);
void run_one_shot_input(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const TaskInput& input,
    bool auto_approve,
    SessionLogger* logger = nullptr);
void run_interactive_session(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const std::vector<std::string>& attached_files,
    bool auto_approve,
    SessionLogger* logger = nullptr);

}  // namespace maglev
