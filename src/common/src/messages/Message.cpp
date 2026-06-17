#include "common/messages/Message.h"

#include <sstream>
#include <string_view>

namespace messages {

std::string formatLogEntry(const LogEntry& entry)
{
    std::ostringstream oss;
    oss << "[" << entry.timestampNs << "] "
        << "[" << static_cast<uint8_t>(entry.level) << "] "
        << "[" << static_cast<uint16_t>(entry.id) << "] "
        << messageIdLabel(entry.id);
    if (entry.p1 != 0 || entry.p2 != 0 || entry.p3 != 0) {
        oss << " p1=" << entry.p1 << " p2=" << entry.p2 << " p3=" << entry.p3;
    }
    return oss.str();
}

std::string_view messageIdLabel(MessageId id)
{
    switch (id) {
        // System
        case MessageId::SYSTEM_INIT_COMPLETE:     return "System init complete";
        case MessageId::APPLICATION_SHUTDOWN:     return "Application shutdown";
        case MessageId::RT_CYCLE_STATS:           return "RT cycle stats";
        // Main
        case MessageId::MAIN_CONFIG_LOADED:       return "Config loaded";
        case MessageId::MAIN_CONFIG_ERROR:        return "Config error";
        case MessageId::MAIN_MLOCKALL_FAILED:     return "mlockall failed";
        case MessageId::MAIN_ETHERCAT_READY:      return "EtherCAT ready";
        case MessageId::MAIN_ETHERCAT_UNAVAILABLE:return "EtherCAT unavailable";
        case MessageId::MAIN_SIM_INIT_FAILED:     return "Sim init failed";
        // gRPC
        case MessageId::GRPC_LOG_CLIENT_CONNECTED:    return "gRPC client connected";
        case MessageId::GRPC_LOG_CLIENT_DISCONNECTED: return "gRPC client disconnected";
        // Hardware
        case MessageId::HW_SLAVE_DISCOVERED:      return "Slave discovered";
        case MessageId::HW_PDO_FROZEN:            return "PDO frozen";
        // Safety
        case MessageId::SAFETY_ESTOP_ACTIVE:      return "E-Stop active";
        case MessageId::SAFETY_FAULT_RAISED:      return "Fault raised";
        case MessageId::SAFETY_FAULT_CLEARED:     return "Fault cleared";
        case MessageId::SAFETY_HALT_ACTIVE:       return "Halt active";
        // Redundancy
        case MessageId::REDUNDANCY_ROLE_CHANGED:      return "Role changed";
        case MessageId::REDUNDANCY_HEARTBEAT_LOST:    return "Heartbeat lost";
        case MessageId::REDUNDANCY_FAILOVER_COMPLETE: return "Failover complete";
        default:                                    return "Unknown";
    }
}

} // namespace messages
