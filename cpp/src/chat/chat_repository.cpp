#include "maglev/chat/chat_repository.h"

#include <algorithm>
#include <regex>

#include "maglev/chat/chat_intent.h"
#include "maglev/runtime/execution.h"
#include "maglev/runtime/util.h"

namespace maglev::chat_detail {

namespace {

constexpr std::size_t kMaxAttachedFileChars = 24000;
constexpr std::size_t kMaxMountedFileChars = 24000;
constexpr std::size_t kMaxMountedTotalChars = 48000;
constexpr std::size_t kMaxMountedFiles = 32;

std::string trim_path_token(std::string value) {
    while (!value.empty()) {
        const char tail = value.back();
        if (tail == '.' || tail == ',' || tail == ';' || tail == ':' || tail == '!' || tail == '?' || tail == ')' ||
            tail == ']' || tail == '}' || tail == '"' || tail == '\'') {
            value.pop_back();
            continue;
        }
        break;
    }
    return value;
}

bool is_probably_text_file(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return true;
    }

    static const std::vector<std::string> text_extensions = {
        ".txt",  ".md",   ".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf", ".cpp", ".cc", ".c",
        ".h",    ".hpp",  ".hh",   ".py",   ".rs",  ".go",   ".js",  ".ts",  ".tsx",  ".java", ".cs", ".html",
        ".css",  ".xml",  ".sh",   ".sql",  ".csv",
    };
    const auto extension = to_lower(path.extension().string());
    return std::find(text_extensions.begin(), text_extensions.end(), extension) != text_extensions.end();
}

std::filesystem::path resolve_user_path(const std::filesystem::path& workspace_root, const std::string& raw_path) {
    auto path = std::filesystem::path(trim(raw_path));
    if (!path.is_absolute()) {
        path = workspace_root / path;
    }
    return path.lexically_normal();
}

std::vector<DirectoryTreeEntry> build_directory_tree(const std::filesystem::path& root) {
    std::vector<DirectoryTreeEntry> tree;
    if (std::filesystem::is_regular_file(root)) {
        tree.push_back(DirectoryTreeEntry{root.filename().string(), "file"});
        return tree;
    }

    tree.push_back(DirectoryTreeEntry{".", "directory"});
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto relative = std::filesystem::relative(entry.path(), root).generic_string();
        tree.push_back(DirectoryTreeEntry{relative, entry.is_directory() ? "directory" : "file"});
    }
    return tree;
}

}  // namespace

RepositoryContext load_repository_context(
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files,
    const std::vector<MountedPathContext>& mounted_paths) {
    RepositoryContext repository;
    repository.root_path = workspace_root.string();
    execution::WorkspaceExecutor workspace_executor(workspace_root);

    try {
        repository.branch = workspace_executor.current_branch();
    } catch (...) {
        repository.branch = "unknown";
    }

    try {
        repository.changed_files = workspace_executor.changed_files();
    } catch (...) {
        repository.changed_files.clear();
    }

    repository.attached_files = load_attached_file_context(workspace_root, attached_files);
    repository.mounted_paths = mounted_paths;

    return repository;
}

std::vector<AttachedFileContext> load_attached_file_context(
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files) {
    std::vector<AttachedFileContext> file_contexts;
    for (const auto& raw_path : attached_files) {
        const auto path = std::filesystem::path(raw_path);
        const auto resolved = path.is_absolute() ? path : workspace_root / path;
        const auto content = read_text_file(resolved);
        const bool truncated = content.size() > kMaxAttachedFileChars;
        file_contexts.push_back(
            AttachedFileContext{raw_path, truncated ? content.substr(0, kMaxAttachedFileChars) : content, truncated});
    }
    return file_contexts;
}

std::optional<std::string> extract_path_reference(const std::string& input) {
    static const std::regex path_pattern(R"(((?:[A-Za-z]:[\\/]|/)[^\s\"'<>|]+))");
    std::smatch match;
    if (!std::regex_search(input, match, path_pattern) || match.empty()) {
        return std::nullopt;
    }

    const auto candidate = trim_path_token(match.str(1));
    if (candidate.empty()) {
        return std::nullopt;
    }
    return candidate;
}

