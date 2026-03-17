#include "maglev/chat/chat.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "maglev/chat/chat_commands.h"
#include "maglev/chat/chat_intent.h"
#include "maglev/chat/chat_presenter.h"
#include "maglev/chat/chat_repository.h"
#include "maglev/gateway/gateway_internal.h"
#include "maglev/runtime/config.h"
#include "maglev/runtime/execution.h"
#include "maglev/runtime/util.h"

namespace maglev {

namespace {

bool timings_enabled() {
    return env_flag("AI_CVSC_DEBUG_TIMINGS");
}

void log_timing(const std::string& stage, std::chrono::steady_clock::duration elapsed) {
    if (!timings_enabled()) {
        return;
    }
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "[timing] " << stage << ": " << milliseconds << " ms\n";
}

void log_event(SessionLogger* logger, const std::string& type, const Json& payload) {
    if (logger == nullptr) {
        return;
    }
    logger->log_event(type, payload);
}

TaskPlanResponse make_local_plan(std::string summary, std::vector<PlanStep> steps) {
    return TaskPlanResponse{std::move(summary), std::move(steps)};
}

std::string make_local_terminal_reply(const TaskPlanResponse& plan) {
    if (!plan.steps.empty()) {
        return "Подготовка завершена. Следующий шаг: " + plan.steps.front().title;
    }
    return "Подготовка завершена.";
}

TaskPlanResponse make_commit_plan() {
    return make_local_plan(
        "Подготовить и выполнить git commit для текущих изменений.",
        {PlanStep{"stage-current-changes", "Подготовить текущие изменения к коммиту", "git", false},
         PlanStep{"commit-current-changes", "Создать коммит для текущих изменений", "git", true}});
}

TaskPlanResponse make_push_plan(const std::string& branch) {
    return make_local_plan(
        "Подготовить и выполнить git push текущей ветки в origin.",
        {PlanStep{"push-current-branch", "Отправить ветку " + branch + " в origin", "git", true}});
}

void print_edit_selection_menu(const std::vector<EditProposal>& edits) {
    std::cout << "Proposed edits:\n";
    for (std::size_t index = 0; index < edits.size(); ++index) {
        const auto& edit = edits[index];
        std::cout << "  " << (index + 1) << ". " << edit.path;
        if (!trim(edit.summary).empty()) {
            std::cout << " - " << edit.summary;
        }
        std::cout << '\n';
    }
}

std::optional<std::vector<std::size_t>> parse_selected_edit_indexes(
    const std::string& input,
    std::size_t count) {
    const auto normalized = to_lower(trim(input));
    if (normalized.empty() || normalized == "all" || normalized == "*") {
        std::vector<std::size_t> all_indexes(count);
        for (std::size_t index = 0; index < count; ++index) {
            all_indexes[index] = index;
        }
        return all_indexes;
    }

    if (normalized == "none" || normalized == "skip") {
        return std::vector<std::size_t>{};
    }

    std::vector<std::size_t> indexes;
    std::unordered_set<std::size_t> seen;
    std::stringstream stream(normalized);
    std::string token;
    while (std::getline(stream, token, ',')) {
        const auto value = trim(token);
        if (value.empty()) {
            continue;
        }
        std::size_t parsed = 0;
        try {
            parsed = static_cast<std::size_t>(std::stoul(value));
        } catch (...) {
            return std::nullopt;
        }
        if (parsed == 0 || parsed > count) {
            return std::nullopt;
        }
        const auto zero_based = parsed - 1;
        if (seen.insert(zero_based).second) {
            indexes.push_back(zero_based);
        }
    }

    return indexes;
}

std::vector<std::size_t> prompt_selected_edit_indexes(const std::vector<EditProposal>& edits, bool auto_approve) {
    if (auto_approve) {
        std::vector<std::size_t> indexes(edits.size());
        for (std::size_t index = 0; index < edits.size(); ++index) {
            indexes[index] = index;
        }
        return indexes;
    }

    print_edit_selection_menu(edits);
    while (true) {
        std::cout << "Select edit numbers to apply [all]: " << std::flush;
        std::string answer;
        std::getline(std::cin, answer);
        if (const auto parsed = parse_selected_edit_indexes(answer, edits.size())) {
            return *parsed;
        }
        std::cout << "Invalid selection. Use comma-separated numbers, `all`, or `none`.\n";
    }
}

CommitMessageProposal review_commit_proposal(const CommitMessageProposal& proposal, bool auto_approve) {
    if (auto_approve) {
        return proposal;
    }

    CommitMessageProposal reviewed = proposal;
    std::cout << "Commit proposal:\n";
    std::cout << "- title: " << reviewed.title << '\n';
    if (reviewed.body) {
        std::cout << "- body: " << *reviewed.body << '\n';
    } else {
        std::cout << "- body: <none>\n";
    }

    std::cout << "Edit title [leave empty to keep]: " << std::flush;
    std::string title_input;
    std::getline(std::cin, title_input);
    title_input = trim(title_input);
    if (!title_input.empty()) {
        reviewed.title = title_input;
    }

    std::cout << "Edit body [leave empty to keep, '-' to clear]: " << std::flush;
    std::string body_input;
    std::getline(std::cin, body_input);
    body_input = trim(body_input);
    if (body_input == "-") {
        reviewed.body.reset();
    } else if (!body_input.empty()) {
        reviewed.body = body_input;
    }

    std::cout << "Reviewed commit proposal:\n";
    std::cout << "- title: " << reviewed.title << '\n';
    if (reviewed.body) {
        std::cout << "- body: " << *reviewed.body << '\n';
    } else {
        std::cout << "- body: <none>\n";
    }
    return reviewed;
}

DeployRequestProposal review_deploy_request(const DeployRequestProposal& proposal, bool auto_approve) {
    if (auto_approve) {
        return proposal;
    }

    DeployRequestProposal reviewed = proposal;
    std::cout << "Deploy proposal:\n" << execution::DeployExecutor::preview(reviewed) << '\n';

    std::cout << "Edit host [leave empty to keep]: " << std::flush;
    std::string host_input;
    std::getline(std::cin, host_input);
    host_input = trim(host_input);
    if (!host_input.empty()) {
        reviewed.host = host_input;
    }

    std::cout << "Edit repo path [leave empty to keep]: " << std::flush;
    std::string repo_path_input;
    std::getline(std::cin, repo_path_input);
    repo_path_input = trim(repo_path_input);
    if (!repo_path_input.empty()) {
        reviewed.repo_path = repo_path_input;
    }

    std::cout << "Edit branch [leave empty to keep]: " << std::flush;
    std::string branch_input;
    std::getline(std::cin, branch_input);
    branch_input = trim(branch_input);
    if (!branch_input.empty()) {
        reviewed.branch = branch_input;
    }

    std::cout << "Edit restart command [leave empty to keep, '-' to clear]: " << std::flush;
    std::string restart_input;
    std::getline(std::cin, restart_input);
    restart_input = trim(restart_input);
    if (restart_input == "-") {
        reviewed.restart_command.reset();
    } else if (!restart_input.empty()) {
        reviewed.restart_command = restart_input;
    }

    std::cout << "Reviewed deploy proposal:\n" << execution::DeployExecutor::preview(reviewed) << '\n';
    return reviewed;
}

void run_terminal_chat_reply(
    AgentGateway& gateway,
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files,
    const std::string& task,
    SessionLogger* logger) {
    log_event(
        logger,
        "chat.prompt",
        Json{{"task", task}, {"attachedFiles", attached_files}, {"workspaceRoot", workspace_root.string()}});
    if (chat_detail::looks_like_identity_question(task)) {
        std::cout << "Я терминальный ассистент для работы с кодом.\n";
        log_event(logger, "chat.reply", Json{{"message", "Я терминальный ассистент для работы с кодом."}});
        return;
    }
    if (chat_detail::looks_like_capability_question(task)) {
        std::cout << "Я могу отвечать в чате и запускать агентный workflow: план, правки, проверки, коммит, пуш и деплой.\n";
        log_event(
            logger,
            "chat.reply",
            Json{{"message", "Я могу отвечать в чате и запускать агентный workflow: план, правки, проверки, коммит, пуш и деплой."}});
        return;
    }

    Json details = {
        {"mode", "chat"},
        {"platform", platform_name()},
        {"compiler", compiler_name()},
    };
    if (gateway_detail::is_model_question(task)) {
        const auto started_at = std::chrono::steady_clock::now();
        const auto reply = gateway.terminal_reply(make_run_id(), task, details);
        log_timing("chat.reply", std::chrono::steady_clock::now() - started_at);
        std::cout << reply.message << '\n';
        log_event(logger, "chat.reply", Json{{"message", reply.message}});
        return;
    }

    const auto context_started_at = std::chrono::steady_clock::now();
    details["attachedFiles"] = chat_detail::load_attached_file_context(workspace_root, attached_files);
    log_timing("chat.load_attached_files", std::chrono::steady_clock::now() - context_started_at);

    const auto reply_started_at = std::chrono::steady_clock::now();
    const auto reply = gateway.terminal_reply(make_run_id(), task, details);
    log_timing("chat.reply", std::chrono::steady_clock::now() - reply_started_at);
    std::cout << reply.message << '\n';
    log_event(logger, "chat.reply", Json{{"message", reply.message}});
}

void run_local_uncommitted_changes_task(
    const std::filesystem::path& workspace_root,
    const std::string& task,
    SessionState& session,
    SessionLogger* logger) {
    execution::WorkspaceExecutor workspace_executor(workspace_root);
    const auto run_id = make_run_id();
    session.begin_run(run_id, task, session.attached_files());
    session.set_pending_approvals({});
    session.set_plan(TaskPlanResponse{"Inspect local git status and print uncommitted changes.", {}});

    const auto status = workspace_executor.git_status_short();
    session.set_terminal_reply(status);
    session.record_action("git status --short");
    std::cout << "Uncommitted changes:\n" << status << '\n';
    log_event(logger, "task.local_uncommitted_changes", Json{{"task", task}, {"status", status}});
    session.complete_run();
}

void prepare_agent_run(
    AgentGateway& gateway,
    const std::filesystem::path& workspace_root,
    const std::string& task,
    SessionState& session,
    SessionLogger* logger) {
    const auto repository_started_at = std::chrono::steady_clock::now();
    const auto repository = chat_detail::load_repository_context(workspace_root, session.attached_files());
    log_timing("agent.load_repository_context", std::chrono::steady_clock::now() - repository_started_at);
    const auto run_id = make_run_id();
    session.begin_run(run_id, task, session.attached_files());
    log_event(
        logger,
        "task.begin",
        Json{{"runId", run_id}, {"task", task}, {"attachedFiles", session.attached_files()}, {"workspaceRoot", workspace_root.string()}});
    session.set_repository(repository);
    session.set_pending_approvals(chat_detail::prepared_approval_labels(task));

    if (chat_detail::looks_like_commit_request(task)) {
        const auto plan = make_commit_plan();
        session.set_plan(plan);
        chat_detail::print_plan(plan);
        session.set_terminal_reply(make_local_terminal_reply(plan));
        std::cout << *session.active_run()->terminal_reply() << '\n';
        session.mark_prepared();
        log_event(
            logger,
            "task.plan",
            Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
        log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", *session.active_run()->terminal_reply()}});
        std::cout << "Active task prepared. Available next step: /commit\n";
        return;
    }

    if (chat_detail::looks_like_push_request(task)) {
        const auto plan = make_push_plan(repository.branch.empty() ? "current branch" : repository.branch);
        session.set_plan(plan);
        chat_detail::print_plan(plan);
        session.set_terminal_reply(make_local_terminal_reply(plan));
        std::cout << *session.active_run()->terminal_reply() << '\n';
        session.mark_prepared();
        log_event(
            logger,
            "task.plan",
            Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
        log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", *session.active_run()->terminal_reply()}});
        std::cout << "Active task prepared. Available next step: /push\n";
        return;
    }

    const auto plan_started_at = std::chrono::steady_clock::now();
    const auto plan = gateway.create_task_plan(run_id, task, repository);
    log_timing("agent.create_task_plan", std::chrono::steady_clock::now() - plan_started_at);
    session.set_plan(plan);
    chat_detail::print_plan(plan);
    log_event(
        logger,
        "task.plan",
        Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
    const auto local_reply = make_local_terminal_reply(plan);
    session.set_terminal_reply(local_reply);
    std::cout << local_reply << '\n';
    log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", local_reply}});

    const auto edits_started_at = std::chrono::steady_clock::now();
    const auto edits = gateway.request_edits(run_id, task, repository);
    log_timing("agent.request_edits", std::chrono::steady_clock::now() - edits_started_at);
    session.set_proposed_edits(edits);
    session.mark_prepared();
    log_event(logger, "task.edits_prepared", Json{{"runId", run_id}, {"count", edits.size()}, {"edits", Json(edits)}});
    std::cout << "Prepared " << edits.size() << " proposed edit(s). Use /apply to write them locally.\n";
    std::cout << "Active task prepared. Available next steps: /plan /apply /checks /commit /push /deploy\n";
}

void prepare_or_execute_agent_input(
    AgentGateway& gateway,
    const std::filesystem::path& workspace_root,
    const std::string& task,
    SessionState& session,
    SessionLogger* logger) {
    if (chat_detail::looks_like_uncommitted_changes_request(task)) {
        run_local_uncommitted_changes_task(workspace_root, task, session, logger);
        return;
    }
    prepare_agent_run(gateway, workspace_root, task, session, logger);
}

void apply_active_run(const std::filesystem::path& workspace_root, bool auto_approve, SessionState& session, SessionLogger* logger) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }
    if (run->proposed_edits().empty()) {
        std::cout << "Active run has no proposed edits.\n";
        return;
    }

    const bool approved = execution::ask_for_approval(chat_detail::kApplyPreparedApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kApplyPreparedApprovalLabel, approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kApplyPreparedApprovalLabel}, {"approved", approved}});
    if (!approved) {
        return;
    }

