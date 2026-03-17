# maglev

C++ CLI repository for Maglev.

This repository contains the standalone CLI agent that runs locally on the user's machine and executes filesystem, shell, git, SSH, and deploy actions from that machine.

## Structure

- `cpp/`
- `config/model-endpoints.json`
- `tools/bin/`
- `.vscode/`

## Build

Build outputs are split by target platform:

- Linux x64 debug: `target/linux-x64/debug/maglev`
- Linux x64 release: `target/linux-x64/release/maglev`
- Windows x64 debug: `target/windows-x64/debug/maglev.exe`
- Windows x64 release: `target/windows-x64/release/maglev.exe`

Build entrypoints:

- Linux x64 debug: `cmake --preset linux-x64-debug && cmake --build --preset linux-x64-debug`
- Linux x64 release: `cmake --preset linux-x64-release && cmake --build --preset linux-x64-release`
- Windows x64 debug: `cmake --preset windows-x64-debug && cmake --build --preset windows-x64-debug`
- Windows x64 release: `cmake --preset windows-x64-release && cmake --build --preset windows-x64-release`

Build toolchain:

- `CMake`
- `vcpkg`
- Linux: prefers `clang`, falls back to `gcc/g++` if `clang` is not installed
- Windows: `MSVC`

Dependency resolution:

- platform dependencies are installed automatically through `vcpkg` during configure/build
- VS Code is configured to run CMake automatically on open, so `vcpkg_installed` and `compile_commands.json` are generated before IntelliSense needs them

Linux note:

- the repo includes local wrapper tools in `tools/bin` for `zip`, `unzip`, and `pkg-config`
- they are used by the Linux build flow so the project can still build in a constrained environment where those tools are missing from the system package manager

## File Context

The CLI can attach local files to the model context.

- Non-interactive:
  - `target/linux-x64/debug/maglev --file README.md "summarize the attached file"`
- Interactive:
  - start the CLI without a task
  - enter `/file path/to/file`
  - then enter the task

Attached files are read locally and included in the repository context sent to the model.

## Interactive Session

If you start `maglev` without a task, it opens a persistent session.

Available commands:

- `/file <path>`
- `/files`
- `/clear-files`
- `/status`
- `/task <text>`
- `/plan`
- `/apply`
- `/checks`
- `/commit`
- `/push`
- `/deploy`
- `/help`
- `/exit`

Interactive behavior:

- non-command input is routed by heuristic intent detection into `chat` or `agent task`
- `/task <text>` explicitly prepares or runs an agent task
- prepared agent tasks can then be stepped through with `/plan`, `/apply`, `/checks`, `/commit`, `/push`, `/deploy`
- requests like “show uncommitted changes” are handled locally through deterministic runtime logic instead of asking the model to guess git state
- step failures keep the interactive session alive and are stored in the active run as `last error`

## CLI Flags

Supported flags:

- `--task "<text>"`
- `--file <path>` (repeatable)
- `--auto-approve`
- `--config <path>`
- `--backend <openai_compat|secure_gateway>`
- `--model <model-id>`

Examples:

- `target/linux-x64/debug/maglev --task "Кто ты?"`
- `target/linux-x64/debug/maglev --config config/model-endpoints.json --backend openai_compat --model qwen/qwen3.5-35b-a3b --task "Какая модель сейчас активна?"`
- `target/linux-x64/debug/maglev --file README.md --task "summarize the attached file"`

## Transcript Logs

Each process writes a local transcript/audit log in JSONL format.

- default directory: `.maglev/transcripts/`
- entries include process start, session commands, task plans, approvals, edit review, commit/deploy proposals, commit hash, deploy results, and recorded errors

## VS Code

Workspace tasks and launch configurations in [`.vscode`](./.vscode) are CLI-only:

- `Build: Linux x64 Debug`
- `Build: Linux x64 Release`
- `Build: Windows x64 Debug`
- `Build: Windows x64 Release`
- `Build Matrix`

Recommended debug launches:

- `Debug: Linux x64`
- `Debug: Windows x64`

Each debug launch now uses VS Code variables from `launch.json`:

- `MAGLEV_DEBUG_MODE`: `Interactive`, `Task`, or `File Task`

Runtime prompt behavior:

- `Interactive`: launches the session immediately
- `Task`: VS Code asks only for the mode, then the CLI asks for the task in the terminal
- `File Task`: VS Code asks only for the mode, then the CLI asks for the task and file path in the terminal

Build-only entries are intentionally kept out of the `Run and Debug` dropdown. Use `Tasks: Run Task` for builds and keep the debug list limited to actual run/debug profiles.

`Debug: Windows x64` is also provided in the `Run and Debug` dropdown for WSL convenience, but in a WSL window it is a Windows run entry, not a true `cppvsdbg` step debugger. True Windows-native debugging still belongs in [`launch.windows.local.json`](./.vscode/launch.windows.local.json) from a local Windows VS Code session.

Important:

- these are `inputs` in `launch.json`, not debugger-scope variables from the `Variables` panel
- VS Code will prompt for them when the debug session starts
- they do not appear in the runtime `Variables` view because that panel shows values from the active debug process, not launch-time inputs

## WSL And Windows Debugging

- `Build: Windows x64 Debug` and `Build: Windows x64 Release` work from WSL
- `Build Matrix` runs all configured builds sequentially
- the main [`launch.json`](./.vscode/launch.json) contains only Linux debugging, because `cppvsdbg` is a Windows-local debugger and is not available inside a WSL remote window
- for local Windows debugging, use [`launch.windows.local.json`](./.vscode/launch.windows.local.json) as the template for a Windows-local `launch.json`

## Portability

The repository build configuration does not hardcode machine-specific filesystem paths.

- `CMakePresets.json` resolves `vcpkg` through `VCPKG_ROOT`, `PATH`, or a sibling checkout via [`cmake/vcpkg-toolchain.cmake`](./cmake/vcpkg-toolchain.cmake)
- builds go directly through `cmake --preset` and `cmake --build --preset`
- WSL-to-Windows flows require `MAGLEV_WINDOWS_CMD` or `cmd.exe` in `PATH`

Expected environment:

- `cmake` available in `PATH`
- `VCPKG_ROOT` set, or a `vcpkg` checkout placed next to the repository, or `vcpkg` available in `PATH`
- for WSL-driven Windows builds: `MAGLEV_WINDOWS_CMD` set to a valid `cmd.exe` path, or `cmd.exe` available in `PATH`

All launch profiles use the real model settings from `config/model-endpoints.json`.

## Current Migration Status

- implementation: `C++`
- working paths: `mock`, `openai_compat`, `secure_gateway`
