#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>

#include "maglev/chat/chat.h"
#include "maglev/runtime/config.h"
#include "maglev/gateway/gateway.h"
#include "maglev/runtime/logging.h"
#include "maglev/runtime/util.h"

namespace {

struct CliArguments {
    maglev::TaskInput input;
    bool auto_approve = false;
    std::optional<std::filesystem::path> runtime_config_path;
    std::optional<std::string> backend_mode;
    std::optional<std::string> model;
};

std::filesystem::path resolve_executable_path(const char* argv0) {
    if (argv0 == nullptr || std::string(argv0).empty()) {
        return std::filesystem::current_path();
    }

    auto executable_path = std::filesystem::path(argv0);
    if (executable_path.is_absolute()) {
        return executable_path.lexically_normal();
    }
    if (executable_path.has_parent_path()) {
        return std::filesystem::absolute(executable_path).lexically_normal();
    }
    if (const auto from_path = maglev::find_program_on_path(executable_path.string())) {
        return from_path->lexically_normal();
    }
    return std::filesystem::absolute(executable_path).lexically_normal();
}

std::optional<std::filesystem::path> resolve_runtime_config_path(
    const CliArguments& arguments,
    [[maybe_unused]] const std::filesystem::path& executable_path) {
    if (arguments.runtime_config_path) {
        return arguments.runtime_config_path;
    }

#ifdef _WIN32
    const auto executable_directory_config = executable_path.parent_path() / "model-endpoints.json";
    if (std::filesystem::exists(executable_directory_config)) {
        return executable_directory_config;
    }
#else
    const auto xdg_config_home = maglev::getenv_or_empty("XDG_CONFIG_HOME");
    const auto linux_config = !maglev::trim(xdg_config_home).empty()
                                  ? std::filesystem::path(xdg_config_home) / "maglev" / "model-endpoints.json"
                                  : std::filesystem::path(maglev::getenv_or_empty("HOME")) / ".config" / "maglev" /
                                        "model-endpoints.json";
    if (std::filesystem::exists(linux_config)) {
        return linux_config;
    }
#endif

    const auto workspace_config = std::filesystem::current_path() / "config" / "model-endpoints.json";
    if (std::filesystem::exists(workspace_config)) {
        return workspace_config;
    }

    return std::nullopt;
}

std::string normalize_debug_mode(std::string mode) {
    for (char& ch : mode) {
        if (ch == ' ' || ch == '-') {
            ch = '_';
        }
    }
    return maglev::to_lower(maglev::trim(mode));
}

std::string prompt_required_value(const std::string& name, const std::string& mode_name, const std::string& prompt) {
    const auto env_value = maglev::getenv_or_empty(name);
    if (!maglev::trim(env_value).empty()) {
        return env_value;
    }

    std::cout << prompt << ": " << std::flush;
    std::string value;
    std::getline(std::cin, value);
    value = maglev::trim(value);
    if (value.empty()) {
        throw std::runtime_error(name + " is required when MAGLEV_DEBUG_MODE is `" + mode_name + "`");
    }
    return value;
}

std::optional<maglev::TaskInput> parse_debug_launch_input() {
    const auto mode = maglev::getenv_or_empty("MAGLEV_DEBUG_MODE");
    if (mode.empty()) {
        return std::nullopt;
    }

    const auto normalized = normalize_debug_mode(mode);
    if (normalized.empty() || normalized == "interactive") {
        return maglev::TaskInput{};
    }
    if (normalized == "task") {
        return maglev::TaskInput{prompt_required_value("MAGLEV_DEBUG_TASK", "Task", "Enter task"), {}};
    }
    if (normalized == "file_task") {
        return maglev::TaskInput{
            prompt_required_value("MAGLEV_DEBUG_TASK", "File Task", "Enter task"),
            {prompt_required_value("MAGLEV_DEBUG_FILE", "File Task", "Enter file path")},
        };
    }

    throw std::runtime_error("unsupported MAGLEV_DEBUG_MODE `" + mode + "`; expected Interactive, Task, or File Task");
}

CliArguments parse_cli_input(int argc, char** argv) {
    CliArguments arguments;
    std::vector<std::string> attached_files;
    std::vector<std::string> task_parts;

    for (int index = 1; index < argc; ++index) {
        std::string arg = argv[index];
        if (arg == "--file") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--file requires a path argument");
            }
            attached_files.push_back(argv[++index]);
            continue;
        }
        if (arg == "--task") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--task requires a value");
            }
            task_parts.push_back(argv[++index]);
            continue;
        }
        if (arg == "--auto-approve") {
            arguments.auto_approve = true;
            continue;
        }
        if (arg == "--config-file" || arg == "-c" || arg == "--config") {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string(arg) + " requires a path");
            }
            arguments.runtime_config_path = argv[++index];
            continue;
        }
        if (arg == "--backend") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--backend requires a value");
            }
            arguments.backend_mode = argv[++index];
            continue;
        }
        if (arg == "--model") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--model requires a value");
            }
            arguments.model = argv[++index];
            continue;
        }
        task_parts.push_back(arg);
    }

    if (!task_parts.empty() || !attached_files.empty()) {
        arguments.input = maglev::TaskInput{maglev::join_strings(task_parts, " "), attached_files};
        return arguments;
    }

    if (const auto debug_input = parse_debug_launch_input()) {
        arguments.input = *debug_input;
        return arguments;
    }

    return arguments;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = parse_cli_input(argc, argv);
        const auto executable_path = resolve_executable_path(argc > 0 ? argv[0] : nullptr);
        const auto runtime_config_path = resolve_runtime_config_path(arguments, executable_path);
        if (arguments.backend_mode) {
            maglev::set_process_env("AI_CVSC_BACKEND_MODE", *arguments.backend_mode);
        }
        if (arguments.model) {
            maglev::set_process_env("AI_CVSC_MODEL", *arguments.model);
            maglev::set_process_env("AI_CVSC_CHAT_MODEL", *arguments.model);
        }
        if (arguments.auto_approve) {
            maglev::set_process_env("AI_CVSC_AUTO_APPROVE", "1");
        }

        const auto config = maglev::GatewayConfig::from_environment(runtime_config_path);
        const auto auto_approve = maglev::env_flag("AI_CVSC_AUTO_APPROVE");
        auto gateway = maglev::make_gateway(config);
        // Transcript/audit file creation is intentionally disabled.
        // auto logger = maglev::SessionLogger::create(config.workspace_root);
        // logger.log_event(
        //     "process.start",
        //     maglev::Json{
        //         {"platform", maglev::platform_name()},
        //         {"compiler", maglev::compiler_name()},
        //         {"workspaceRoot", config.workspace_root.string()},
        //         {"autoApprove", auto_approve},
        //         {"backendMode", config.backend_mode == maglev::BackendMode::OpenAiCompat ? "openai_compat" : "secure_gateway"},
        //     });
        maglev::SessionLogger* logger = nullptr;

        if (maglev::trim(arguments.input.task).empty()) {
            maglev::run_interactive_session(*gateway, config.workspace_root.string(), arguments.input.attached_files, auto_approve, logger);
            return 0;
        }

        maglev::run_one_shot_input(*gateway, config.workspace_root.string(), arguments.input, auto_approve, logger);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "maglev failed: " << error.what() << '\n';
        return 1;
    }
}