    execution::WorkspaceExecutor workspace_executor(workspace_root);
    const auto selected_indexes = prompt_selected_edit_indexes(run->proposed_edits(), auto_approve);
    std::vector<EditProposal> applied_edits;
    std::vector<EditProposal> skipped_edits;
    applied_edits.reserve(selected_indexes.size());

    std::unordered_set<std::size_t> selected_lookup(selected_indexes.begin(), selected_indexes.end());
    for (std::size_t index = 0; index < run->proposed_edits().size(); ++index) {
        const auto& edit = run->proposed_edits()[index];
        if (selected_lookup.contains(index)) {
            applied_edits.push_back(edit);
        } else {
            skipped_edits.push_back(edit);
        }
    }

    if (applied_edits.empty()) {
        session.set_edit_review_result({}, run->proposed_edits());
        session.record_action("Skipped all prepared file changes");
        log_event(logger, "task.edit_review", Json{{"applied", Json::array()}, {"skipped", Json(run->proposed_edits())}});
        std::cout << "No file changes were applied.\n";
        return;
    }

    workspace_executor.apply_edits(applied_edits);
    session.set_edit_review_result(applied_edits, skipped_edits);
    session.record_action("Applied " + std::to_string(applied_edits.size()) + " selected file change(s)");
    log_event(logger, "task.edit_review", Json{{"applied", Json(applied_edits)}, {"skipped", Json(skipped_edits)}});
    std::cout << "Applied " << applied_edits.size() << " file change(s)";
    if (!skipped_edits.empty()) {
        std::cout << "; skipped " << skipped_edits.size();
    }
    std::cout << ".\n";
}

void run_checks_command(const std::filesystem::path& workspace_root, bool auto_approve, SessionState& session, SessionLogger* logger) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }

    const bool approved = execution::ask_for_approval(chat_detail::kChecksApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kChecksApprovalLabel, approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kChecksApprovalLabel}, {"approved", approved}});
    if (!approved) {
        return;
    }

    execution::WorkspaceExecutor workspace_executor(workspace_root);
    workspace_executor.run_checks();
    session.record_action("Run local checks");
    log_event(logger, "task.checks", Json{{"result", "passed"}});
    std::cout << "Local checks passed.\n";
}

