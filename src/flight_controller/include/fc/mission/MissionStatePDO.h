#pragma once

#include "fc/pdo/IHardwareAdapter.h"
#include "fc/pdo/PDO.h"

#include <cstdint>
#include <vector>

namespace fc::mission {

// ============================================================================
// MissionFlagEntry — one projection binding.
// Projects a mission state flag into the PDO image so it's accessible
// as a standard DigitalInput entry.
// ============================================================================
struct MissionFlagEntry {
    const bool* source;   // Points to MachineStateController flag
    uint8_t*    imageSlot; // Destination in PDO image
};

// ============================================================================
// MissionStatePDO — RT-phase IHardwareAdapter that projects mission state bits
// into a process image so they are accessible as standard DigitalInput entries.
//
// Same pattern as MachineStatePDO: 2 virtual calls per cycle, no virtual
// dispatch in hot path.
// ============================================================================
class MissionStatePDO final : public fc::pdo::IHardwareAdapter {
public:
    MissionStatePDO(std::vector<uint8_t>     image,
                    std::vector<MissionFlagEntry> flagEntries,
                    std::vector<fc::pdo::PDOEntry> entries) noexcept;

    bool initialize() override { return true; }
    void onBeforeReadInputs() noexcept override;
    void onAfterWriteOutputs() noexcept override {}

    [[nodiscard]] const std::vector<fc::pdo::PDO>& getPDOs() const noexcept { return pdos_; }

private:
    std::vector<uint8_t>          image_;
    std::vector<MissionFlagEntry> flagEntries_;
    std::vector<fc::pdo::PDO>     pdos_;
};

} // namespace fc::mission
