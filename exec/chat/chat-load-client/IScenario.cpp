#include "IScenario.h"
#include "util/MalformedPacketGenerator.h"

#include <algorithm>
#include <barrier>
#include <chrono>
#include <format>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "util/ScenarioConfig.h"

void IScenario::ConnectClients(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const ScenarioConfig& config,
    int targetCount,
    int rampDelayMs,
    const std::shared_ptr<highp::log::Logger>& logger,
    const std::shared_ptr<highp::log::ILogger>& innerLogger,
    const std::shared_ptr<highp::metrics::IClientMetrics>& metrics) {
    const int currentCount = static_cast<int>(sessions.size());
    const int toConnect = targetCount - currentCount;

    if (toConnect <= 0) {
        logger->Info("Already have {} sessions, target is {}. Skipping.", currentCount, targetCount);
        return;
    }

    int connected = 0;
    for (int i = 0; i < toConnect; ++i) {
        int index = currentCount + i;
        std::string nickname = std::format("{}-{}", config.nickname_prefix, index);

        auto session = std::make_unique<ClientSession>(
            index, nickname, innerLogger, metrics);

        if (session->Connect(config.target_host, static_cast<unsigned short>(config.target_port))) {
            session->StartRecv();
            sessions.push_back(std::move(session));
            ++connected;
        }
        else {
            logger->Error("Failed to connect session {}", index);
        }

        if (i + 1 < toConnect && rampDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(rampDelayMs));
        }
    }

    logger->Info("Connected {}/{} new sessions (total: {})", connected, toConnect, sessions.size());
}

void IScenario::JoinAll(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const std::shared_ptr<highp::log::Logger>& logger) {
    for (const std::unique_ptr<ClientSession>& session : sessions) {
        session->JoinRoom();
    }
    logger->Info("All {} sessions joined room.", sessions.size());
}

void IScenario::Hold(
    int durationSec,
    const std::shared_ptr<highp::log::Logger>& logger
) {
    for (int elapsed = 0; elapsed < durationSec; ++elapsed) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if ((elapsed + 1) % 30 == 0 || elapsed + 1 == durationSec) {
            logger->Info("  hold: {}/{}s", elapsed + 1, durationSec);
        }
    }
}

void IScenario::SteadySend(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    int sendDurationSec,
    int sendIntervalMs,
    int messageSizeBytes,
    const std::shared_ptr<highp::log::Logger>& logger
) {
    const std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
    const std::chrono::seconds durationSec = std::chrono::seconds(sendDurationSec);
    const std::chrono::milliseconds intervalMs = std::chrono::milliseconds(sendIntervalMs);

    std::string message(
        messageSizeBytes > 0 ? static_cast<size_t>(messageSizeBytes) : 1, 'x');

    std::chrono::time_point<std::chrono::steady_clock> nextSendTime = startTime + intervalMs;
    int totalSent = 0;
    long long lastLogAt = -1;

    while (std::chrono::steady_clock::now() - startTime < durationSec) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            for (auto& session : sessions) {
                session->SendMessage(message);
            }
            totalSent += static_cast<int>(sessions.size());
            nextSendTime += intervalMs;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 0 && elapsed % 30 == 0 && elapsed != lastLogAt) {
            lastLogAt = elapsed;
            logger->Info("  steady_send: {}/{}s, total messages sent: {}",
                         elapsed, sendDurationSec, totalSent);
        }
    }

    logger->Info("Steady send complete. Total messages sent: {}", totalSent);
}

void IScenario::DisconnectAll(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const std::shared_ptr<highp::log::Logger>& logger) {
    for (auto& session : sessions) {
        session->Disconnect();
    }
    logger->Info("All {} sessions disconnected.", sessions.size());
    sessions.clear();
}

void IScenario::BurstSend(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    int burstCount,
    int burstMessages,
    int idleWindowMs,
    int messageSizeBytes,
    const std::shared_ptr<highp::log::Logger>& logger) {
    const int sessionCount = static_cast<int>(sessions.size());
    if (sessionCount == 0) return;

    const int threadCount = std::min(
        sessionCount,
        static_cast<int>(std::thread::hardware_concurrency()));

    std::string message(messageSizeBytes > 0 ? static_cast<size_t>(messageSizeBytes) : 1, 'x');
    int totalSent = 0;

    for (int wave = 0; wave < burstCount; ++wave) {
        const auto waveStart = std::chrono::steady_clock::now();

        std::barrier syncPoint(threadCount);
        std::vector<std::jthread> workers;
        workers.reserve(threadCount);

        const int chunkSize = sessionCount / threadCount;
        const int remainder = sessionCount % threadCount;

        for (int t = 0; t < threadCount; ++t) {
            int begin = t * chunkSize + std::min(t, remainder);
            int end = begin + chunkSize + (t < remainder ? 1 : 0);

            workers.emplace_back([&, begin, end]() {
                syncPoint.arrive_and_wait();
                for (int msg = 0; msg < burstMessages; ++msg) {
                    for (int i = begin; i < end; ++i) {
                        if (sessions[i]->IsConnected()) {
                            sessions[i]->SendMessage(message);
                        }
                    }
                }
            });
        }

        workers.clear(); // join all

        const auto waveEnd = std::chrono::steady_clock::now();
        const auto waveMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            waveEnd - waveStart).count();

        int waveSent = sessionCount * burstMessages;
        totalSent += waveSent;

        logger->Info("  burst wave {}/{}: {} messages in {}ms, idle {}ms",
                     wave + 1, burstCount, waveSent, waveMs, idleWindowMs);

        if (wave + 1 < burstCount) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idleWindowMs));
        }
    }

    logger->Info("Burst send complete. Total messages sent: {}", totalSent);
}

