#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "maglev/contracts/agent_contracts.h"

namespace maglev::execution {

bool ask_for_approval(const std::string& question, bool auto_approve);

class WorkspaceExecutor {
  public:
    explicit WorkspaceExecutor(std::filesystem::path workspace_root);

    void apply_edits(const std::vector<EditProposal>& edits) const;
    void run_checks() const;
    void git_commit(const CommitMessageProposal& proposal) const;
    void git_push() const;
    std::string git_status_short() const;
    std::string current_branch() const;
    std::string head_commit() const;
    std::vector<std::string> changed_files() const;

  private:
    const std::filesystem::path workspace_root_;
};

class DeployExecutor {
  public:
    static std::string preview(const DeployRequestProposal& request);
    static DeployResult execute(const DeployRequestProposal& request);

  private:
    static std::string build_remote_command(const DeployRequestProposal& request);
};

}  // namespace maglev::execution