void commit_active_run(
    AgentGateway& gateway,
    const std::filesystem::path& workspace_root,
    bool auto_approve,
    SessionState& session,
    SessionLogger* logger) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }

    if (!run->commit_proposal()) {
        session.set_commit_proposal(
            gateway.request_commit_message(run->run_id(), run->task(), chat_detail::diff_summary_from_run(*run)));
        run = session.active_run();
    }

    const auto reviewed_proposal = review_commit_proposal(*run->commit_proposal(), auto_approve);
    session.set_commit_proposal(reviewed_proposal);
    log_event(logger, "task.commit_proposal", Json(reviewed_proposal));
    run = session.active_run();

    const bool approved = execution::ask_for_approval(chat_detail::kCommitApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kCommitApprovalLabel, approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kCommitApprovalLabel}, {"approved", approved}});
    if (!approved) {
        return;
    }

    execution::WorkspaceExecutor workspace_executor(workspace_root);
    workspace_executor.git_commit(*run->commit_proposal());
    session.record_action("Created git commit");
    log_event(
        logger,
        "task.commit_created",
        Json{{"title", run->commit_proposal()->title}, {"head", workspace_executor.head_commit()}});
    std::cout << "Commit created.\n";
}

void push_active_run(const std::filesystem::path& workspace_root, bool auto_approve, SessionState& session, SessionLogger* logger) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }

    const bool approved = execution::ask_for_approval(chat_detail::kPushApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kPushApprovalLabel, approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kPushApprovalLabel}, {"approved", approved}});
    if (!approved) {
        return;
    }

    execution::WorkspaceExecutor workspace_executor(workspace_root);
    workspace_executor.git_push();
    session.record_action("Pushed current branch");
    log_event(logger, "task.push", Json{{"branch", workspace_executor.current_branch()}});
    std::cout << "Branch pushed.\n";
}

