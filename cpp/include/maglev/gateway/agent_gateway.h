#pragma once

#include <string>
#include <vector>

#include "maglev/contracts/contracts.h"

namespace maglev {

class AgentGateway {
  public:
    virtual ~AgentGateway() = default;

    virtual TaskPlanResponse create_task_plan(
        const std::string& run_id,
        const std::string& task,
        const RepositoryContext& repository) = 0;
    virtual std::vector<EditProposal> request_edits(
        const std::string& run_id,
        const std::string& task,
        const RepositoryContext& repository) = 0;
    virtual CommitMessageProposal request_commit_message(
        const std::string& run_id,
        const std::string& task,
        const std::string& diff_summary) = 0;
    virtual DeployRequestProposal request_deploy(
        const std::string& run_id,
        const std::string& instruction,
        const RepositoryContext& repository) = 0;
    virtual TerminalReply terminal_reply(const std::string& run_id, const std::string& task, const Json& details) = 0;
};

}  // namespace maglev
