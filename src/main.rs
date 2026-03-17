mod auth;
mod chat;
mod config;
mod contracts;
mod execution;
mod gateway;

use anyhow::Result;
use chat::{prompt_for_task, run_chat_session};
use config::{env_flag, BackendMode, GatewayConfig};
use gateway::{AgentGateway, MockGateway, OpenAiCompatGateway, SecureGateway};

fn main() -> Result<()> {
    let task = std::env::args().skip(1).collect::<Vec<String>>().join(" ");
    let task = if task.trim().is_empty() {
        prompt_for_task()?
    } else {
        task
    };

    if task.trim().is_empty() {
        eprintln!("No task provided.");
        std::process::exit(1);
    }

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

    run_chat_session(gateway.as_ref(), &workspace_root, &task, auto_approve)
}
