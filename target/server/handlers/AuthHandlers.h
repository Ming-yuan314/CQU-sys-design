#pragma once

#include "../core/CommandRouter.h"
#include "../core/ServerConfig.h"

namespace server {

void RegisterAuthHandlers(CommandRouter& router, const ServerConfig& config);

} // namespace server
