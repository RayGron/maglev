#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "maglev/contracts/contracts.h"

namespace maglev::chat_detail {

inline constexpr char kApplyPreparedApprovalLabel[] = "Apply prepared file changes?";
inline constexpr char kApplyProposedApprovalLabel[] = "Apply proposed file changes?";
inline constexpr char kChecksApprovalLabel[] = "Run local checks?";
inline constexpr char kCommitApprovalLabel[] = "Create commit?";
inline constexpr char kPushApprovalLabel[] = "Push branch?";
inline constexpr char kDeployApprovalLabel[] = "Deploy to target host?";

RepositoryContext load_repository_context(
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files,
    const std::vector<MountedPathContext>& mounted_paths = {});
std::vector<AttachedFileContext> load_attached_file_context(
    const std::filesystem::path& workspace_root,
    const std::vector<std::string>& attached_files);
std::optional<std::string> extract_path_reference(const std::string& input);
MountedPathContext mount_user_path(const std::filesystem::path& workspace_root, const std::string& raw_path);
std::vector<AttachedFileContext> load_mounted_path_content(
    const std::filesystem::path& workspace_root,
    const MountedPathContext& mounted_path);
std::string describe_mounted_path(const MountedPathContext& mounted_path);
std::vector<std::string> approval_labels(const std::string& task);
std::vector<std::string> prepared_approval_labels(const std::string& task);

}  // namespace maglev::chat_detail
