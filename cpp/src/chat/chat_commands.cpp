#include "maglev/chat/chat_commands.h"

#include <iostream>

#include "maglev/runtime/util.h"

namespace maglev::chat_detail {

void print_interactive_help() {
    std::cout << "Interactive commands:\n"
              << "- /file <path>       attach a file to the model context\n"
              << "- /files             list attached files\n"
              << "- /clear-files       remove all attached files\n"
              << "- /status            show current session status\n"
              << "- /task <text>       run a task explicitly\n"
              << "- /plan              show the prepared plan\n"
              << "- /apply             apply prepared edits\n"
              << "- /checks            run local checks\n"
              << "- /commit            create commit from current run\n"
              << "- /push              push current branch\n"
              << "- /deploy            deploy the current run\n"
              << "- /help              show this help\n"
              << "- /exit              close the session\n"
              << "Any non-command input is treated as chat unless agent intent is detected.\n";
}

SlashCommand parse_slash_command(const std::string& input) {
    const auto trimmed = trim(input);
    if (trimmed.empty() || trimmed.front() != '/') {
        return {};
    }

    if (trimmed == "/exit" || trimmed == "/quit") {
        return {SlashCommandType::Exit, {}};
    }
    if (trimmed == "/help") {
        return {SlashCommandType::Help, {}};
    }
    if (trimmed == "/files") {
        return {SlashCommandType::Files, {}};
    }
    if (trimmed == "/clear-files") {
        return {SlashCommandType::ClearFiles, {}};
    }
    if (trimmed == "/status") {
        return {SlashCommandType::Status, {}};
    }
    if (trimmed == "/plan") {
        return {SlashCommandType::Plan, {}};
    }
    if (trimmed == "/apply") {
        return {SlashCommandType::Apply, {}};
    }
    if (trimmed == "/checks") {
        return {SlashCommandType::Checks, {}};
    }
    if (trimmed == "/commit") {
        return {SlashCommandType::Commit, {}};
    }
    if (trimmed == "/push") {
        return {SlashCommandType::Push, {}};
    }
    if (trimmed == "/deploy") {
        return {SlashCommandType::Deploy, {}};
    }
    if (trimmed.rfind("/file ", 0) == 0) {
        return {SlashCommandType::File, trim(trimmed.substr(6))};
    }
    if (trimmed.rfind("/task ", 0) == 0) {
        return {SlashCommandType::Task, trim(trimmed.substr(6))};
    }

    return {SlashCommandType::Unknown, trimmed};
}

}  // namespace maglev::chat_detail
