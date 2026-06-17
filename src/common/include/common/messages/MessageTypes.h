#pragma once

#include <cstdint>

namespace messages {

// ---------------------------------------------------------------------------
// LogLevel — ordered from most to least verbose.
// ---------------------------------------------------------------------------
enum class LogLevel : uint8_t {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Critical = 4,
};

// ---------------------------------------------------------------------------
// MessageId — closed set of message identifiers.
// ---------------------------------------------------------------------------
enum class MessageId : uint16_t {
    // System
    SYSTEM_INIT_COMPLETE = 1000,
    APPLICATION_SHUTDOWN = 1001,
    RT_CYCLE_STATS       = 1002,  // p1=cycleCount, p2=maxOverrunNs, p3=avgOverrunNs

    // Main
    MAIN_CONFIG_LOADED   = 2000,
    MAIN_CONFIG_ERROR    = 2001,
    MAIN_MLOCKALL_FAILED = 2002,
    MAIN_ETHERCAT_READY  = 2003,  // p1=slaveCount, p2=workingCounter, p3=fullyCommunicating
    MAIN_ETHERCAT_UNAVAILABLE = 2004,
    MAIN_SIM_INIT_FAILED = 2005,

    // gRPC
    GRPC_LOG_CLIENT_CONNECTED   = 3000,
    GRPC_LOG_CLIENT_DISCONNECTED = 3001,

    // Hardware
    HW_SLAVE_DISCOVERED = 4000,
    HW_PDO_FROZEN       = 4001,

    // Safety
    SAFETY_ESTOP_ACTIVE    = 5000,
    SAFETY_FAULT_RAISED    = 5001,  // p1=alarmId
    SAFETY_FAULT_CLEARED   = 5002,
    SAFETY_HALT_ACTIVE     = 5003,

    // Redundancy
    REDUNDANCY_ROLE_CHANGED = 6000,  // p1=newRole (0=STANDBY, 1=PRIMARY)
    REDUNDANCY_HEARTBEAT_LOST = 6001,
    REDUNDANCY_FAILOVER_COMPLETE = 6002,
};

} // namespace messages
