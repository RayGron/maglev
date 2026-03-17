mod auth;
mod chat;
mod config;
mod contracts;
mod execution;
mod gateway;
mod session;

use anyhow::Result;
use chat::{run_chat_session, run_interactive_session, TaskInput};
use config::{env_flag, BackendMode, GatewayConfig};
use gateway::{AgentGateway, MockGateway, OpenAiCompatGateway, SecureGateway};
use std::io::{self, Write};

fn main() -> Result<()> {
    let input = parse_cli_input()?;

    let config = GatewayConfig::from_env()?;
    let auto_approve = env_flag("AI_CVSC_AUTO_APPROVE");
    let workspace_root = config.workspace_root.to_string_lossy().to_string();

    let gateway: Box<dyn AgentGateway> = if config.use_mock_gateway {
        Box::new(MockGateway)
    } else if config.backend_mode == BackendMode::OpenAiCompat {
        Box::new(OpenAiCompatGateway::new(config)?)
    } else {
        Box::new(SecureGateway::new(config)?)
    };

    if input.task.trim().is_empty() {
        return run_interactive_session(gateway.as_ref(), &workspace_root, input.attached_files, auto_approve);
    }

    run_chat_session(gateway.as_ref(), &workspace_root, &input, auto_approve)
}

fn parse_cli_input() -> Result<TaskInput> {
    let mut attached_files = Vec::new();
    let mut task_parts = Vec::new();
    let mut args = std::env::args().skip(1);

    while let Some(arg) = args.next() {
        if arg == "--file" {
            let path = args.next().ok_or_else(|| anyhow::anyhow!("--file requires a path argument"))?;
            attached_files.push(path);
            continue;
        }

        task_parts.push(arg);
    }

    if !task_parts.is_empty() || !attached_files.is_empty() {
        return Ok(TaskInput {
            task: task_parts.join(" "),
            attached_files,
        });
    }

    if let Some(debug_input) = parse_debug_launch_input()? {
        return Ok(debug_input);
    }

    Ok(TaskInput {
        task: String::new(),
        attached_files: Vec::new(),
    })
}

fn parse_debug_launch_input() -> Result<Option<TaskInput>> {
    let Ok(mode) = std::env::var("MAGLEV_DEBUG_MODE") else {
        return Ok(None);
    };

    let normalized_mode = normalize_debug_mode(&mode);
    match normalized_mode.as_str() {
        "" | "interactive" => Ok(Some(TaskInput {
            task: String::new(),
            attached_files: Vec::new(),
        })),
        "task" => Ok(Some(TaskInput {
            task: required_debug_value("MAGLEV_DEBUG_TASK", "Task", "Enter task")?,
            attached_files: Vec::new(),
        })),
        "file_task" => Ok(Some(TaskInput {
            task: required_debug_value("MAGLEV_DEBUG_TASK", "File Task", "Enter task")?,
            attached_files: vec![required_debug_value(
                "MAGLEV_DEBUG_FILE",
                "File Task",
                "Enter file path",
            )?],
        })),
        _ => Err(anyhow::anyhow!(
            "unsupported MAGLEV_DEBUG_MODE `{mode}`; expected Interactive, Task, or File Task"
        )),
    }
}

fn normalize_debug_mode(mode: &str) -> String {
    mode.trim().to_lowercase().replace([' ', '-'], "_")
}

fn required_debug_value(name: &str, mode_name: &str, prompt: &str) -> Result<String> {
    let value = std::env::var(name).unwrap_or_default();
    if !value.trim().is_empty() {
        return Ok(value);
    }

    prompt_for_debug_value(name, mode_name, prompt)
}

fn prompt_for_debug_value(name: &str, mode_name: &str, prompt: &str) -> Result<String> {
    print!("{prompt}: ");
    io::stdout().flush()?;

    let mut buffer = String::new();
    io::stdin().read_line(&mut buffer)?;
    let value = buffer.trim().to_string();
    if value.is_empty() {
        return Err(anyhow::anyhow!("{name} is required when MAGLEV_DEBUG_MODE is `{mode_name}`"));
    }

    Ok(value)
}
