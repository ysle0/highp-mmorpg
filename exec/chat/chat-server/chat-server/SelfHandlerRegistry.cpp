#include "SelfHandlerRegistry.h"

SelfHandlerRegistry::~SelfHandlerRegistry() noexcept {
    _factories.clear();
}

void SelfHandlerRegistry::Add(FactoryFn fn) {
    _factories.push_back(std::move(fn));
}

void SelfHandlerRegistry::RegisterAll(
    const std::unique_ptr<highp::net::PacketDispatcher>& dispatcher,
    const std::shared_ptr<highp::log::Logger>& logger,
    const std::shared_ptr<RoomManager>& roomManager,
    const std::shared_ptr<UserManager>& userManager
) const {
    logger->Info("[SelfHandlerRegistry] registering {} handler(s)", _factories.size());
    for (auto& fn : _factories) {
        fn(dispatcher, logger, roomManager, userManager);
    }
}
