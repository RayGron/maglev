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

void to_json(Json& json, const RepositoryContext& value) {
    json = Json{
        {"rootPath", value.root_path},
        {"branch", value.branch},
        {"changedFiles", value.changed_files},
        {"attachedFiles", value.attached_files},
    };
}

void from_json(const Json& json, RepositoryContext& value) {
    value.root_path = json.value("rootPath", json.value("root_path", std::string{}));
    value.branch = json.value("branch", std::string{});
    value.changed_files = json.value("changedFiles", json.value("changed_files", std::vector<std::string>{}));
    value.attached_files = json.value("attachedFiles", json.value("attached_files", std::vector<AttachedFileContext>{}));
}

}  // namespace maglev
