#pragma once

#include "maglev/gateway/agent_gateway.h"

namespace maglev {

class MockGateway final : public AgentGateway {
  public:
    TaskPlanResponse create_task_plan(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    std::vector<EditProposal> request_edits(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    CommitMessageProposal request_commit_message(const std::string& run_id, const std::string& task, const std::string& diff_summary) override;
    DeployRequestProposal request_deploy(const std::string& run_id, const std::string& instruction, const RepositoryContext& repository) override;
    TerminalReply terminal_reply(const std::string& run_id, const std::string& task, const Json& details) override;
};

}  // namespace maglev
