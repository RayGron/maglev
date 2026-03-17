#pragma once

#include <optional>
#include <string>
#include <vector>

#include "maglev/contracts/agent_contracts.h"
#include "maglev/contracts/repository_contracts.h"

namespace maglev {

enum class RunStatus {
    Prepared,
    Running,
    Completed,
    Failed,
};

class RunSnapshot {
  public:
    static RunSnapshot create(std::string run_id, std::string task, std::vector<std::string> attached_files);

    const std::string& run_id() const;
    const std::string& task() const;
    const std::optional<RunStatus>& status() const;
    const std::vector<std::string>& attached_files() const;
    const std::optional<std::string>& plan_summary() const;
    const std::vector<std::string>& plan_steps() const;
    const std::optional<std::string>& terminal_reply() const;
    const std::vector<std::string>& pending_approvals() const;
    const std::vector<std::string>& approval_decisions() const;
    const std::vector<std::string>& completed_actions() const;
    const std::optional<std::string>& last_error() const;
    const std::optional<RepositoryContext>& repository() const;
    const std::vector<EditProposal>& proposed_edits() const;
    const std::vector<EditProposal>& applied_edits() const;
    const std::vector<EditProposal>& skipped_edits() const;
    const std::optional<CommitMessageProposal>& commit_proposal() const;
    const std::optional<DeployRequestProposal>& deploy_request() const;

    void mark_prepared();
    void set_plan(const TaskPlanResponse& plan);
    void set_terminal_reply(std::string message);
    void set_repository(const RepositoryContext& repository);
    void set_proposed_edits(const std::vector<EditProposal>& edits);
    void set_edit_review_result(const std::vector<EditProposal>& applied, const std::vector<EditProposal>& skipped);
    void set_commit_proposal(const CommitMessageProposal& proposal);
    void set_deploy_request(const DeployRequestProposal& proposal);
    void set_pending_approvals(const std::vector<std::string>& labels);
    void record_approval(const std::string& label, bool approved);
    void record_action(std::string action);
    void set_runtime_error(std::string error);
    void mark_completed();
    void mark_failed(std::string error);

  private:
    std::string run_id_;
    std::string task_;
    std::optional<RunStatus> status_;
    std::vector<std::string> attached_files_;
    std::optional<std::string> plan_summary_;
    std::vector<std::string> plan_steps_;
    std::optional<std::string> terminal_reply_;
    std::vector<std::string> pending_approvals_;
    std::vector<std::string> approval_decisions_;
    std::vector<std::string> completed_actions_;
    std::optional<std::string> last_error_;
    std::optional<RepositoryContext> repository_;
    std::vector<EditProposal> proposed_edits_;
    std::vector<EditProposal> applied_edits_;
    std::vector<EditProposal> skipped_edits_;
    std::optional<CommitMessageProposal> commit_proposal_;
    std::optional<DeployRequestProposal> deploy_request_;
};

class SessionState {
  public:
    void add_attached_file(const std::string& path);
    void clear_attached_files();
    const std::vector<std::string>& attached_files() const;
    const std::vector<MountedPathContext>& mounted_paths() const;
    void mount_path(const MountedPathContext& mounted_path);
    void update_mounted_path_loaded_files(const std::string& path, const std::vector<AttachedFileContext>& loaded_files);
    const RunSnapshot* active_run() const;
    const RunSnapshot* last_run() const;
    void begin_run(const std::string& run_id, const std::string& task, const std::vector<std::string>& attached_files);
    void mark_prepared();
    void set_plan(const TaskPlanResponse& plan);
    void set_terminal_reply(const std::string& message);
    void set_repository(const RepositoryContext& repository);
    void set_proposed_edits(const std::vector<EditProposal>& edits);
    void set_edit_review_result(const std::vector<EditProposal>& applied, const std::vector<EditProposal>& skipped);
    void set_commit_proposal(const CommitMessageProposal& proposal);
    void set_deploy_request(const DeployRequestProposal& proposal);
    void set_pending_approvals(const std::vector<std::string>& labels);
    void record_approval(const std::string& label, bool approved);
    void record_action(const std::string& action);
    void set_runtime_error(const std::string& error);
    void complete_run();
    void fail_run(const std::string& error);
    std::optional<std::string> last_task() const;
    std::size_t completed_runs() const;

  private:
    RunSnapshot* active_run_mut();
    void archive_active_run();

    std::vector<std::string> attached_files_;
    std::vector<MountedPathContext> mounted_paths_;
    std::optional<std::string> last_task_;
    std::size_t completed_runs_ = 0;
    std::optional<RunSnapshot> active_run_;
    std::optional<RunSnapshot> last_run_;
};

std::string run_status_label(RunStatus status);

}  // namespace maglev
