#include "maglev/runtime/session.h"

#include <algorithm>
#include <utility>

namespace maglev {

RunSnapshot RunSnapshot::create(std::string run_id, std::string task, std::vector<std::string> attached_files) {
    RunSnapshot snapshot;
    snapshot.run_id_ = std::move(run_id);
    snapshot.task_ = std::move(task);
    snapshot.attached_files_ = std::move(attached_files);
    snapshot.status_ = RunStatus::Running;
    return snapshot;
}

const std::string& RunSnapshot::run_id() const {
    return run_id_;
}

const std::string& RunSnapshot::task() const {
    return task_;
}

const std::optional<RunStatus>& RunSnapshot::status() const {
    return status_;
}

const std::vector<std::string>& RunSnapshot::attached_files() const {
    return attached_files_;
}

const std::optional<std::string>& RunSnapshot::plan_summary() const {
    return plan_summary_;
}

const std::vector<std::string>& RunSnapshot::plan_steps() const {
    return plan_steps_;
}

const std::optional<std::string>& RunSnapshot::terminal_reply() const {
    return terminal_reply_;
}

const std::vector<std::string>& RunSnapshot::pending_approvals() const {
    return pending_approvals_;
}

const std::vector<std::string>& RunSnapshot::approval_decisions() const {
    return approval_decisions_;
}

const std::vector<std::string>& RunSnapshot::completed_actions() const {
    return completed_actions_;
}

const std::optional<std::string>& RunSnapshot::last_error() const {
    return last_error_;
}

const std::optional<RepositoryContext>& RunSnapshot::repository() const {
    return repository_;
}

const std::vector<EditProposal>& RunSnapshot::proposed_edits() const {
    return proposed_edits_;
}

const std::vector<EditProposal>& RunSnapshot::applied_edits() const {
    return applied_edits_;
}

const std::vector<EditProposal>& RunSnapshot::skipped_edits() const {
    return skipped_edits_;
}

const std::optional<CommitMessageProposal>& RunSnapshot::commit_proposal() const {
    return commit_proposal_;
}

const std::optional<DeployRequestProposal>& RunSnapshot::deploy_request() const {
    return deploy_request_;
}

void RunSnapshot::mark_prepared() {
    status_ = RunStatus::Prepared;
}

void RunSnapshot::set_plan(const TaskPlanResponse& plan) {
    plan_summary_ = plan.summary;
    plan_steps_.clear();
    for (const auto& step : plan.steps) {
        plan_steps_.push_back(step.title);
    }
}

void RunSnapshot::set_terminal_reply(std::string message) {
    terminal_reply_ = std::move(message);
}

void RunSnapshot::set_repository(const RepositoryContext& repository) {
    repository_ = repository;
}

void RunSnapshot::set_proposed_edits(const std::vector<EditProposal>& edits) {
    proposed_edits_ = edits;
    applied_edits_.clear();
    skipped_edits_.clear();
}

void RunSnapshot::set_edit_review_result(const std::vector<EditProposal>& applied, const std::vector<EditProposal>& skipped) {
    applied_edits_ = applied;
    skipped_edits_ = skipped;
}

void RunSnapshot::set_commit_proposal(const CommitMessageProposal& proposal) {
    commit_proposal_ = proposal;
}

void RunSnapshot::set_deploy_request(const DeployRequestProposal& proposal) {
    deploy_request_ = proposal;
}

void RunSnapshot::set_pending_approvals(const std::vector<std::string>& labels) {
    pending_approvals_ = labels;
}

void RunSnapshot::record_approval(const std::string& label, bool approved) {
    pending_approvals_.erase(std::remove(pending_approvals_.begin(), pending_approvals_.end(), label), pending_approvals_.end());
    approval_decisions_.push_back(label + ": " + (approved ? "approved" : "skipped"));
}

void RunSnapshot::record_action(std::string action) {
    completed_actions_.push_back(std::move(action));
}

void RunSnapshot::set_runtime_error(std::string error) {
    last_error_ = std::move(error);
}

void RunSnapshot::mark_completed() {
    status_ = RunStatus::Completed;
}

void RunSnapshot::mark_failed(std::string error) {
    status_ = RunStatus::Failed;
    last_error_ = std::move(error);
}

void SessionState::add_attached_file(const std::string& path) {
    attached_files_.push_back(path);
}

void SessionState::clear_attached_files() {
    attached_files_.clear();
}

const std::vector<std::string>& SessionState::attached_files() const {
    return attached_files_;
}

const std::vector<MountedPathContext>& SessionState::mounted_paths() const {
    return mounted_paths_;
}

void SessionState::mount_path(const MountedPathContext& mounted_path) {
    const auto existing = std::find_if(mounted_paths_.begin(), mounted_paths_.end(), [&](const MountedPathContext& current) {
        return current.path == mounted_path.path;
    });
    if (existing != mounted_paths_.end()) {
        const auto preserved_loaded_files = mounted_path.loaded_files.empty() ? existing->loaded_files : mounted_path.loaded_files;
        *existing = mounted_path;
        existing->loaded_files = preserved_loaded_files;
        return;
    }
    mounted_paths_.push_back(mounted_path);
}

void SessionState::update_mounted_path_loaded_files(
    const std::string& path,
    const std::vector<AttachedFileContext>& loaded_files) {
    const auto existing = std::find_if(mounted_paths_.begin(), mounted_paths_.end(), [&](const MountedPathContext& current) {
        return current.path == path;
    });
    if (existing == mounted_paths_.end()) {
        return;
    }
    existing->loaded_files = loaded_files;
}

const RunSnapshot* SessionState::active_run() const {
    return active_run_ ? &*active_run_ : nullptr;
}

RunSnapshot* SessionState::active_run_mut() {
    return active_run_ ? &*active_run_ : nullptr;
}

const RunSnapshot* SessionState::last_run() const {
    return last_run_ ? &*last_run_ : nullptr;
}

void SessionState::begin_run(const std::string& run_id, const std::string& task, const std::vector<std::string>& attached_files) {
    archive_active_run();
    last_task_ = task;
    active_run_ = RunSnapshot::create(run_id, task, attached_files);
}

void SessionState::archive_active_run() {
    if (active_run_) {
        last_run_ = active_run_;
        active_run_.reset();
        ++completed_runs_;
    }
}

void SessionState::mark_prepared() {
    if (auto* run = active_run_mut()) {
        run->mark_prepared();
    }
}

void SessionState::set_plan(const TaskPlanResponse& plan) {
    if (auto* run = active_run_mut()) {
        run->set_plan(plan);
    }
}

void SessionState::set_terminal_reply(const std::string& message) {
    if (auto* run = active_run_mut()) {
        run->set_terminal_reply(message);
    }
}

void SessionState::set_repository(const RepositoryContext& repository) {
    if (auto* run = active_run_mut()) {
        run->set_repository(repository);
    }
}

void SessionState::set_proposed_edits(const std::vector<EditProposal>& edits) {
    if (auto* run = active_run_mut()) {
        run->set_proposed_edits(edits);
    }
}

void SessionState::set_edit_review_result(const std::vector<EditProposal>& applied, const std::vector<EditProposal>& skipped) {
    if (auto* run = active_run_mut()) {
        run->set_edit_review_result(applied, skipped);
    }
}

void SessionState::set_commit_proposal(const CommitMessageProposal& proposal) {
    if (auto* run = active_run_mut()) {
        run->set_commit_proposal(proposal);
    }
}

void SessionState::set_deploy_request(const DeployRequestProposal& proposal) {
    if (auto* run = active_run_mut()) {
        run->set_deploy_request(proposal);
    }
}

void SessionState::set_pending_approvals(const std::vector<std::string>& labels) {
    if (auto* run = active_run_mut()) {
        run->set_pending_approvals(labels);
    }
}

void SessionState::record_approval(const std::string& label, bool approved) {
    if (auto* run = active_run_mut()) {
        run->record_approval(label, approved);
    }
}

void SessionState::record_action(const std::string& action) {
    if (auto* run = active_run_mut()) {
        run->record_action(action);
    }
}

void SessionState::set_runtime_error(const std::string& error) {
    if (auto* run = active_run_mut()) {
        run->set_runtime_error(error);
    }
}

void SessionState::complete_run() {
    if (auto* run = active_run_mut()) {
        run->mark_completed();
        archive_active_run();
    }
}

void SessionState::fail_run(const std::string& error) {
    if (auto* run = active_run_mut()) {
        run->mark_failed(error);
        archive_active_run();
    }
}

std::optional<std::string> SessionState::last_task() const {
    return last_task_;
}

std::size_t SessionState::completed_runs() const {
    return completed_runs_;
}

std::string run_status_label(RunStatus status) {
    switch (status) {
        case RunStatus::Prepared:
            return "prepared";
        case RunStatus::Running:
            return "running";
        case RunStatus::Completed:
            return "completed";
        case RunStatus::Failed:
            return "failed";
    }

    return "unknown";
}

}  // namespace maglev