void deploy_active_run(AgentGateway& gateway, bool auto_approve, SessionState& session, SessionLogger* logger) {
    const auto* run = session.active_run();
    if (run == nullptr) {
        std::cout << "No active prepared run.\n";
        return;
    }
    if (!run->repository()) {
        std::cout << "Active run does not have repository context.\n";
        return;
    }

    if (!run->deploy_request()) {
        session.set_deploy_request(gateway.request_deploy(run->run_id(), run->task(), *run->repository()));
        run = session.active_run();
    }

    const auto reviewed_request = review_deploy_request(*run->deploy_request(), auto_approve);
    session.set_deploy_request(reviewed_request);
    log_event(logger, "task.deploy_request", Json(reviewed_request));
    run = session.active_run();

    const bool approved = execution::ask_for_approval(chat_detail::kDeployApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kDeployApprovalLabel, approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kDeployApprovalLabel}, {"approved", approved}});
    if (!approved) {
        return;
    }

    const auto result = execution::DeployExecutor::execute(*run->deploy_request());
    session.record_action("Deploy to " + result.host);
    log_event(logger, "task.deploy_result", Json(result));
    std::cout << "Deploy result: " << result.health << '\n';
    for (const auto& log : result.logs) {
        const auto trimmed_log = trim(log);
        if (!trimmed_log.empty()) {
            std::cout << trimmed_log << '\n';
        }
    }
}

