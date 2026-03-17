use std::fs;
use std::io::{self, Write};
use std::path::Path;
use std::process::Command;

use anyhow::{bail, Context, Result};

use crate::contracts::{CommitMessageProposal, DeployRequestProposal, DeployResult, EditProposal};

pub fn ask_for_approval(question: &str, auto_approve: bool) -> Result<bool> {
    if auto_approve {
        return Ok(true);
    }

    print!("{question} [y/N] ");
    io::stdout().flush().context("failed to flush stdout")?;

    let mut answer = String::new();
    io::stdin().read_line(&mut answer).context("failed to read approval input")?;
    Ok(matches!(answer.trim().to_lowercase().as_str(), "y" | "yes"))
}

pub fn apply_edits(workspace_root: &str, edits: &[EditProposal]) -> Result<()> {
    for edit in edits {
      let file_path = Path::new(workspace_root).join(&edit.path);
      fs::write(&file_path, &edit.content).with_context(|| format!("failed to write {}", file_path.display()))?;
    }

    Ok(())
}

pub fn run_checks(workspace_root: &str) -> Result<()> {
    let status = Command::new("npm")
        .args(["run", "check"])
        .current_dir(workspace_root)
        .status()
        .context("failed to spawn npm run check")?;

    if !status.success() {
        bail!("local checks failed");
    }

    Ok(())
}

pub fn git_commit(workspace_root: &str, proposal: &CommitMessageProposal) -> Result<()> {
    let add_status = Command::new("git")
        .args(["add", "."])
        .current_dir(workspace_root)
        .status()
        .context("failed to spawn git add")?;
    if !add_status.success() {
        bail!("git add failed");
    }

    let mut commit = Command::new("git");
    commit.args(["commit", "-m", &proposal.title]).current_dir(workspace_root);
    if let Some(body) = &proposal.body {
        commit.args(["-m", body]);
    }

    let commit_status = commit.status().context("failed to spawn git commit")?;
    if !commit_status.success() {
        bail!("git commit failed");
    }

    Ok(())
}

pub fn git_push(workspace_root: &str) -> Result<()> {
    let branch_output = Command::new("git")
        .args(["branch", "--show-current"])
        .current_dir(workspace_root)
        .output()
        .context("failed to read current branch")?;
    if !branch_output.status.success() {
        bail!("failed to resolve current branch");
    }

    let branch = String::from_utf8_lossy(&branch_output.stdout).trim().to_string();
    let push_status = Command::new("git")
        .args(["push", "origin", &branch])
        .current_dir(workspace_root)
        .status()
        .context("failed to spawn git push")?;

    if !push_status.success() {
        bail!("git push failed");
    }

    Ok(())
}

pub fn preview_deploy(request: &DeployRequestProposal) -> String {
    let remote_command = match &request.restart_command {
        Some(restart) => format!("cd {} && git pull origin {} && {}", request.repo_path, request.branch, restart),
        None => format!("cd {} && git pull origin {}", request.repo_path, request.branch),
    };

    format!(
        "Host: {}\nSSH command: ssh {}\nRemote command: {}",
        request.host, request.host, remote_command
    )
}

pub fn deploy(request: &DeployRequestProposal) -> Result<DeployResult> {
    let remote_command = match &request.restart_command {
        Some(restart) => format!("cd {} && git pull origin {} && {}", request.repo_path, request.branch, restart),
        None => format!("cd {} && git pull origin {}", request.repo_path, request.branch),
    };

    let output = Command::new("ssh")
        .arg(&request.host)
        .arg(&remote_command)
        .output()
        .context("failed to spawn ssh")?;

    Ok(DeployResult {
        success: output.status.success(),
        host: request.host.clone(),
        branch: request.branch.clone(),
        health: if output.status.success() { "ok".into() } else { "failed".into() },
        logs: vec![
            String::from_utf8_lossy(&output.stdout).to_string(),
            String::from_utf8_lossy(&output.stderr).to_string(),
        ],
    })
}
