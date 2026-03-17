# Maglev CLI Plan Completed

## Goal

Develop `maglev` into a standalone C++ CLI agent with a terminal workflow for chat, repository actions, code edits, commit, push, deploy, and user filesystem context.

## Completion Summary

This plan is completed.

Implemented result:

1. `maglev` works as a persistent interactive CLI session, not only as a single-run tool.
2. Session state tracks the active run, plan, approvals, repository context, proposed edits, applied edits, skipped edits, commit proposal, deploy proposal, and runtime errors.
3. Slash commands are implemented for the main workflow stages.
4. Selective edit review is implemented.
5. Commit review is implemented.
6. Deploy review is implemented.
7. Interactive failures no longer terminate the whole session flow.
8. Runtime/backend/model prompts and response profiles are centralized in [`model-endpoints.json`](../config/model-endpoints.json).
9. Natural-language intent routing is implemented for chat, generic agent tasks, commit, push, deploy, and repository inspection.
10. Session-scoped filesystem mounting is implemented for user-provided files and directories, with lazy content loading.

## Implemented Scope

### Phase 1: Session-Based Chat

Completed.

Implemented in:

- [`main.cpp`](../cpp/src/app/main.cpp)
- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`session.h`](../cpp/include/maglev/runtime/session.h)
- [`session.cpp`](../cpp/src/runtime/session.cpp)

Delivered behavior:

- interactive REPL loop
- one-shot and interactive entrypoints
- persistent session state across turns

### Phase 2: Structured Commands

Completed.

Implemented in:

- [`chat_commands.h`](../cpp/include/maglev/chat/chat_commands.h)
- [`chat_commands.cpp`](../cpp/src/chat/chat_commands.cpp)
- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`execution.h`](../cpp/include/maglev/runtime/execution.h)
- [`execution.cpp`](../cpp/src/runtime/execution.cpp)

Delivered behavior:

- `/task`
- `/status`
- `/plan`
- `/apply`
- `/checks`
- `/commit`
- `/push`
- `/deploy`
- `/help`
- `/exit`

### Phase 3: Edit Review

Completed.

Implemented in:

- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`chat_presenter.cpp`](../cpp/src/chat/chat_presenter.cpp)
- [`session.h`](../cpp/include/maglev/runtime/session.h)
- [`session.cpp`](../cpp/src/runtime/session.cpp)

Delivered behavior:

- selective file application
- `applied/skipped` tracking
- explicit review output in session status

### Phase 4: Commit Review

Completed.

Implemented in:

- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`execution.cpp`](../cpp/src/runtime/execution.cpp)

Delivered behavior:

- generated commit proposal
- editable title/body before approval
- commit execution with reviewed message

### Phase 5: Deploy Review

Completed.

Implemented in:

- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`execution.cpp`](../cpp/src/runtime/execution.cpp)

Delivered behavior:

- preview of deploy target
- editable deploy fields before approval
- explicit confirmation before execution

### Phase 6: Reliability and Runtime Ergonomics

Completed.

Implemented in:

- [`main.cpp`](../cpp/src/app/main.cpp)
- [`chat.cpp`](../cpp/src/chat/chat.cpp)
- [`config.h`](../cpp/include/maglev/runtime/config.h)
- [`config.cpp`](../cpp/src/runtime/config.cpp)

Delivered behavior:

- failed steps keep the session alive
- `last error` is preserved in session state
- supported flags:
  - `--task`
  - `--file`
  - `--auto-approve`
  - `--config-file` / `-c`
  - `--config` as legacy alias
  - `--backend`
  - `--model`

Note:

- transcript/audit logging was implemented earlier, but transcript file creation is currently disabled in [`main.cpp`](../cpp/src/app/main.cpp) by design

### Additional Completed Work Beyond The Original Plan

Implemented in:

- [`chat_intent.h`](../cpp/include/maglev/chat/chat_intent.h)
- [`chat_intent.cpp`](../cpp/src/chat/chat_intent.cpp)
- [`chat_repository.h`](../cpp/include/maglev/chat/chat_repository.h)
- [`chat_repository.cpp`](../cpp/src/chat/chat_repository.cpp)
- [`repository_contracts.h`](../cpp/include/maglev/contracts/repository_contracts.h)
- [`repository_contracts.cpp`](../cpp/src/contracts/repository_contracts.cpp)

Delivered behavior:

- natural-language intent detection for agent actions
- deterministic local handling for uncommitted changes
- mounted file/directory paths that live for the whole session
- directory tree passed to the model before file contents
- file contents loaded only on explicit request

## Current State

`maglev` is now a working C++ CLI with:

- chat mode
- staged agent workflow
- repository-aware operations
- config-driven backend/model setup
- Windows and Linux builds
- local filesystem context with lazy loading

## Next Steps

1. latency reduction for structured Qwen calls
2. richer filesystem policies and approvals outside repo root
3. terminal UX polish or TUI
4. release packaging and distribution