void run_chat_session_inner(
    AgentGateway& gateway,
    const std::filesystem::path& workspace_root,
    const TaskInput& input,
    bool auto_approve,
    SessionState& session,
    SessionLogger* logger) {
    const auto repository_started_at = std::chrono::steady_clock::now();
    const auto repository = chat_detail::load_repository_context(workspace_root, input.attached_files);
    log_timing("run.load_repository_context", std::chrono::steady_clock::now() - repository_started_at);
    const auto run_id = make_run_id();
    session.begin_run(run_id, input.task, input.attached_files);
    log_event(logger, "task.begin", Json{{"runId", run_id}, {"task", input.task}, {"attachedFiles", input.attached_files}});
    session.set_repository(repository);
    session.set_pending_approvals(chat_detail::approval_labels(input.task));
    execution::WorkspaceExecutor workspace_executor(workspace_root);

    if (chat_detail::looks_like_commit_request(input.task)) {
        const auto plan = make_commit_plan();
        session.set_plan(plan);
        chat_detail::print_plan(plan);
        session.set_terminal_reply(make_local_terminal_reply(plan));
        std::cout << *session.active_run()->terminal_reply() << '\n';
        log_event(logger, "task.plan", Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
        log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", *session.active_run()->terminal_reply()}});

        const auto generated_proposal =
            gateway.request_commit_message(run_id, input.task, gateway_detail::repository_context_prompt(repository));
        const auto reviewed_proposal = review_commit_proposal(generated_proposal, auto_approve);
        session.set_commit_proposal(reviewed_proposal);
        log_event(logger, "task.commit_proposal", Json(reviewed_proposal));

        const bool commit_approved = execution::ask_for_approval(chat_detail::kCommitApprovalLabel, auto_approve);
        session.record_approval(chat_detail::kCommitApprovalLabel, commit_approved);
        log_event(logger, "task.approval", Json{{"label", chat_detail::kCommitApprovalLabel}, {"approved", commit_approved}});
        if (commit_approved) {
            workspace_executor.git_commit(reviewed_proposal);
            session.record_action("Created git commit");
            log_event(logger, "task.commit_created", Json{{"title", reviewed_proposal.title}, {"head", workspace_executor.head_commit()}});
        }

        session.complete_run();
        log_event(logger, "task.completed", Json{{"runId", run_id}});
        return;
    }

    if (chat_detail::looks_like_push_request(input.task)) {
        const auto plan = make_push_plan(repository.branch.empty() ? "current branch" : repository.branch);
        session.set_plan(plan);
        chat_detail::print_plan(plan);
        session.set_terminal_reply(make_local_terminal_reply(plan));
        std::cout << *session.active_run()->terminal_reply() << '\n';
        log_event(logger, "task.plan", Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
        log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", *session.active_run()->terminal_reply()}});

        const bool push_approved = execution::ask_for_approval(chat_detail::kPushApprovalLabel, auto_approve);
        session.record_approval(chat_detail::kPushApprovalLabel, push_approved);
        log_event(logger, "task.approval", Json{{"label", chat_detail::kPushApprovalLabel}, {"approved", push_approved}});
        if (push_approved) {
            workspace_executor.git_push();
            session.record_action("Pushed current branch");
            log_event(logger, "task.push", Json{{"branch", workspace_executor.current_branch()}});
        }

        session.complete_run();
        log_event(logger, "task.completed", Json{{"runId", run_id}});
        return;
    }

    const auto plan_started_at = std::chrono::steady_clock::now();
    const auto plan = gateway.create_task_plan(run_id, input.task, repository);
    log_timing("run.create_task_plan", std::chrono::steady_clock::now() - plan_started_at);
    session.set_plan(plan);
    chat_detail::print_plan(plan);
    log_event(logger, "task.plan", Json{{"runId", run_id}, {"summary", plan.summary}, {"stepCount", plan.steps.size()}, {"steps", Json(plan.steps)}});
    const auto local_reply = make_local_terminal_reply(plan);
    session.set_terminal_reply(local_reply);
    std::cout << local_reply << '\n';
    log_event(logger, "task.reply", Json{{"runId", run_id}, {"message", local_reply}});

    const auto edits_started_at = std::chrono::steady_clock::now();
    const auto edits = gateway.request_edits(run_id, input.task, repository);
    log_timing("run.request_edits", std::chrono::steady_clock::now() - edits_started_at);
    session.set_proposed_edits(edits);
    log_event(logger, "task.edits_prepared", Json{{"runId", run_id}, {"count", edits.size()}, {"edits", Json(edits)}});

    const bool apply_approved = execution::ask_for_approval(chat_detail::kApplyProposedApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kApplyProposedApprovalLabel, apply_approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kApplyProposedApprovalLabel}, {"approved", apply_approved}});
    if (apply_approved) {
        const auto selected_indexes = prompt_selected_edit_indexes(edits, auto_approve);
        std::vector<EditProposal> applied_edits;
        std::vector<EditProposal> skipped_edits;
        std::unordered_set<std::size_t> selected_lookup(selected_indexes.begin(), selected_indexes.end());
        for (std::size_t index = 0; index < edits.size(); ++index) {
            const auto& edit = edits[index];
            if (selected_lookup.contains(index)) {
                applied_edits.push_back(edit);
            } else {
                skipped_edits.push_back(edit);
            }
        }

        session.set_edit_review_result(applied_edits, skipped_edits);
        if (!applied_edits.empty()) {
            workspace_executor.apply_edits(applied_edits);
            session.record_action("Applied " + std::to_string(applied_edits.size()) + " selected file change(s)");
            log_event(logger, "task.edit_review", Json{{"applied", Json(applied_edits)}, {"skipped", Json(skipped_edits)}});
        } else {
            session.record_action("Skipped all proposed file changes");
            log_event(logger, "task.edit_review", Json{{"applied", Json::array()}, {"skipped", Json(edits)}});
        }
    }

    const bool checks_approved = execution::ask_for_approval(chat_detail::kChecksApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kChecksApprovalLabel, checks_approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kChecksApprovalLabel}, {"approved", checks_approved}});
    if (checks_approved) {
        workspace_executor.run_checks();
        session.record_action("Run local checks");
        log_event(logger, "task.checks", Json{{"result", "passed"}});
    }

    const auto generated_proposal =
        gateway.request_commit_message(run_id, input.task, chat_detail::diff_summary_from_run(*session.active_run()));
    const auto reviewed_proposal = review_commit_proposal(generated_proposal, auto_approve);
    session.set_commit_proposal(reviewed_proposal);
    log_event(logger, "task.commit_proposal", Json(reviewed_proposal));

    const bool commit_approved = execution::ask_for_approval(chat_detail::kCommitApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kCommitApprovalLabel, commit_approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kCommitApprovalLabel}, {"approved", commit_approved}});
    if (commit_approved) {
        workspace_executor.git_commit(reviewed_proposal);
        session.record_action("Created git commit");
        log_event(logger, "task.commit_created", Json{{"title", reviewed_proposal.title}, {"head", workspace_executor.head_commit()}});
    }

    const bool push_approved = execution::ask_for_approval(chat_detail::kPushApprovalLabel, auto_approve);
    session.record_approval(chat_detail::kPushApprovalLabel, push_approved);
    log_event(logger, "task.approval", Json{{"label", chat_detail::kPushApprovalLabel}, {"approved", push_approved}});
    if (push_approved) {
        workspace_executor.git_push();
        session.record_action("Pushed current branch");
        log_event(logger, "task.push", Json{{"branch", workspace_executor.current_branch()}});
    }

    if (chat_detail::looks_like_deploy_instruction(input.task)) {
        const auto generated_request = gateway.request_deploy(run_id, input.task, repository);
        const auto reviewed_request = review_deploy_request(generated_request, auto_approve);
        session.set_deploy_request(reviewed_request);
        log_event(logger, "task.deploy_request", Json(reviewed_request));
        const bool deploy_approved = execution::ask_for_approval(chat_detail::kDeployApprovalLabel, auto_approve);
        session.record_approval(chat_detail::kDeployApprovalLabel, deploy_approved);
        log_event(logger, "task.approval", Json{{"label", chat_detail::kDeployApprovalLabel}, {"approved", deploy_approved}});
        if (deploy_approved) {
            const auto result = execution::DeployExecutor::execute(reviewed_request);
            session.record_action("Deploy to " + result.host);
            log_event(logger, "task.deploy_result", Json(result));
        }
    }

    session.complete_run();
    log_event(logger, "task.completed", Json{{"runId", run_id}});
}

}  // namespace

