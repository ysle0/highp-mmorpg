#pragma once
#include <concepts>
#include <functional>
#include <memory>
#include <vector>

#include "server/PacketDispatcher.hpp"

class RoomManager;
class UserManager;

/**
 * IPacketHandler 를 상송하는 각 핸들러 자신이 GameLoop 에 등록하는 데에 사용.
 * TODO: 이후 크로스 플랫폼으로 진화할 시기에 Dependency Injection, IoC Container 로 대체.
 */
class SelfHandlerRegistry final {
public:
    using FactoryFn = std::function<void(
        const std::unique_ptr<highp::net::PacketDispatcher>& dispatcher,
        const std::shared_ptr<highp::log::Logger>& logger,
        const std::shared_ptr<RoomManager>& roomManager,
        const std::shared_ptr<UserManager>& userManager)>;

    ~SelfHandlerRegistry() noexcept;

    static SelfHandlerRegistry& Instance();
    void Add(FactoryFn fn);
    void RegisterAll(
        const std::unique_ptr<highp::net::PacketDispatcher>& dispatcher,
        const std::shared_ptr<highp::log::Logger>& logger,
        const std::shared_ptr<RoomManager>& roomManager,
        const std::shared_ptr<UserManager>& userManager) const;

private:
    std::vector<FactoryFn> _factories;
};

inline SelfHandlerRegistry& SelfHandlerRegistry::Instance() {
    static SelfHandlerRegistry r;
    return r;
}


/*
 * @description 핸들러를 등록. 
 * @param HandlerClass SelfHandlerRegistry::FactoryFn signature
 * @param isEnable true: 서버에 등록 및 패킷을 받음. false: 수동으로 비활성화.
 */
template <typename THandler, typename TPayload>
    requires highp::net::PayloadType<TPayload> &&
    std::derived_from<THandler, highp::net::IPacketHandler<TPayload>>
bool registerSelf(const bool isEnable) {
    if (!isEnable) return false;

    SelfHandlerRegistry::Instance().Add(
        [](const std::unique_ptr<highp::net::PacketDispatcher>& dispatcher,
           const std::shared_ptr<highp::log::Logger>& logger,
           const std::shared_ptr<RoomManager>& roomManager,
           const std::shared_ptr<UserManager>& userManager
    ) {
            auto handler = std::make_unique<THandler>(
                logger,
                roomManager,
                userManager);
            dispatcher->RegisterHandler<TPayload>(std::move(handler));
        });
    return true;
}
