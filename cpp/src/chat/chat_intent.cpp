#include "maglev/chat/chat_intent.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "maglev/runtime/util.h"

namespace maglev::chat_detail {

namespace {

bool contains_any(const std::string& value, const std::vector<std::string>& markers) {
    return std::any_of(markers.begin(), markers.end(), [&](const std::string& marker) {
        return value.find(marker) != std::string::npos;
    });
}

bool contains_agent_keywords(const std::string& input) {
    static const std::vector<std::string> repo_keywords = {
        "repo",       "repository", "репозитор", "git",   "branch", "diff",   "commit", "push",    "deploy",
        "server",     "ssh",        "file",      "files", "файл",   "файлы",  "код",    "project", "проект",
        "workspace",  "изменени",   "незакоммич","check", "tests",  "origin", "ветк",   "коммит",
        "пуш",        "push",       "commit",
    };
    static const std::vector<std::string> action_keywords = {
        "show", "list", "scan", "inspect", "analyze", "review", "run", "apply", "fix", "update", "create",
        "write", "read", "выведи", "покажи", "пройдись", "проанализируй", "проверь", "собери", "исправь",
        "обнови", "создай", "прочитай", "сделай", "выполни", "запушь", "закоммить", "закомить", "зафиксируй",
        "отправь", "опубликуй",
    };

    const bool has_repo = std::any_of(repo_keywords.begin(), repo_keywords.end(), [&](const std::string& marker) {
        return input.find(marker) != std::string::npos;
    });
    const bool has_action = std::any_of(action_keywords.begin(), action_keywords.end(), [&](const std::string& marker) {
        return input.find(marker) != std::string::npos;
    });
    return has_repo && has_action;
}

}  // namespace

bool looks_like_deploy_instruction(const std::string& task) {
    const auto lowered = to_lower(task);
    return lowered.find("подключись к серверу") != std::string::npos || lowered.find("deploy") != std::string::npos ||
           lowered.find("connect to server") != std::string::npos;
}

bool looks_like_apply_request(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "apply changes", "apply edits", "применяй изменения", "Применяй изменения", "примени изменения",
        "Примени изменения", "внеси изменения", "Внеси изменения", "сохрани изменения", "Сохрани изменения",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

bool looks_like_checks_request(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "run checks", "run tests", "запусти проверки", "Запусти проверки", "запусти тесты", "Запусти тесты",
        "проверь проект", "Проверь проект", "прогони проверки", "Прогони проверки",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

bool looks_like_commit_request(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "initial commit",           "commit changes",            "сделай коммит",      "Сделай коммит",
        "сделай initial commit",    "Сделай initial commit",    "первый коммит",      "Первый коммит",
        "начальный коммит",         "Начальный коммит",         "закоммить",          "Закоммить",
        "закомить",                 "Закомить",                 "зафиксируй изменения","Зафиксируй изменения",
        "зафиксируй текущие изменения", "Зафиксируй текущие изменения",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

bool looks_like_push_request(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "push branch",                  "push current branch",          "запушь",                 "Запушь",
        "запушь ветку",                "Запушь ветку",                "запушь текущую ветку",   "Запушь текущую ветку",
        "запушь изменения",            "Запушь изменения",            "отправь в origin",       "Отправь в origin",
        "отправь текущую ветку в origin", "Отправь текущую ветку в origin",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

UserIntent infer_user_intent(const std::string& input) {
    const auto lowered = to_lower(trim(input));
    if (lowered.empty()) {
        return UserIntent::Chat;
    }

    static const std::vector<std::string> strong_chat_markers = {
        "какая модель", "что за модель", "что ты за модель", "who are you", "what model", "which model", "what are you",
        "hello", "hi", "hey", "привет", "здравствуй", "здравствуйте",
    };
    static const std::vector<std::string> strong_agent_markers = {
        "пройдись по",        "посмотри репозиторий", "проанализируй",   "проверь",         "собери",
        "исправь",            "измени",                "обнови",           "создай файл",     "сделай коммит",
        "сделай пуш",         "сделай initial commit", "initial commit",   "сделай первый коммит",
        "первый коммит",      "начальный коммит",      "закоммить",        "закомить",
        "зафиксируй изменения","зафиксируй текущие изменения", "запушь",   "запушь ветку",
        "запушь изменения",   "отправь в origin",      "запушь в origin",  "Запушь",
        "Запушь текущую ветку","Запушь изменения",     "Отправь в origin", "Отправь текущую ветку в origin",
        "подключись к серверу",
        "run checks",         "scan the repository",
        "scan repo",          "inspect repo",          "inspect repository","analyze repository","analyze repo",
        "show uncommitted changes", "uncommitted changes", "git status", "edit file", "update file", "create file",
        "commit changes", "push branch", "deploy",
    };

    for (const auto& marker : strong_chat_markers) {
        if (lowered == marker || (lowered.find(marker) != std::string::npos && !contains_agent_keywords(lowered))) {
            return UserIntent::Chat;
        }
    }

    for (const auto& marker : strong_agent_markers) {
        if (lowered.find(marker) != std::string::npos) {
            return UserIntent::AgentTask;
        }
    }

    if (looks_like_commit_request(input) || looks_like_push_request(input)) {
        return UserIntent::AgentTask;
    }

    return contains_agent_keywords(lowered) ? UserIntent::AgentTask : UserIntent::Chat;
}

bool looks_like_uncommitted_changes_request(const std::string& input) {
    const auto lowered = to_lower(input);
    static const std::vector<std::string> change_markers = {
        "uncommitted changes", "git status", "незакоммиченные изменения", "незакоммиченные",
        "несохраненные изменения", "изменения в репозитории", "выведи изменения", "show changes", "show status",
        "working tree",
    };
    static const std::vector<std::string> repo_markers = {"repo", "repository", "репозитор", "git", "maglev"};

    const bool has_change_marker = std::any_of(change_markers.begin(), change_markers.end(), [&](const std::string& marker) {
        return lowered.find(marker) != std::string::npos;
    });
    const bool has_repo_marker = std::any_of(repo_markers.begin(), repo_markers.end(), [&](const std::string& marker) {
        return lowered.find(marker) != std::string::npos;
    });
    return has_change_marker && has_repo_marker;
}

bool looks_like_identity_question(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "кто ты", "Кто ты", "что ты такое", "Что ты такое", "who are you", "what are you",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

bool looks_like_capability_question(const std::string& input) {
    const auto trimmed = trim(input);
    const auto lowered = to_lower(trimmed);
    static const std::vector<std::string> markers = {
        "что ты умеешь", "Что ты умеешь", "чем можешь помочь", "Чем можешь помочь", "what can you do", "how can you help",
    };
    return contains_any(trimmed, markers) || contains_any(lowered, markers);
}

}  // namespace maglev::chat_detail
