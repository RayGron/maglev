# maglev

`maglev` is a local C++ coding CLI that runs on the user's machine and can:

- chat in a terminal
- detect agent intent from natural language
- inspect the current repository
- propose and apply file edits
- run checks
- create commits
- push the current branch
- prepare and execute deploy steps
- mount user-provided files and directories into the session context

## Usage

Run an interactive session:

```bash
target/linux-x64/debug/maglev
```

Run a one-shot task:

```bash
target/linux-x64/debug/maglev --task "Кто ты?"
```

Attach a file explicitly:

```bash
target/linux-x64/debug/maglev --file README.md --task "summarize the attached file"
```

Use a custom runtime config:

```bash
target/linux-x64/debug/maglev --config-file config/model-endpoints.json --task "Какая модель сейчас активна?"
```

Natural-language interactive flow:

- ordinary chat messages stay in chat mode
- repository/code actions are routed into agent mode when intent is detected
- inside an active run, natural phrases like `Применяй изменения`, `Сделай коммит`, `Запушь текущую ветку` trigger the corresponding stage

Path and filesystem context:

- `/file <path>` attaches a file immediately and sends its content to the model
- if you mention a file or directory path in natural language, `maglev` mounts that path for the whole session
- mounting a directory reads only its tree and metadata first
- file contents are loaded only when you explicitly ask to read, summarize, or analyze them

Interactive commands:

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

CLI flags:

- `--task "<text>"`
- `--file <path>` repeatable
- `--auto-approve`
- `--config-file <path>`
- `-c <path>`
- `--config <path>` legacy alias
- `--backend <openai_compat|secure_gateway>`
- `--model <model-id>`

Runtime config lookup order:

1. `--config-file <path>` or `-c <path>`
2. Windows: `model-endpoints.json` next to the executable
3. Linux: `$XDG_CONFIG_HOME/maglev/model-endpoints.json` or `~/.config/maglev/model-endpoints.json`
4. development fallback: `./config/model-endpoints.json`

## Build

Build outputs:

- Linux x64 debug: `target/linux-x64/debug/maglev`
- Linux x64 release: `target/linux-x64/release/maglev`
- Windows x64 debug: `target/windows-x64/debug/maglev.exe`
- Windows x64 release: `target/windows-x64/release/maglev.exe`

Build commands:

- Linux x64 debug:
  ```bash
  cmake --preset linux-x64-debug && cmake --build --preset linux-x64-debug
  ```
- Linux x64 release:
  ```bash
  cmake --preset linux-x64-release && cmake --build --preset linux-x64-release
  ```
- Windows x64 debug:
  ```bat
  cmake --preset windows-x64-debug && cmake --build --preset windows-x64-debug
  ```
- Windows x64 release:
  ```bat
  cmake --preset windows-x64-release && cmake --build --preset windows-x64-release
  ```

Toolchain:

- `CMake`
- `vcpkg`
- Linux: `clang` preferred, `gcc/g++` fallback
- Windows: `MSVC`

Dependencies are installed automatically through `vcpkg` during configure/build.

## VS Code

Workspace tasks:

- `Build: Linux x64 Debug`
- `Build: Linux x64 Release`
- `Build: Windows x64 Debug`
- `Build: Windows x64 Release`
- `Build Matrix`

Run and Debug entries:

- `Debug: Linux x64`
- `Debug: Windows x64`

Notes:

- in a WSL window, `Debug: Windows x64` is a Windows run entry, not a true native `cppvsdbg` session
- for true Windows-native debugging, use [launch.windows.local.json](./.vscode/launch.windows.local.json) from a local Windows VS Code session
- CMake is configured to run automatically on open so IntelliSense can pick up generated build metadata

## Configuration

Runtime model/backend settings live in [config/model-endpoints.json](./config/model-endpoints.json).

That file controls:

- backend mode
- API endpoints
- model IDs
- timeouts
- chat and structured response profiles
- system prompts for chat, planning, edits, commit, deploy, status, and repair

## Portability

The build setup does not hardcode machine-specific project paths.

- `CMakePresets.json` resolves `vcpkg` through `VCPKG_ROOT`, `PATH`, or a sibling checkout via [cmake/vcpkg-toolchain.cmake](./cmake/vcpkg-toolchain.cmake)
- builds go through `cmake --preset` and `cmake --build --preset`

Expected environment:

- `cmake` in `PATH`
- `vcpkg` available through `VCPKG_ROOT`, `PATH`, or a sibling checkout

## Status

Current implementation:

- language: `C++`
- working backends: `mock`, `openai_compat`, `secure_gateway`
