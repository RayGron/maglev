#include "maglev/runtime/util.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace maglev {

namespace {

std::filesystem::path make_temp_path(std::string_view suffix) {
    auto base = std::filesystem::temp_directory_path();
    return base / ("maglev-" + random_hex(8) + std::string(suffix));
}

std::string quote_path_for_launcher(const std::filesystem::path& path) {
#ifdef _WIN32
    std::string value = path.string();
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
#else
    return quote_shell_argument(path.string());
#endif
}

int normalize_exit_code(int status_code) {
#ifdef _WIN32
    return status_code;
#else
    if (WIFEXITED(status_code)) {
        return WEXITSTATUS(status_code);
    }
    return status_code;
#endif
}

}  // namespace

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> split_lines(const std::string& value) {
    std::vector<std::string> lines;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string join_lines(const std::vector<std::string>& values) {
    return join_strings(values, "\n");
}

std::string join_strings(const std::vector<std::string>& values, std::string_view separator) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << separator;
        }
        stream << values[index];
    }
    return stream.str();
}

std::string quote_shell_argument(const std::string& value) {
#ifdef _WIN32
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
#else
    std::string escaped = "'";
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
#endif
}

CommandResult run_script(const std::string& script_body, const std::filesystem::path& cwd) {
    const auto script_path =
#ifdef _WIN32
        make_temp_path(".cmd");
#else
        make_temp_path(".sh");
#endif
    const auto stdout_path = make_temp_path(".stdout.log");
    const auto stderr_path = make_temp_path(".stderr.log");

    try {
        std::ofstream script(script_path);
        if (!script) {
            throw std::runtime_error("failed to create temporary script");
        }

#ifdef _WIN32
        script << "@echo off\n";
        script << "cd /D " << quote_shell_argument(cwd.string()) << "\n";
        script << script_body << "\n";
#else
        script << "#!/usr/bin/env bash\n";
        script << "set -e\n";
        script << "cd " << quote_shell_argument(cwd.string()) << "\n";
        script << script_body << "\n";
#endif
        script.close();

        std::string launcher_command;
#ifdef _WIN32
        launcher_command = "cmd.exe /D /C " + quote_path_for_launcher(script_path) + " > " +
                           quote_path_for_launcher(stdout_path) + " 2> " + quote_path_for_launcher(stderr_path);
#else
        launcher_command = "bash " + quote_path_for_launcher(script_path) + " > " + quote_path_for_launcher(stdout_path) +
                           " 2> " + quote_path_for_launcher(stderr_path);
#endif

        const int status_code = std::system(launcher_command.c_str());

        CommandResult result;
        result.exit_code = normalize_exit_code(status_code);
        if (std::filesystem::exists(stdout_path)) {
            result.stdout_text = read_text_file(stdout_path);
        }
        if (std::filesystem::exists(stderr_path)) {
            result.stderr_text = read_text_file(stderr_path);
        }

        std::error_code ignored;
        std::filesystem::remove(script_path, ignored);
        std::filesystem::remove(stdout_path, ignored);
        std::filesystem::remove(stderr_path, ignored);
        return result;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(script_path, ignored);
        std::filesystem::remove(stdout_path, ignored);
        std::filesystem::remove(stderr_path, ignored);
        throw;
    }
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& content, std::string& error) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "failed to create directories for " + path.string() + ": " + ec.message();
            return false;
        }
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = "failed to open " + path.string();
        return false;
    }
    output << content;
    if (!output.good()) {
        error = "failed to write " + path.string();
        return false;
    }
    return true;
}

std::string random_hex(std::size_t bytes) {
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<int> distribution(0, 255);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes; ++index) {
        stream << std::setw(2) << distribution(generator);
    }
    return stream.str();
}

std::string make_run_id() {
    return random_hex(16);
}

std::uint64_t unix_time_millis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string platform_name() {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

std::string compiler_name() {
#if defined(_MSC_VER)
    return "msvc";
#elif defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

std::optional<std::filesystem::path> find_program_on_path(const std::string& name) {
    const char* raw_path_value = std::getenv("PATH");
    const auto raw_path = raw_path_value == nullptr ? std::string() : std::string(raw_path_value);
    if (raw_path.empty()) {
        return std::nullopt;
    }

#ifdef _WIN32
    constexpr char separator = ';';
    std::vector<std::string> candidates = {name};
    if (std::filesystem::path(name).extension().empty()) {
        candidates.push_back(name + ".exe");
        candidates.push_back(name + ".cmd");
        candidates.push_back(name + ".bat");
    }
#else
    constexpr char separator = ':';
    std::vector<std::string> candidates = {name};
#endif

    std::stringstream stream(raw_path);
    std::string entry;
    while (std::getline(stream, entry, separator)) {
        const auto directory = trim(entry);
        if (directory.empty()) {
            continue;
        }

        for (const auto& candidate_name : candidates) {
            const auto candidate = std::filesystem::path(directory) / candidate_name;
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
    }

    return std::nullopt;
}

void set_process_env(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

}  // namespace maglev
