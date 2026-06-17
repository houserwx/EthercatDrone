#include "fc/safety/MachineStateController.h"

#include "common/log/LogHelper.h"
#include "common/messages/MessageTypes.h"

namespace fc::safety {

MachineStateController::MachineStateController() = default;

void MachineStateController::tick(bool estop, uint64_t nowNs) noexcept
{
    (void)nowNs;  // Used in Phase 2 for timestamped state events
    // Service clear alarms request from gRPC
    if (clearAlarmsRequested_.load(std::memory_order_acquire)) {
        clearAlarmsRequested_.store(false, std::memory_order_release);
        latchTable_.fill(false);
        activeLatchCount_ = 0;
    }

    // Recompute fault/halt flags
    faultActive_ = (activeFaultMask_ != 0);
    haltActive_  = (state_ >= MachineState::Halted);

    // E-Stop handling
    if (estop) {
        if (state_ != MachineState::EStop) {
            state_ = MachineState::EStop;
            common::log::logError(messages::MessageId::SAFETY_ESTOP_ACTIVE);
        }
    } else if (state_ == MachineState::EStop) {
        // E-Stop released — return to previous state or Running
        state_ = MachineState::Running;
    }

    // Latch check: if any latches are set, force Halted (unless E-Stop)
    if (activeLatchCount_ > 0 && state_ == MachineState::Running) {
        state_ = MachineState::Halted;
        common::log::logError(messages::MessageId::SAFETY_HALT_ACTIVE);
    }
}

void MachineStateController::raiseAlarm(AlarmId id, AlarmSeverity sev, uint64_t nowNs) noexcept
{
    (void)nowNs;
    uint8_t idx = static_cast<uint8_t>(id);

    if (sev == AlarmSeverity::HaltFault) {
        if (!latchTable_[idx]) {
            latchTable_[idx] = true;
            ++activeLatchCount_;
        }
    } else {
        // Non-latching fault: set bit in mask
        activeFaultMask_ |= (1ULL << idx);
    }

    common::log::logWarn(messages::MessageId::SAFETY_FAULT_RAISED,
                         static_cast<int64_t>(idx));
}

void MachineStateController::clearFault(AlarmId id) noexcept
{
    uint8_t idx = static_cast<uint8_t>(id);
    activeFaultMask_ &= ~(1ULL << idx);
    common::log::logInfo(messages::MessageId::SAFETY_FAULT_CLEARED,
                         static_cast<int64_t>(idx));
}

} // namespace fc::safety