void run_one_shot_input(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const TaskInput& input,
    bool auto_approve,
    SessionLogger* logger) {
    if (chat_detail::infer_user_intent(input.task) == chat_detail::UserIntent::AgentTask ||
        chat_detail::looks_like_uncommitted_changes_request(input.task)) {
        run_chat_session(gateway, workspace_root, input, auto_approve, logger);
        return;
    }

    run_terminal_chat_reply(gateway, std::filesystem::path(workspace_root), input.attached_files, input.task, logger);
}

void run_chat_session(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const TaskInput& input,
    bool auto_approve,
    SessionLogger* logger,
    SessionState* session_ptr) {
    SessionState local_session;
    auto& session = session_ptr != nullptr ? *session_ptr : local_session;

    try {
        if (chat_detail::looks_like_uncommitted_changes_request(input.task)) {
            run_local_uncommitted_changes_task(std::filesystem::path(workspace_root), input.task, session, logger);
            return;
        }

        run_chat_session_inner(gateway, std::filesystem::path(workspace_root), input, auto_approve, session, logger);
    } catch (const std::exception& error) {
        if (session.active_run() != nullptr) {
            session.set_runtime_error(error.what());
            log_event(
                logger,
                "task.error",
                Json{{"runId", session.active_run()->run_id()}, {"task", session.active_run()->task()}, {"message", error.what()}});
        } else {
            log_event(logger, "task.error", Json{{"message", error.what()}});
        }
        throw;
    }
}

