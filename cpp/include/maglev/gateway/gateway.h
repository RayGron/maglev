#pragma once

#include <memory>

#include "maglev/gateway/agent_gateway.h"
#include "maglev/runtime/config.h"
#include "maglev/gateway/mock_gateway.h"
#include "maglev/gateway/openai_compat_gateway.h"
#include "maglev/gateway/secure_gateway.h"

namespace maglev {

std::unique_ptr<AgentGateway> make_gateway(const GatewayConfig& config);

}  // namespace maglev
