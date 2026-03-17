#include "maglev/contracts/repository_contracts.h"

namespace maglev {

void to_json(Json& json, const AttachedFileContext& value) {
    json = Json{{"path", value.path}, {"content", value.content}, {"truncated", value.truncated}};
}

void from_json(const Json& json, AttachedFileContext& value) {
    value.path = json.value("path", std::string{});
    value.content = json.value("content", std::string{});
    value.truncated = json.value("truncated", false);
}

void to_json(Json& json, const DirectoryTreeEntry& value) {
    json = Json{{"path", value.path}, {"kind", value.kind}};
}

void from_json(const Json& json, DirectoryTreeEntry& value) {
    value.path = json.value("path", std::string{});
    value.kind = json.value("kind", std::string{});
}

void to_json(Json& json, const MountedPathContext& value) {
    json = Json{
        {"path", value.path},
        {"isDirectory", value.is_directory},
        {"tree", value.tree},
        {"loadedFiles", value.loaded_files},
    };
}

void from_json(const Json& json, MountedPathContext& value) {
    value.path = json.value("path", std::string{});
    value.is_directory = json.value("isDirectory", json.value("is_directory", false));
    value.tree = json.value("tree", std::vector<DirectoryTreeEntry>{});
    value.loaded_files = json.value("loadedFiles", json.value("loaded_files", std::vector<AttachedFileContext>{}));
}

void to_json(Json& json, const RepositoryContext& value) {
    json = Json{
        {"rootPath", value.root_path},
        {"branch", value.branch},
        {"changedFiles", value.changed_files},
        {"attachedFiles", value.attached_files},
        {"mountedPaths", value.mounted_paths},
    };
}

void from_json(const Json& json, RepositoryContext& value) {
    value.root_path = json.value("rootPath", json.value("root_path", std::string{}));
    value.branch = json.value("branch", std::string{});
    value.changed_files = json.value("changedFiles", json.value("changed_files", std::vector<std::string>{}));
    value.attached_files = json.value("attachedFiles", json.value("attached_files", std::vector<AttachedFileContext>{}));
    value.mounted_paths = json.value("mountedPaths", json.value("mounted_paths", std::vector<MountedPathContext>{}));
}

}  // namespace maglev
