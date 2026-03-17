use std::env;
use std::fs;
use std::path::PathBuf;

use serde::Deserialize;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BackendMode {
    OpenAiCompat,
    SecureGateway,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
struct RuntimeConfigFile {
    default_backend_mode: Option<String>,
    openai_compat: Option<BackendDefaults>,
    secure_gateway: Option<BackendDefaults>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
struct BackendDefaults {
    api_base_url: Option<String>,
    model: Option<String>,
    request_timeout_ms: Option<u64>,
    json_response_profile: Option<JsonResponseProfile>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
struct JsonResponseProfile {
    temperature: Option<f32>,
    max_tokens: Option<Vec<u32>>,
}

#[derive(Debug, Clone)]
pub struct GatewayConfig {
    pub backend_mode: BackendMode,
    pub api_base_url: String,
    pub model: String,
    pub request_timeout_ms: u64,
    pub openai_temperature: f32,
    pub openai_json_max_tokens: Vec<u32>,
    pub private_key_path: PathBuf,
    pub public_key_path: Option<PathBuf>,
    pub use_mock_gateway: bool,
    pub workspace_root: PathBuf,
}

impl GatewayConfig {
    pub fn from_env() -> std::io::Result<Self> {
        let workspace_root = env::current_dir()?;
        let runtime_config = load_runtime_config(&workspace_root);
        let backend_mode = parse_backend_mode(
            env::var("AI_CVSC_BACKEND_MODE")
                .ok()
                .as_deref()
                .or(runtime_config.default_backend_mode.as_deref()),
        );
        let backend_defaults = runtime_config.defaults_for(backend_mode);
        let json_profile = backend_defaults.json_response_profile.clone().unwrap_or(JsonResponseProfile {
            temperature: None,
            max_tokens: None,
        });
        Ok(Self {
            backend_mode,
            api_base_url: env::var("AI_CVSC_API_BASE_URL")
                .ok()
                .or_else(|| backend_defaults.api_base_url.clone())
                .unwrap_or_else(|| "http://127.0.0.1:1234/v1".into()),
            model: env::var("AI_CVSC_MODEL")
                .ok()
                .or_else(|| backend_defaults.model.clone())
                .unwrap_or_else(|| "qwen/qwen3.5-35b-a3b".into()),
            request_timeout_ms: env::var("AI_CVSC_REQUEST_TIMEOUT_MS")
                .ok()
                .and_then(|value| value.parse::<u64>().ok())
                .or(backend_defaults.request_timeout_ms)
                .unwrap_or(30_000),
            openai_temperature: json_profile.temperature.unwrap_or(0.2),
            openai_json_max_tokens: normalize_max_tokens(json_profile.max_tokens),
            private_key_path: env::var("AI_CVSC_PRIVATE_KEY_PATH")
                .map(PathBuf::from)
                .unwrap_or_else(|_| default_private_key_path()),
            public_key_path: env::var("AI_CVSC_PUBLIC_KEY_PATH").ok().map(PathBuf::from),
            use_mock_gateway: env_flag("AI_CVSC_USE_MOCK_GATEWAY"),
            workspace_root,
        })
    }
}

pub fn env_flag(name: &str) -> bool {
    matches!(env::var(name).ok().as_deref(), Some("1") | Some("true") | Some("yes"))
}

impl RuntimeConfigFile {
    fn defaults_for(&self, backend_mode: BackendMode) -> BackendDefaults {
        match backend_mode {
            BackendMode::OpenAiCompat => self.openai_compat.clone().unwrap_or(BackendDefaults {
                api_base_url: None,
                model: None,
                request_timeout_ms: None,
                json_response_profile: None,
            }),
            BackendMode::SecureGateway => self.secure_gateway.clone().unwrap_or(BackendDefaults {
                api_base_url: None,
                model: None,
                request_timeout_ms: None,
                json_response_profile: None,
            }),
        }
    }
}

fn parse_backend_mode(value: Option<&str>) -> BackendMode {
    match value.unwrap_or("openai_compat") {
        "secure_gateway" => BackendMode::SecureGateway,
        _ => BackendMode::OpenAiCompat,
    }
}

fn load_runtime_config(workspace_root: &PathBuf) -> RuntimeConfigFile {
    let config_path = env::var("AI_CVSC_RUNTIME_CONFIG_PATH")
        .map(PathBuf::from)
        .unwrap_or_else(|_| workspace_root.join("config").join("model-endpoints.json"));
    let raw = match fs::read_to_string(config_path) {
        Ok(content) => content,
        Err(_) => {
            return RuntimeConfigFile {
                default_backend_mode: None,
                openai_compat: None,
                secure_gateway: None,
            };
        }
    };

    serde_json::from_str::<RuntimeConfigFile>(&raw).unwrap_or(RuntimeConfigFile {
        default_backend_mode: None,
        openai_compat: None,
        secure_gateway: None,
    })
}

fn normalize_max_tokens(values: Option<Vec<u32>>) -> Vec<u32> {
    let mut tokens = values.unwrap_or_else(|| vec![512, 1024]);
    tokens.retain(|value| *value > 0);
    tokens.sort_unstable();
    tokens.dedup();
    if tokens.is_empty() {
        vec![512, 1024]
    } else {
        tokens
    }
}

fn default_private_key_path() -> PathBuf {
    let home = env::var_os("HOME")
        .or_else(|| env::var_os("USERPROFILE"))
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."));
    home.join(".ai-cvsc").join("id_ed25519")
}