void IScenario::ReconnectWave(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    const ScenarioConfig& config,
    int waveCount,
    int holdBetweenWaveSec,
    const std::shared_ptr<highp::log::Logger>& logger,
    const std::shared_ptr<highp::log::ILogger>& innerLogger,
    const std::shared_ptr<highp::metrics::IClientMetrics>& metrics) {
    for (int wave = 0; wave < waveCount; ++wave) {
        logger->Info("  reconnect wave {}/{}: disconnecting {} sessions",
                     wave + 1, waveCount, sessions.size());
        DisconnectAll(sessions, logger);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        logger->Info("  reconnect wave {}/{}: reconnecting {} clients",
                     wave + 1, waveCount, config.client_count);
        
        ConnectClients(sessions, config, config.client_count,
                       config.FindPhase(PhaseType::Connect).connect.ramp_delay_ms,
                       logger, innerLogger, metrics);

        JoinAll(sessions, logger);

        if (holdBetweenWaveSec > 0) {
            Hold(holdBetweenWaveSec, logger);
        }
    }

    logger->Info("Reconnect storm complete. {} waves executed.", waveCount);
}

void IScenario::MalformedSend(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    int durationSec,
    int sendIntervalMs,
    int messageSizeBytes,
    int malformedPercent,
    const std::shared_ptr<highp::log::Logger>& logger) {
    const auto startTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(durationSec);
    const auto interval = std::chrono::milliseconds(sendIntervalMs);

    std::string normalMessage(messageSizeBytes > 0 ? static_cast<size_t>(messageSizeBytes) : 1, 'x');
    MalformedPacketGenerator generator;
    std::mt19937 rng(std::random_device{}());

    auto nextSendTime = startTime + interval;
    int totalNormal = 0;
    int totalMalformed = 0;
    long long lastLogAt = -1;

    while (std::chrono::steady_clock::now() - startTime < duration) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            for (auto& session : sessions) {
                if (!session->IsConnected()) continue;

                if (static_cast<int>(rng() % 100) < malformedPercent) {
                    auto badPacket = generator.Generate(messageSizeBytes);
                    session->SendRawBytes(std::span<const uint8_t>{badPacket.data(), badPacket.size()});
                    ++totalMalformed;
                }
                else {
                    session->SendMessage(normalMessage);
                    ++totalNormal;
                }
            }
            nextSendTime += interval;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 0 && elapsed % 30 == 0 && elapsed != lastLogAt) {
            lastLogAt = elapsed;
            logger->Info("  malformed_send: {}/{}s, normal: {}, malformed: {}",
                         elapsed, durationSec, totalNormal, totalMalformed);
        }
    }

    logger->Info("Malformed send complete. Normal: {}, Malformed: {}", totalNormal, totalMalformed);
}

void IScenario::RoleSend(
    std::vector<std::unique_ptr<ClientSession>>& sessions,
    int senderCount,
    int durationSec,
    int sendIntervalMs,
    int messageSizeBytes,
    const std::shared_ptr<highp::log::Logger>& logger) {
    senderCount = std::min(senderCount, static_cast<int>(sessions.size()));

    const auto startTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(durationSec);
    const auto interval = std::chrono::milliseconds(sendIntervalMs);

    std::string message(messageSizeBytes > 0 ? static_cast<size_t>(messageSizeBytes) : 1, 'x');

    const int receiverCount = static_cast<int>(sessions.size()) - senderCount;
    logger->Info("Role send: {} senders, {} receivers", senderCount, receiverCount);

    auto nextSendTime = startTime + interval;
    int totalSent = 0;
    long long lastLogAt = -1;

    while (std::chrono::steady_clock::now() - startTime < duration) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextSendTime) {
            for (int i = 0; i < senderCount; ++i) {
                if (sessions[i]->IsConnected()) {
                    sessions[i]->SendMessage(message);
                    ++totalSent;
                }
            }
            nextSendTime += interval;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 0 && elapsed % 30 == 0 && elapsed != lastLogAt) {
            lastLogAt = elapsed;
            logger->Info("  role_send: {}/{}s, senders: {}, total sent: {}",
                         elapsed, durationSec, senderCount, totalSent);
        }
    }

    logger->Info("Role send complete. Total messages sent: {}", totalSent);
}
