#include "maglev/chat/chat_presenter.h"

#include <iostream>
#include <vector>

#include "maglev/runtime/util.h"

namespace maglev::chat_detail {

void print_plan(const TaskPlanResponse& plan) {
    std::cout << "Plan: " << plan.summary << '\n';
    for (const auto& step : plan.steps) {
        std::cout << "- [" << (step.requires_approval ? "approval" : "auto") << "] " << step.title << '\n';
    }
}

void print_run_snapshot(const std::string& title, const RunSnapshot& run) {
    std::cout << title << ":\n";
    std::cout << "- id: " << run.run_id() << '\n';
    std::cout << "- task: " << run.task() << '\n';
    if (run.status()) {
        std::cout << "- status: " << run_status_label(*run.status()) << '\n';
    }
    if (run.plan_summary()) {
        std::cout << "- plan: " << *run.plan_summary() << '\n';
    }
    if (!run.attached_files().empty()) {
        std::cout << "- attached files:\n";
        for (const auto& file : run.attached_files()) {
            std::cout << "  - " << file << '\n';
        }
    }
    if (!run.plan_steps().empty()) {
        std::cout << "- steps:\n";
        for (const auto& step : run.plan_steps()) {
            std::cout << "  - " << step << '\n';
        }
    }
    if (!run.pending_approvals().empty()) {
        std::cout << "- pending approvals:\n";
        for (const auto& item : run.pending_approvals()) {
            std::cout << "  - " << item << '\n';
        }
    }
    if (!run.approval_decisions().empty()) {
        std::cout << "- approvals:\n";
        for (const auto& item : run.approval_decisions()) {
            std::cout << "  - " << item << '\n';
        }
    }
    if (!run.completed_actions().empty()) {
        std::cout << "- actions:\n";
        for (const auto& item : run.completed_actions()) {
            std::cout << "  - " << item << '\n';
        }
    }
    if (run.terminal_reply()) {
        std::cout << "- terminal reply: " << *run.terminal_reply() << '\n';
    }
    if (!run.proposed_edits().empty()) {
        std::cout << "- proposed edits:\n";
        for (const auto& edit : run.proposed_edits()) {
            std::cout << "  - " << edit.path << ": " << edit.summary << '\n';
        }
    }
    if (!run.applied_edits().empty()) {
        std::cout << "- applied edits:\n";
        for (const auto& edit : run.applied_edits()) {
            std::cout << "  - " << edit.path << ": " << edit.summary << '\n';
        }
    }
    if (!run.skipped_edits().empty()) {
        std::cout << "- skipped edits:\n";
        for (const auto& edit : run.skipped_edits()) {
            std::cout << "  - " << edit.path << ": " << edit.summary << '\n';
        }
    }
    if (run.commit_proposal()) {
        std::cout << "- commit proposal:\n";
        std::cout << "  - title: " << run.commit_proposal()->title << '\n';
        if (run.commit_proposal()->body) {
            std::cout << "  - body: " << *run.commit_proposal()->body << '\n';
        } else {
            std::cout << "  - body: <none>\n";
        }
    }
    if (run.deploy_request()) {
        std::cout << "- deploy request:\n";
        std::cout << "  - host: " << run.deploy_request()->host << '\n';
        std::cout << "  - repoPath: " << run.deploy_request()->repo_path << '\n';
        std::cout << "  - branch: " << run.deploy_request()->branch << '\n';
        if (run.deploy_request()->restart_command) {
            std::cout << "  - restartCommand: " << *run.deploy_request()->restart_command << '\n';
        } else {
            std::cout << "  - restartCommand: <none>\n";
        }
    }
    if (run.last_error()) {
        std::cout << "- last error: " << *run.last_error() << '\n';
    }
}

void print_session_status(const SessionState& session) {
    std::cout << "Session status:\n";
    if (const auto last_task = session.last_task()) {
        std::cout << "- last task: " << *last_task << '\n';
    } else {
        std::cout << "- last task: <none>\n";
    }
    std::cout << "- completed runs: " << session.completed_runs() << '\n';

    if (!session.attached_files().empty()) {
        std::cout << "- attached files:\n";
        for (const auto& file : session.attached_files()) {
            std::cout << "  - " << file << '\n';
        }
    }
    if (!session.mounted_paths().empty()) {
        std::cout << "- mounted paths:\n";
        for (const auto& mounted_path : session.mounted_paths()) {
            std::cout << "  - " << mounted_path.path << " [" << (mounted_path.is_directory ? "directory" : "file") << "]";
            if (!mounted_path.loaded_files.empty()) {
                std::cout << " loaded=" << mounted_path.loaded_files.size();
            }
            std::cout << '\n';
        }
    }
    if (const auto* active = session.active_run()) {
        print_run_snapshot("Active run", *active);
    }
    if (const auto* last = session.last_run()) {
        print_run_snapshot("Last run", *last);
    }
}

void print_prepared_plan(const SessionState& session) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }
    if (!run->plan_summary()) {
        std::cout << "Active run has no prepared plan.\n";
        return;
    }

    std::cout << "Prepared plan: " << *run->plan_summary() << '\n';
    for (const auto& step : run->plan_steps()) {
        std::cout << "- " << step << '\n';
    }
}

std::string diff_summary_from_run(const RunSnapshot& run) {
    const auto& edits = run.applied_edits().empty() ? run.proposed_edits() : run.applied_edits();
    if (edits.empty()) {
        return "No proposed edits.";
    }

    std::vector<std::string> lines;
    for (const auto& edit : edits) {
        lines.push_back(edit.path + ": " + edit.summary);
    }
    return join_lines(lines);
}

}  // namespace maglev::chat_detail
