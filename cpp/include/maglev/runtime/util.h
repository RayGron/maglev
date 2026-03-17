#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maglev {

struct CommandResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

std::string trim(std::string value);
std::string to_lower(std::string value);
std::vector<std::string> split_lines(const std::string& value);
std::string join_lines(const std::vector<std::string>& values);
std::string join_strings(const std::vector<std::string>& values, std::string_view separator);
std::string quote_shell_argument(const std::string& value);
CommandResult run_script(const std::string& script_body, const std::filesystem::path& cwd);
std::string read_text_file(const std::filesystem::path& path);
bool write_text_file(const std::filesystem::path& path, const std::string& content, std::string& error);
std::string random_hex(std::size_t bytes);
std::string make_run_id();
std::uint64_t unix_time_millis();
std::string platform_name();
std::string compiler_name();
std::optional<std::filesystem::path> find_program_on_path(const std::string& name);
void set_process_env(const std::string& name, const std::string& value);

}  // namespace maglev
