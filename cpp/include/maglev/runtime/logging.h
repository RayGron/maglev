#pragma once

#include <filesystem>
#include <string>

#include "maglev/contracts/json_types.h"

namespace maglev {

class SessionLogger {
  public:
    static SessionLogger create(
        const std::filesystem::path& workspace_root,
        const std::filesystem::path& explicit_path = {});

    const std::filesystem::path& path() const;
    void log_event(const std::string& type, const Json& payload) const;

  private:
    explicit SessionLogger(std::filesystem::path path);

    std::filesystem::path path_;
};

}  // namespace maglev
