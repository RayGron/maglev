#include "maglev/gateway/gateway.h"

namespace maglev {

std::unique_ptr<AgentGateway> make_gateway(const GatewayConfig& config) {
    if (config.use_mock_gateway) {
        return std::make_unique<MockGateway>();
    }
    if (config.backend_mode == BackendMode::OpenAiCompat) {
        return std::make_unique<OpenAiCompatGateway>(config);
    }
    return std::make_unique<SecureGateway>(config);
}

}  // namespace maglev
