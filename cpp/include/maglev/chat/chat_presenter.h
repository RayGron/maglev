#pragma once

#include <string>

#include "maglev/contracts/contracts.h"
#include "maglev/runtime/session.h"

namespace maglev::chat_detail {

void print_plan(const TaskPlanResponse& plan);
void print_run_snapshot(const std::string& title, const RunSnapshot& run);
void print_session_status(const SessionState& session);
void print_prepared_plan(const SessionState& session);
std::string diff_summary_from_run(const RunSnapshot& run);

}  // namespace maglev::chat_detail