MountedPathContext mount_user_path(const std::filesystem::path& workspace_root, const std::string& raw_path) {
    const auto resolved = resolve_user_path(workspace_root, raw_path);
    if (!std::filesystem::exists(resolved)) {
        throw std::runtime_error("path does not exist: " + resolved.string());
    }

    MountedPathContext mounted_path;
    mounted_path.path = resolved.string();
    mounted_path.is_directory = std::filesystem::is_directory(resolved);
    mounted_path.tree = build_directory_tree(resolved);
    return mounted_path;
}

std::vector<AttachedFileContext> load_mounted_path_content(
    const std::filesystem::path& workspace_root,
    const MountedPathContext& mounted_path) {
    const auto resolved = resolve_user_path(workspace_root, mounted_path.path);
    if (!std::filesystem::exists(resolved)) {
        throw std::runtime_error("path does not exist: " + resolved.string());
    }

    std::vector<std::filesystem::path> files_to_read;
    if (std::filesystem::is_regular_file(resolved)) {
        files_to_read.push_back(resolved);
    } else {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(resolved)) {
            if (!entry.is_regular_file() || !is_probably_text_file(entry.path())) {
                continue;
            }
            files_to_read.push_back(entry.path());
            if (files_to_read.size() >= kMaxMountedFiles) {
                break;
            }
        }
    }

    std::vector<AttachedFileContext> loaded_files;
    std::size_t total_chars = 0;
    for (const auto& file_path : files_to_read) {
        const auto relative = std::filesystem::is_directory(resolved)
                                  ? std::filesystem::relative(file_path, resolved).generic_string()
                                  : file_path.filename().generic_string();
        auto content = read_text_file(file_path);
        bool truncated = false;

        if (content.size() > kMaxMountedFileChars) {
            content = content.substr(0, kMaxMountedFileChars);
            truncated = true;
        }

        if (total_chars >= kMaxMountedTotalChars) {
            break;
        }
        if (total_chars + content.size() > kMaxMountedTotalChars) {
            content = content.substr(0, kMaxMountedTotalChars - total_chars);
            truncated = true;
        }

        total_chars += content.size();
        loaded_files.push_back(AttachedFileContext{relative, std::move(content), truncated});
        if (total_chars >= kMaxMountedTotalChars) {
            break;
        }
    }

    return loaded_files;
}

std::string describe_mounted_path(const MountedPathContext& mounted_path) {
    std::vector<std::string> lines;
    lines.push_back(std::string("Mounted ") + (mounted_path.is_directory ? "directory" : "file") + ": " + mounted_path.path);
    if (!mounted_path.tree.empty()) {
        lines.push_back("Tree:");
        const auto preview_count = std::min<std::size_t>(mounted_path.tree.size(), 12);
        for (std::size_t index = 0; index < preview_count; ++index) {
            const auto& entry = mounted_path.tree[index];
            lines.push_back("  - [" + entry.kind + "] " + entry.path);
        }
        if (mounted_path.tree.size() > preview_count) {
            lines.push_back("  - ...");
        }
    }
    if (!mounted_path.loaded_files.empty()) {
        lines.push_back("Loaded file contents:");
        for (const auto& file : mounted_path.loaded_files) {
            lines.push_back("  - " + file.path + (file.truncated ? " (truncated)" : ""));
        }
    }
    return join_lines(lines);
}

std::vector<std::string> approval_labels(const std::string& task) {
    if (looks_like_push_request(task)) {
        return {kPushApprovalLabel};
    }
    if (looks_like_commit_request(task)) {
        return {kCommitApprovalLabel};
    }

    std::vector<std::string> labels = {
        kApplyProposedApprovalLabel,
        kChecksApprovalLabel,
        kCommitApprovalLabel,
        kPushApprovalLabel,
    };
    if (looks_like_deploy_instruction(task)) {
        labels.push_back(kDeployApprovalLabel);
    }
    return labels;
}

std::vector<std::string> prepared_approval_labels(const std::string& task) {
    auto labels = approval_labels(task);
    if (!labels.empty()) {
        labels.front() = kApplyPreparedApprovalLabel;
    }
    return labels;
}

}  // namespace maglev::chat_detail
