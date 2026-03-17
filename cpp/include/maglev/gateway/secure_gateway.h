#pragma once

#include "maglev/gateway/agent_gateway.h"
#include "maglev/runtime/auth.h"
#include "maglev/runtime/config.h"

namespace maglev {

class SecureGateway final : public AgentGateway {
  public:
    explicit SecureGateway(GatewayConfig config);

    TaskPlanResponse create_task_plan(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    std::vector<EditProposal> request_edits(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    CommitMessageProposal request_commit_message(const std::string& run_id, const std::string& task, const std::string& diff_summary) override;
    DeployRequestProposal request_deploy(const std::string& run_id, const std::string& instruction, const RepositoryContext& repository) override;
    TerminalReply terminal_reply(const std::string& run_id, const std::string& task, const Json& details) override;

  private:
    Json post(const std::string& path, const AgentTaskRequest& payload) const;
    AgentTaskRequest build_payload(TaskMode mode, const std::string& run_id, const std::string& task, const Json& context) const;

    GatewayConfig config_;
    RequestSigner signer_;
};

}  // namespace maglev
