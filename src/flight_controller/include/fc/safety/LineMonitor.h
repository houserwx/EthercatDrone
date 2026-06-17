#pragma once

#include "common/rt/SignalProcess.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace fc::safety {

class WrapperPool;

// ---------------------------------------------------------------------------
// MonitorType — tagged discriminant for switch dispatch.
// ---------------------------------------------------------------------------
enum class MonitorType : uint8_t {
    Rate,
    Spacing,
    Width,
    Gap,
    Speed,
    DwellTime,
    DetectHealth,
    EncoderHealth
};

// ---------------------------------------------------------------------------
// LineMonitor — signal-level queue monitors (edge/encoder triggered).
// ---------------------------------------------------------------------------
class LineMonitor {
public:
    void load(const std::vector<MonitorType>& types);
    void freeze() noexcept;
    void tick(int32_t encoderNow, bool rising, bool falling, uint64_t nowNs, WrapperPool& pool) noexcept;

private:
    struct MonitorState {
        MonitorType type{MonitorType::Rate};
        int64_t     id{0};
        std::string name;
        int         alarmFlagIdx{-1};
        int64_t     param1{0};
        int64_t     param2{0};
    };
    std::vector<MonitorState> states_;
};

} // namespace fc::safety
