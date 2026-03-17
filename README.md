# maglev

Rust CLI repository for Maglev.

This repository contains the standalone CLI agent that runs locally on the user's machine and executes filesystem, shell, git, SSH, and deploy actions from that machine.

## Structure

- `src/`
- `config/model-endpoints.json`
- `scripts/`
- `.vscode/`

## Build

Build outputs are split by target platform:

- Linux x64 debug: `target/linux-x64/debug/maglev`
- Linux x64 release: `target/linux-x64/release/maglev`
- Windows x64 debug: `target/windows-x64/debug/maglev.exe`
- Windows x64 release: `target/windows-x64/release/maglev.exe`

Build entrypoints:

- Linux x64 debug: `./scripts/build-linux.sh debug`
- Linux x64 release: `./scripts/build-linux.sh release`
- Windows x64 debug: `cmd.exe /C scripts\\build-windows.cmd debug`
- Windows x64 release: `cmd.exe /C scripts\\build-windows.cmd release`

## File Context

The CLI can attach local files to the model context.

- Non-interactive:
  - `cargo run -- --file README.md "summarize the attached file"`
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
- `/help`
- `/exit`

Any non-command input is treated as a task and executed with the currently attached files.

## VS Code

Workspace tasks and launch configurations in [.vscode](/mnt/e/dev/Repos/maglev/.vscode) are CLI-only:

- `Build: Linux x64 Debug`
- `Build: Linux x64 Release`
- `Build: Windows x64 Debug`
- `Build: Windows x64 Release`
- `Linux x64 Debug`
- `Linux x64 Release`
- `Linux x64 Build Matrix`
- `Windows x64 Debug`
- `Windows x64 Release`
- `Windows x64 Build Matrix`
- `Build Matrix`

Recommended debug launches:

- `Build: Linux x64 Debug`
- `Build: Linux x64 Release`
- `Build: Windows x64 Debug`
- `Build: Windows x64 Release`
- `Debug: Windows x64`
- `Build Matrix`
- `Debug: Linux x64`

Each debug launch now uses VS Code variables from `launch.json`:

- `MAGLEV_DEBUG_MODE`: `Interactive`, `Task`, or `File Task`

Runtime prompt behavior:

- `Interactive`: launches the session immediately
- `Task`: VS Code asks only for the mode, then the CLI asks for the task in the terminal
- `File Task`: VS Code asks only for the mode, then the CLI asks for the task and file path in the terminal

One-click builds from the `Run and Debug` dropdown are provided through `node-terminal` launch entries. These are separate from task execution and are intended as the most reliable UI path in a WSL workspace.

`Debug: Windows x64` is also provided in the `Run and Debug` dropdown for WSL convenience, but in a WSL window it is a Windows run entry, not a true `cppvsdbg` step debugger. True Windows-native debugging still belongs in [launch.windows.local.json](/mnt/e/dev/Repos/maglev/.vscode/launch.windows.local.json) from a local Windows VS Code session.

Important:

- these are `inputs` in `launch.json`, not debugger-scope variables from the `Variables` panel
- VS Code will prompt for them when the debug session starts
- they do not appear in the runtime `Variables` view because that panel shows values from the active debug process, not launch-time inputs

## WSL And Windows Debugging

- `Build: Windows x64 Debug` and `Build: Windows x64 Release` work from WSL
- `Windows x64 Debug` and `Windows x64 Release` are shortcut tasks for one-click UI execution
- `Windows x64 Build Matrix` runs both Windows builds sequentially
- the main [launch.json](/mnt/e/dev/Repos/maglev/.vscode/launch.json) contains only Linux debugging, because `cppvsdbg` is a Windows-local debugger and is not available inside a WSL remote window
- for local Windows debugging, use [launch.windows.local.json](/mnt/e/dev/Repos/maglev/.vscode/launch.windows.local.json) as the template for a Windows-local `launch.json`

All launch profiles use the real model settings from `config/model-endpoints.json`.
