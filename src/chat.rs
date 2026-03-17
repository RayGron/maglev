use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};
use serde_json::json;
use uuid::Uuid;

use crate::contracts::{AttachedFileContext, RepositoryContext};
use crate::execution;
use crate::gateway::AgentGateway;
use crate::session::SessionState;

const MAX_ATTACHED_FILE_CHARS: usize = 24_000;

#[derive(Debug, Clone)]
pub struct TaskInput {
    pub task: String,
    pub attached_files: Vec<String>,
}

pub fn looks_like_deploy_instruction(task: &str) -> bool {
    let lowered = task.to_lowercase();
    lowered.contains("подключись к серверу")
        || lowered.contains("deploy")
        || lowered.contains("connect to server")
}

pub fn run_chat_session(
    gateway: &dyn AgentGateway,
    workspace_root: &str,
    input: &TaskInput,
    auto_approve: bool,
) -> Result<()> {
    let repository = build_repository_context(workspace_root, &input.attached_files)?;
    let run_id = Uuid::new_v4().to_string();
    if !repository.attached_files.is_empty() {
        println!("Attached files:");
        for file in &repository.attached_files {
            let truncation_note = if file.truncated { " (truncated)" } else { "" };
            println!("- {}{}", file.path, truncation_note);
        }
    }

    let plan = gateway.create_task_plan(&run_id, &input.task, &repository)?;
    println!("Plan: {}", plan.summary);
    for step in &plan.steps {
        println!(
            "- [{}] {}",
            if step.requires_approval { "approval" } else { "auto" },
            step.title
        );
    }

    let reply = gateway.terminal_reply(&run_id, &input.task, json!({ "phase": "planning_complete" }))?;
    println!("{}", reply.message);

    let edits = gateway.request_edits(&run_id, &input.task, &repository)?;
    if execution::ask_for_approval("Apply proposed file changes?", auto_approve)? {
        execution::apply_edits(workspace_root, &edits)?;
    }

    if execution::ask_for_approval("Run local checks?", auto_approve)? {
        execution::run_checks(workspace_root)?;
    }

    if execution::ask_for_approval("Create commit?", auto_approve)? {
        let proposal = gateway.request_commit_message(&run_id, &input.task, &diff_summary(workspace_root)?)?;
        execution::git_commit(workspace_root, &proposal)?;
    }

    if execution::ask_for_approval("Push branch?", auto_approve)? {
        execution::git_push(workspace_root)?;
    }

    if looks_like_deploy_instruction(&input.task) {
        let deploy_request = gateway.request_deploy(&run_id, &input.task, &repository)?;
        println!("{}", execution::preview_deploy(&deploy_request));
        if execution::ask_for_approval("Deploy to target host?", auto_approve)? {
            let deploy_result = execution::deploy(&deploy_request)?;
            println!("Deploy status: {} -> {}", deploy_result.host, deploy_result.health);
        }
    }

    Ok(())
}

pub fn run_interactive_session(
    gateway: &dyn AgentGateway,
    workspace_root: &str,
    initial_attached_files: Vec<String>,
    auto_approve: bool,
) -> Result<()> {
    let mut session = SessionState::new(initial_attached_files);
    print_interactive_help();

    loop {
        print_prompt(session.attached_files().len())?;
        let line = read_input_line()?;
        let trimmed = line.trim();

        if trimmed.is_empty() {
            continue;
        }

        if matches!(trimmed, "/exit" | "/quit") {
            println!("Exiting session.");
            break;
        }

        if trimmed == "/help" {
            print_interactive_help();
            continue;
        }

        if trimmed == "/files" {
            print_attached_files(session.attached_files());
            continue;
        }

        if trimmed == "/clear-files" {
            session.clear_attached_files();
            println!("Cleared attached files.");
            continue;
        }

        if trimmed == "/status" {
            print_session_status(&session);
            continue;
        }

        if let Some(path) = trimmed.strip_prefix("/file ").map(str::trim) {
            if path.is_empty() {
                println!("No file path provided.");
                continue;
            }

            session.add_attached_file(path.to_string());
            println!("Attached file: {path}");
            continue;
        }

        let task = trimmed.strip_prefix("/task ").map(str::trim).unwrap_or(trimmed);
        if task.is_empty() {
            println!("No task provided.");
            continue;
        }

        let input = TaskInput {
            task: task.to_string(),
            attached_files: session.attached_files().to_vec(),
        };

        match run_chat_session(gateway, workspace_root, &input, auto_approve) {
            Ok(()) => session.mark_completed_run(task),
            Err(error) => println!("Run failed: {error}"),
        }
    }

    Ok(())
}

