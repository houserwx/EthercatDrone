#include "fc/redundancy/RedundancyController.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace fc::redundancy {

// ---------------------------------------------------------------------------
// Constructor — wire up thread configuration.
// ---------------------------------------------------------------------------
RedundancyController::RedundancyController(
    std::string localIp, std::string peerIp,
    uint16_t port,
    uint32_t heartbeatIntervalMs,
    uint32_t timeoutMs)
    : Threadrunner(common::rt::ThreadConfiguration{
          .name            = "Redundancy",
          .cpuCore         = -1,
          .priority        = 0,
          .useRealtime     = false,
          .stackPrefaultBytes = 0
      })
    , localIp_(std::move(localIp))
    , peerIp_(std::move(peerIp))
    , port_(port)
    , heartbeatIntervalMs_(heartbeatIntervalMs)
    , timeoutMs_(timeoutMs)
{
}

// ---------------------------------------------------------------------------
// requestStop()
// ---------------------------------------------------------------------------
void RedundancyController::requestStop() noexcept
{
    running_.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Socket helpers
// ---------------------------------------------------------------------------
bool RedundancyController::createSocket()
{
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        std::perror("[Redundancy] socket() failed");
        return false;
    }

    // Allow local broadcast if needed
    int opt = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        std::perror("[Redundancy] setsockopt SO_BROADCAST failed");
    }

    // Set socket timeout
    struct timeval tv{};
    tv.tv_sec  = timeoutMs_ / 1000;
    tv.tv_usec = (timeoutMs_ % 1000) * 1000;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::perror("[Redundancy] setsockopt SO_RCVTIMEO failed");
    }

    // Bind to local address
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port_);
    addr.sin_addr.s_addr = inet_addr(localIp_.c_str());

    if (addr.sin_addr.s_addr == INADDR_NONE) {
        // Try resolving hostname
        struct hostent* he = gethostbyname(localIp_.c_str());
        if (!he) {
            std::fprintf(stderr, "[Redundancy] Cannot resolve local IP '%s'\n", localIp_.c_str());
            closeSocket();
            return false;
        }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (bind(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::perror("[Redundancy] bind() failed");
        closeSocket();
        return false;
    }

    std::printf("[Redundancy] Socket bound to %s:%u\n", localIp_.c_str(), port_);
    return true;
}

void RedundancyController::closeSocket()
{
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}

bool RedundancyController::sendHeartbeat(const HeartbeatPacket& packet)
{
    if (socket_ < 0) return false;

    struct sockaddr_in dest{};
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(port_);
    dest.sin_addr.s_addr = inet_addr(peerIp_.c_str());

    if (dest.sin_addr.s_addr == INADDR_NONE) {
        return false;
    }

    ssize_t sent = sendto(socket_, &packet, sizeof(packet), 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    return sent == sizeof(packet);
}

bool RedundancyController::receiveHeartbeat(HeartbeatPacket& packet, uint32_t timeoutMs)
{
    if (socket_ < 0) return false;

    // Set receive timeout
    struct timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in from{};
    socklen_t fromLen = sizeof(from);

    ssize_t recv = recvfrom(socket_, &packet, sizeof(packet), 0,
                            (struct sockaddr*)&from, &fromLen);
    return recv == sizeof(packet);
}

uint64_t RedundancyController::nowNs() const noexcept
{
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}

// ---------------------------------------------------------------------------
// Role election
// ---------------------------------------------------------------------------
void RedundancyController::electRole()
{
    // Simple MAC-based election: lower MAC address becomes primary.
    // For now, use IP address as a proxy (lower IP = primary preference).
    uint32_t localAddr = inet_addr(localIp_.c_str());
    uint32_t peerAddr  = inet_addr(peerIp_.c_str());

    if (localAddr < peerAddr) {
        role_.store(Role::PRIMARY, std::memory_order_release);
        std::printf("[Redundancy] Role election: PRIMARY (local=%s < peer=%s)\n",
                    localIp_.c_str(), peerIp_.c_str());
    } else {
        role_.store(Role::STANDBY, std::memory_order_release);
        std::printf("[Redundancy] Role election: STANDBY (local=%s >= peer=%s)\n",
                    localIp_.c_str(), peerIp_.c_str());
    }
}

// ---------------------------------------------------------------------------
// run() — the redundancy thread body.
// ---------------------------------------------------------------------------
void RedundancyController::run()
{
    running_.store(true, std::memory_order_release);

    // Initialize socket
    if (!createSocket()) {
        std::fprintf(stderr, "[Redundancy] Cannot create socket — failing over to PRIMARY\n");
        role_.store(Role::PRIMARY, std::memory_order_release);
        // Continue running without heartbeat (degenerate single-node mode)
    }

    // Initial role election
    electRole();

    HeartbeatPacket txPacket{};
    HeartbeatPacket rxPacket{};

    std::printf("[Redundancy] Starting heartbeat loop (interval=%u ms, timeout=%u ms)\n",
                heartbeatIntervalMs_, timeoutMs_);

    while (running_.load(std::memory_order_acquire)) {
        // Build and send heartbeat
        txPacket.timestamp_ns = nowNs();
        txPacket.role         = static_cast<uint8_t>(role_.load(std::memory_order_relaxed));
        txPacket.cycle_count  = 0; // Updated by caller if needed

        if (socket_ >= 0) {
            sendHeartbeat(txPacket);

            // Try to receive peer heartbeat (non-blocking with timeout)
            if (receiveHeartbeat(rxPacket, timeoutMs_)) {
                lastHeartbeatReceivedNs_.store(rxPacket.timestamp_ns, std::memory_order_release);
                peerCycleCount_.store(rxPacket.cycle_count, std::memory_order_release);

                // If we were primary and peer is also primary, resolve conflict
                uint8_t peerRole = rxPacket.role;
                if (peerRole == static_cast<uint8_t>(Role::PRIMARY) &&
                    role_.load(std::memory_order_acquire) == Role::PRIMARY) {
                    std::printf("[Redundancy] Conflict: both nodes PRIMARY — re-electing\n");
                    electRole();
                }
            } else {
                // No heartbeat received — check if we should fail over
                uint64_t now = nowNs();
                uint64_t lastRx = lastHeartbeatReceivedNs_.load(std::memory_order_acquire);

                if (lastRx > 0) {
                    uint64_t elapsedMs = (now - lastRx) / 1'000'000;
                    if (elapsedMs > timeoutMs_ * 2) {
                        std::printf("[Redundancy] Peer heartbeat timeout (%llu ms) — promoting to PRIMARY\n",
                                    (unsigned long long)elapsedMs);
                        role_.store(Role::PRIMARY, std::memory_order_release);
                    }
                }
            }
        }

        // Sleep until next heartbeat interval
        usleep(heartbeatIntervalMs_ * 1000);
    }

    std::printf("[Redundancy] Thread exiting\n");
    closeSocket();
}

} // namespace fc::redundancy
