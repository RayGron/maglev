#include "maglev/chat/chat_repository.h"

#include "maglev/chat/chat_intent.h"
#include "maglev/runtime/execution.h"
#include "maglev/runtime/util.h"

namespace maglev::chat_detail {

namespace {

constexpr std::size_t kMaxAttachedFileChars = 24000;

}  // namespace

RepositoryContext load_repository_context(
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files) {
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
