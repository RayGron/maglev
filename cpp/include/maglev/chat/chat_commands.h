#pragma once

#include <string>

namespace maglev::chat_detail {

enum class SlashCommandType {
    Exit,
    Help,
    Files,
    ClearFiles,
    Status,
    Plan,
    Apply,
    Checks,
    Commit,
    Push,
    Deploy,
    File,
    Task,
    Unknown,
};

struct SlashCommand {
    SlashCommandType type = SlashCommandType::Unknown;
    std::string argument;
};

void print_interactive_help();
SlashCommand parse_slash_command(const std::string& input);

}  // namespace maglev::chat_detail
