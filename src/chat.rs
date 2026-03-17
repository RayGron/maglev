use std::io::{self, Write};
use std::process::Command;

use anyhow::{Context, Result};
use serde_json::json;
use uuid::Uuid;

use crate::contracts::RepositoryContext;
use crate::execution;
use crate::gateway::AgentGateway;

pub fn prompt_for_task() -> Result<String> {
    print!("Task: ");
    io::stdout().flush().context("failed to flush stdout")?;

    let mut task = String::new();
    io::stdin().read_line(&mut task).context("failed to read task")?;
    Ok(task.trim().to_string())
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
    task: &str,
    auto_approve: bool,
) -> Result<()> {
    let repository = build_repository_context(workspace_root)?;
    let run_id = Uuid::new_v4().to_string();
    let plan = gateway.create_task_plan(&run_id, task, &repository)?;
    println!("Plan: {}", plan.summary);
    for step in &plan.steps {
        println!(
            "- [{}] {}",
            if step.requires_approval { "approval" } else { "auto" },
            step.title
        );
    }

    let reply = gateway.terminal_reply(&run_id, task, json!({ "phase": "planning_complete" }))?;
    println!("{}", reply.message);

    let edits = gateway.request_edits(&run_id, task, &repository)?;
    if execution::ask_for_approval("Apply proposed file changes?", auto_approve)? {
        execution::apply_edits(workspace_root, &edits)?;
    }

    if execution::ask_for_approval("Run local checks?", auto_approve)? {
        execution::run_checks(workspace_root)?;
    }

    if execution::ask_for_approval("Create commit?", auto_approve)? {
        let proposal = gateway.request_commit_message(&run_id, task, &diff_summary(workspace_root)?)?;
        execution::git_commit(workspace_root, &proposal)?;
    }

    if execution::ask_for_approval("Push branch?", auto_approve)? {
        execution::git_push(workspace_root)?;
    }

    if looks_like_deploy_instruction(task) {
        let deploy_request = gateway.request_deploy(&run_id, task, &repository)?;
        println!("{}", execution::preview_deploy(&deploy_request));
        if execution::ask_for_approval("Deploy to target host?", auto_approve)? {
            let deploy_result = execution::deploy(&deploy_request)?;
            println!("Deploy status: {} -> {}", deploy_result.host, deploy_result.health);
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

fn build_repository_context(workspace_root: &str) -> Result<RepositoryContext> {
    Ok(RepositoryContext {
        root_path: workspace_root.into(),
        branch: current_branch(workspace_root)?,
        changed_files: changed_files(workspace_root)?,
    })
}
