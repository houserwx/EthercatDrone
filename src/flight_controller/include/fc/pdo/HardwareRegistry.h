#pragma once
#include "fc/pdo/IHardwareAdapter.h"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fc::pdo {

// ============================================================
// HardwareRegistry — owns backends, orchestrates the RT cycle,
// and provides UUID-keyed init-time channel resolution.
//
// RT path (readAll/writeAll) NEVER touches the map.
// lookupByUuid() is ONLY called during init phase.
// ============================================================
class HardwareRegistry {
public:
    void addBackend(std::unique_ptr<IHardwareAdapter> adapter);
    void buildUuidMap();
    void freezeForRt();

    [[nodiscard]] PDOEntry*       lookupByUuid(std::string_view uuid) noexcept;
    [[nodiscard]] const PDOEntry* lookupByUuid(std::string_view uuid) const noexcept;

    // RT cycle (noexcept)
    void readAll() noexcept;
    void writeAll() noexcept;

private:
    std::vector<std::unique_ptr<IHardwareAdapter>> backends_;
    std::unordered_map<std::string, PDOEntry*>     uuidMap_;
};

} // namespace fc::pdo
