#pragma once

#include <string>

namespace maglev::chat_detail {

enum class UserIntent {
    Chat,
    AgentTask,
};

bool looks_like_deploy_instruction(const std::string& task);
bool looks_like_apply_request(const std::string& input);
bool looks_like_checks_request(const std::string& input);
bool looks_like_commit_request(const std::string& input);
bool looks_like_push_request(const std::string& input);
bool looks_like_file_content_request(const std::string& input);
UserIntent infer_user_intent(const std::string& input);
bool looks_like_uncommitted_changes_request(const std::string& input);
bool looks_like_identity_question(const std::string& input);
bool looks_like_capability_question(const std::string& input);

}  // namespace maglev::chat_detail
