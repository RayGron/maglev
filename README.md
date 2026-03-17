# maglev

Rust CLI repository for Maglev.

This repository contains the standalone CLI agent that runs locally on the user's machine and executes filesystem, shell, git, SSH, and deploy actions from that machine.

## Structure

- `src/`
- `config/model-endpoints.json`
- `.vscode/`

## Build

- `cargo build`
- `cargo build --release`
- `cargo check`

## VS Code

Workspace tasks and launch configurations in [.vscode](/mnt/e/dev/Repos/maglev/.vscode) are CLI-only:

- `CLI: Build Debug`
- `CLI: Build Release`
- `CLI: Check`
- `Debug Maglev CLI`
