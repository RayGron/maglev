#include "maglev/runtime/execution.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "maglev/runtime/util.h"

namespace maglev::execution {

namespace {

void ensure_success(const CommandResult& result, const std::string& action) {
    if (result.exit_code != 0) {
        std::ostringstream stream;
        stream << action << " failed";
        if (!trim(result.stdout_text).empty()) {
            stream << ": " << trim(result.stdout_text);
        } else if (!trim(result.stderr_text).empty()) {
            stream << ": " << trim(result.stderr_text);
        }
        throw std::runtime_error(stream.str());
    }
}

}  // namespace

bool ask_for_approval(const std::string& question, bool auto_approve) {
    if (auto_approve) {
        return true;
    }

    std::cout << question << " [y/N] " << std::flush;
    std::string answer;
    std::getline(std::cin, answer);
    const auto normalized = to_lower(trim(answer));
    return normalized == "y" || normalized == "yes";
}

WorkspaceExecutor::WorkspaceExecutor(std::filesystem::path workspace_root) : workspace_root_(std::move(workspace_root)) {}

void WorkspaceExecutor::apply_edits(const std::vector<EditProposal>& edits) const {
    for (const auto& edit : edits) {
        std::string error;
        if (!write_text_file(workspace_root_ / edit.path, edit.content, error)) {
            throw std::runtime_error(error);
        }
    }
}

void WorkspaceExecutor::run_checks() const {
    ensure_success(run_script("npm run check", workspace_root_), "local checks");
}

void WorkspaceExecutor::git_commit(const CommitMessageProposal& proposal) const {
    ensure_success(run_script("git add .", workspace_root_), "git add");

    std::ostringstream script;
    script << "git commit -m " << quote_shell_argument(proposal.title);
    if (proposal.body && !proposal.body->empty()) {
        script << " -m " << quote_shell_argument(*proposal.body);
    }
    ensure_success(run_script(script.str(), workspace_root_), "git commit");
}

void WorkspaceExecutor::git_push() const {
    const auto branch = current_branch();
    if (branch.empty()) {
        throw std::runtime_error("failed to resolve current branch");
    }

    ensure_success(run_script("git push origin " + quote_shell_argument(branch), workspace_root_), "git push");
}

std::string WorkspaceExecutor::git_status_short() const {
    auto result = run_script("git status --short", workspace_root_);
    ensure_success(result, "git status --short");
    const auto output = trim(result.stdout_text);
    return output.empty() ? std::string("No uncommitted changes.") : output;
}

std::string WorkspaceExecutor::current_branch() const {
    auto result = run_script("git branch --show-current", workspace_root_);
    ensure_success(result, "git branch --show-current");
    return trim(result.stdout_text);
}

std::string WorkspaceExecutor::head_commit() const {
    auto result = run_script("git rev-parse HEAD", workspace_root_);
    ensure_success(result, "git rev-parse HEAD");
    return trim(result.stdout_text);
}

std::vector<std::string> WorkspaceExecutor::changed_files() const {
    auto result = run_script("git status --short", workspace_root_);
    ensure_success(result, "git status --short");

    std::vector<std::string> files;
    for (const auto& line : split_lines(result.stdout_text)) {
        const auto trimmed_line = trim(line);
        if (trimmed_line.size() <= 3) {
            continue;
        }
        files.push_back(trimmed_line.substr(3));
    }
    return files;
}

std::string DeployExecutor::build_remote_command(const DeployRequestProposal& request) {
    return request.restart_command ? "cd " + request.repo_path + " && git pull origin " + request.branch + " && " +
                                         *request.restart_command
                                   : "cd " + request.repo_path + " && git pull origin " + request.branch;
}

std::string DeployExecutor::preview(const DeployRequestProposal& request) {
    const auto remote_command = build_remote_command(request);
    return "Host: " + request.host + "\nSSH command: ssh " + request.host + "\nRemote command: " + remote_command;
}

DeployResult DeployExecutor::execute(const DeployRequestProposal& request) {
    const auto remote_command = build_remote_command(request);
    std::ostringstream script;
    script << "ssh " << quote_shell_argument(request.host) << " " << quote_shell_argument(remote_command);
    auto result = run_script(script.str(), std::filesystem::current_path());

    DeployResult deploy_result;
    deploy_result.success = result.exit_code == 0;
    deploy_result.host = request.host;
    deploy_result.branch = request.branch;
    deploy_result.health = result.exit_code == 0 ? "ok" : "failed";
    deploy_result.logs.push_back(result.stdout_text);
    deploy_result.logs.push_back(result.stderr_text);
    return deploy_result;
}

}  // namespace maglev::execution
