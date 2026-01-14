#pragma once

#include "../core/CommandRouter.h"
#include "../core/ServerConfig.h"

namespace server {

void RegisterFileHandlers(CommandRouter& router, const ServerConfig& config);

} // namespace server
