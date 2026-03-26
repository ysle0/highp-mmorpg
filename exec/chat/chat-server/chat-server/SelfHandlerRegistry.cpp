#include "SelfHandlerRegistry.h"

SelfHandlerRegistry::~SelfHandlerRegistry() noexcept {
    _factories.clear();
}

void SelfHandlerRegistry::Add(FactoryFn fn) {
    _factories.push_back(std::move(fn));
}

void SelfHandlerRegistry::RegisterAll(
    highp::net::PacketDispatcher& dispatcher,
    std::shared_ptr<highp::log::Logger> logger
) const {
    logger->Info("[SelfHandlerRegistry] registering {} handler(s)", _factories.size());
    for (auto& fn : _factories) {
        fn(dispatcher, logger);
    }
}
