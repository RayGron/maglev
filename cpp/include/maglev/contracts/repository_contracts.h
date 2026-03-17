#pragma once

#include <string>
#include <vector>

#include "maglev/contracts/json_types.h"

namespace maglev {

struct AttachedFileContext {
    std::string path;
    std::string content;
    bool truncated = false;
};

struct DirectoryTreeEntry {
    std::string path;
    std::string kind;
};

struct MountedPathContext {
    std::string path;
    bool is_directory = false;
    std::vector<DirectoryTreeEntry> tree;
    std::vector<AttachedFileContext> loaded_files;
};

struct RepositoryContext {
    std::string root_path;
    std::string branch;
    std::vector<std::string> changed_files;
    std::vector<AttachedFileContext> attached_files;
    std::vector<MountedPathContext> mounted_paths;
};

void to_json(Json& json, const AttachedFileContext& value);
void from_json(const Json& json, AttachedFileContext& value);
void to_json(Json& json, const DirectoryTreeEntry& value);
void from_json(const Json& json, DirectoryTreeEntry& value);
void to_json(Json& json, const MountedPathContext& value);
void from_json(const Json& json, MountedPathContext& value);
void to_json(Json& json, const RepositoryContext& value);
void from_json(const Json& json, RepositoryContext& value);

}  // namespace maglev