void run_interactive_session(
    AgentGateway& gateway,
    const std::string& workspace_root,
    const std::vector<std::string>& attached_files,
    bool auto_approve,
    SessionLogger* logger) {
    SessionState session;
    log_event(logger, "session.start", Json{{"workspaceRoot", workspace_root}, {"attachedFiles", attached_files}});
    for (const auto& file : attached_files) {
        session.add_attached_file(file);
    }

    chat_detail::print_interactive_help();

    const auto workspace_path = std::filesystem::path(workspace_root);

    while (true) {
        std::cout << "maglev> " << std::flush;
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << '\n';
            return;
        }

        const auto trimmed = trim(input);
        if (trimmed.empty()) {
            continue;
        }

        try {
            const auto command = chat_detail::parse_slash_command(trimmed);
            if (command.type != chat_detail::SlashCommandType::Unknown || (!trimmed.empty() && trimmed.front() == '/')) {
                switch (command.type) {
                    case chat_detail::SlashCommandType::Exit:
                        return;
                    case chat_detail::SlashCommandType::Help:
                        log_event(logger, "session.command", Json{{"command", "/help"}});
                        chat_detail::print_interactive_help();
                        break;
                    case chat_detail::SlashCommandType::Files:
                        log_event(logger, "session.command", Json{{"command", "/files"}});
                        if (session.attached_files().empty()) {
                            std::cout << "No attached files.\n";
                        } else {
                            std::cout << "Attached files:\n";
                            for (const auto& file : session.attached_files()) {
                                std::cout << "- " << file << '\n';
                            }
                        }
                        break;
                    case chat_detail::SlashCommandType::ClearFiles:
                        log_event(logger, "session.command", Json{{"command", "/clear-files"}});
                        session.clear_attached_files();
                        std::cout << "Cleared attached files.\n";
                        break;
                    case chat_detail::SlashCommandType::Status:
                        log_event(logger, "session.command", Json{{"command", "/status"}});
                        chat_detail::print_session_status(session);
                        break;
                    case chat_detail::SlashCommandType::Plan:
                        log_event(logger, "session.command", Json{{"command", "/plan"}});
                        chat_detail::print_prepared_plan(session);
                        break;
                    case chat_detail::SlashCommandType::Apply:
                        log_event(logger, "session.command", Json{{"command", "/apply"}});
                        apply_active_run(workspace_path, auto_approve, session, logger);
                        break;
                    case chat_detail::SlashCommandType::Checks:
                        log_event(logger, "session.command", Json{{"command", "/checks"}});
                        run_checks_command(workspace_path, auto_approve, session, logger);
                        break;
                    case chat_detail::SlashCommandType::Commit:
                        log_event(logger, "session.command", Json{{"command", "/commit"}});
                        commit_active_run(gateway, workspace_path, auto_approve, session, logger);
                        break;
                    case chat_detail::SlashCommandType::Push:
                        log_event(logger, "session.command", Json{{"command", "/push"}});
                        push_active_run(workspace_path, auto_approve, session, logger);
                        break;
                    case chat_detail::SlashCommandType::Deploy:
                        log_event(logger, "session.command", Json{{"command", "/deploy"}});
                        deploy_active_run(gateway, auto_approve, session, logger);
                        break;
                    case chat_detail::SlashCommandType::File:
                        if (command.argument.empty()) {
                            std::cout << "Usage: /file <path>\n";
                        } else {
                            log_event(logger, "session.command", Json{{"command", "/file"}, {"argument", command.argument}});
                            session.add_attached_file(command.argument);
                            std::cout << "Attached file: " << command.argument << '\n';
                        }
                        break;
                    case chat_detail::SlashCommandType::Task:
                        if (command.argument.empty()) {
                            std::cout << "Usage: /task <text>\n";
                        } else {
                            log_event(logger, "session.command", Json{{"command", "/task"}, {"argument", command.argument}});
                            prepare_or_execute_agent_input(gateway, workspace_path, command.argument, session, logger);
                        }
                        break;
                    case chat_detail::SlashCommandType::Unknown:
                        std::cout << "Unknown command. Use /help.\n";
                        break;
                }
                continue;
            }

            if (session.active_run() != nullptr) {
                if (chat_detail::looks_like_apply_request(trimmed)) {
                    log_event(logger, "session.natural_command", Json{{"command", "apply"}, {"input", trimmed}});
                    apply_active_run(workspace_path, auto_approve, session, logger);
                    continue;
                }
                if (chat_detail::looks_like_checks_request(trimmed)) {
                    log_event(logger, "session.natural_command", Json{{"command", "checks"}, {"input", trimmed}});
                    run_checks_command(workspace_path, auto_approve, session, logger);
                    continue;
                }
                if (chat_detail::looks_like_commit_request(trimmed)) {
                    log_event(logger, "session.natural_command", Json{{"command", "commit"}, {"input", trimmed}});
                    commit_active_run(gateway, workspace_path, auto_approve, session, logger);
                    continue;
                }
                if (chat_detail::looks_like_push_request(trimmed)) {
                    log_event(logger, "session.natural_command", Json{{"command", "push"}, {"input", trimmed}});
                    push_active_run(workspace_path, auto_approve, session, logger);
                    continue;
                }
                if (chat_detail::looks_like_deploy_instruction(trimmed)) {
                    log_event(logger, "session.natural_command", Json{{"command", "deploy"}, {"input", trimmed}});
                    deploy_active_run(gateway, auto_approve, session, logger);
                    continue;
                }
            }

            if (chat_detail::infer_user_intent(trimmed) == chat_detail::UserIntent::AgentTask) {
                std::cout << "Detected agent task intent. Running agent workflow. Use /task to force this behavior explicitly.\n";
                prepare_or_execute_agent_input(gateway, workspace_path, trimmed, session, logger);
            } else {
                run_terminal_chat_reply(gateway, workspace_path, session.attached_files(), trimmed, logger);
            }
        } catch (const std::exception& error) {
            if (session.active_run() != nullptr) {
                session.set_runtime_error(error.what());
            }
            log_event(logger, "session.error", Json{{"message", error.what()}});
            std::cout << "Error: " << error.what() << '\n';
        }
    }
}

}  // namespace maglev
