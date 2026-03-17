#pragma once

#include "maglev/gateway/agent_gateway.h"
#include "maglev/runtime/config.h"

namespace maglev {

class OpenAiCompatGateway final : public AgentGateway {
  public:
    explicit OpenAiCompatGateway(GatewayConfig config);

    TaskPlanResponse create_task_plan(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    std::vector<EditProposal> request_edits(const std::string& run_id, const std::string& task, const RepositoryContext& repository) override;
    CommitMessageProposal request_commit_message(const std::string& run_id, const std::string& task, const std::string& diff_summary) override;
    DeployRequestProposal request_deploy(const std::string& run_id, const std::string& instruction, const RepositoryContext& repository) override;
    TerminalReply terminal_reply(const std::string& run_id, const std::string& task, const Json& details) override;

  private:
    std::string chat_text(const std::string& system_prompt, const std::string& user_prompt) const;
    Json chat_json(const std::string& system_prompt, const std::string& user_prompt) const;
    Json repair_json(const std::string& schema_prompt, const std::string& user_prompt, const std::vector<std::string>& raw_candidates) const;
    Json request_native_chat_json(const std::string& system_prompt, const std::string& user_prompt) const;

    GatewayConfig config_;
};

}  // namespace maglev