fn current_branch(workspace_root: &str) -> Result<String> {
    let output = Command::new("git")
        .args(["branch", "--show-current"])
        .current_dir(workspace_root)
        .output()
        .context("failed to resolve current branch")?;

    if !output.status.success() {
        return Ok("unknown".into());
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn changed_files(workspace_root: &str) -> Result<Vec<String>> {
    let output = Command::new("git")
        .args(["status", "--short"])
        .current_dir(workspace_root)
        .output()
        .context("failed to read changed files")?;

    if !output.status.success() {
        return Ok(Vec::new());
    }

    Ok(String::from_utf8_lossy(&output.stdout)
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .map(|line| line.get(3..).unwrap_or(line).trim().to_string())
        .collect())
}

fn diff_summary(workspace_root: &str) -> Result<String> {
    let output = Command::new("git")
        .args(["diff", "--stat"])
        .current_dir(workspace_root)
        .output()
        .context("failed to read diff summary")?;

    if !output.status.success() {
        return Ok(String::new());
    }

    Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
}

fn build_repository_context(workspace_root: &str, attached_file_paths: &[String]) -> Result<RepositoryContext> {
    Ok(RepositoryContext {
        root_path: workspace_root.into(),
        branch: current_branch(workspace_root)?,
        changed_files: changed_files(workspace_root)?,
        attached_files: load_attached_files(workspace_root, attached_file_paths)?,
    })
}

fn load_attached_files(workspace_root: &str, attached_file_paths: &[String]) -> Result<Vec<AttachedFileContext>> {
    let mut attached_files = Vec::with_capacity(attached_file_paths.len());
    for raw_path in attached_file_paths {
        attached_files.push(read_attached_file(workspace_root, raw_path)?);
    }

    Ok(attached_files)
}

fn read_attached_file(workspace_root: &str, raw_path: &str) -> Result<AttachedFileContext> {
    let resolved_path = resolve_attached_file_path(workspace_root, raw_path);
    if !resolved_path.is_file() {
        bail!("attached file not found: {}", resolved_path.display());
    }

    let raw = fs::read(&resolved_path)
        .with_context(|| format!("failed to read attached file {}", resolved_path.display()))?;
    let content = String::from_utf8_lossy(&raw).to_string();
    let truncated = content.chars().count() > MAX_ATTACHED_FILE_CHARS;
    let normalized_content = if truncated {
        content.chars().take(MAX_ATTACHED_FILE_CHARS).collect()
    } else {
        content
    };

    Ok(AttachedFileContext {
        path: display_attached_file_path(workspace_root, &resolved_path),
        content: normalized_content,
        truncated,
    })
}

fn resolve_attached_file_path(workspace_root: &str, raw_path: &str) -> PathBuf {
    let candidate = Path::new(raw_path);
    if candidate.is_absolute() {
        candidate.to_path_buf()
    } else {
        Path::new(workspace_root).join(candidate)
    }
}

fn display_attached_file_path(workspace_root: &str, resolved_path: &Path) -> String {
    resolved_path
        .strip_prefix(workspace_root)
        .map(|path| path.to_string_lossy().to_string())
        .unwrap_or_else(|_| resolved_path.to_string_lossy().to_string())
}

fn print_prompt(attached_file_count: usize) -> Result<()> {
    if attached_file_count == 0 {
        print!("maglev> ");
    } else {
        print!("maglev[{attached_file_count} files]> ");
    }
    io::stdout().flush().context("failed to flush prompt")
}

fn read_input_line() -> Result<String> {
    let mut line = String::new();
    io::stdin().read_line(&mut line).context("failed to read session input")?;
    Ok(line)
}

fn print_interactive_help() {
    println!("Interactive commands:");
    println!("- /file <path>       attach a file to the model context");
    println!("- /files             list attached files");
    println!("- /clear-files       remove all attached files");
    println!("- /status            show current session status");
    println!("- /task <text>       run a task explicitly");
    println!("- /help              show this help");
    println!("- /exit              close the session");
    println!("Any non-command input is treated as a task.");
}

fn print_attached_files(attached_files: &[String]) {
    if attached_files.is_empty() {
        println!("No files attached.");
        return;
    }

    println!("Attached files:");
    for path in attached_files {
        println!("- {path}");
    }
}

fn print_session_status(session: &SessionState) {
    println!("Completed runs: {}", session.completed_runs);
    match &session.last_task {
        Some(task) => println!("Last task: {task}"),
        None => println!("Last task: <none>"),
    }
    print_attached_files(session.attached_files());
}
