#include "maglev/runtime/logging.h"

#include <stdexcept>

#include "maglev/runtime/util.h"

namespace maglev {

namespace {

std::filesystem::path resolve_log_path(const std::filesystem::path& workspace_root, const std::filesystem::path& explicit_path) {
    if (!explicit_path.empty()) {
        return explicit_path;
    }

    return workspace_root / ".maglev" / "transcripts" /
           ("session-" + std::to_string(unix_time_millis()) + "-" + random_hex(4) + ".jsonl");
}

}  // namespace

SessionLogger::SessionLogger(std::filesystem::path path) : path_(std::move(path)) {}

SessionLogger SessionLogger::create(const std::filesystem::path& workspace_root, const std::filesystem::path& explicit_path) {
    return SessionLogger(resolve_log_path(workspace_root, explicit_path));
}

const std::filesystem::path& SessionLogger::path() const {
    return path_;
}

void SessionLogger::log_event(const std::string& type, const Json& payload) const {
    Json entry = {
        {"timestampMs", unix_time_millis()},
        {"type", type},
        {"payload", payload},
    };

    std::string existing;
    if (std::filesystem::exists(path_)) {
        existing = read_text_file(path_);
        if (!existing.empty() && existing.back() != '\n') {
            existing.push_back('\n');
        }
    }
    existing += entry.dump();
    existing.push_back('\n');

    std::string error;
    if (!write_text_file(path_, existing, error)) {
        throw std::runtime_error(error);
    }
}

}  // namespace maglev
