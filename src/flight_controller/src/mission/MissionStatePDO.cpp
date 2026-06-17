#include "fc/mission/MissionStatePDO.h"

#include <cstring>

namespace fc::mission {

MissionStatePDO::MissionStatePDO(std::vector<uint8_t>     image,
                                 std::vector<MissionFlagEntry> flagEntries,
                                 std::vector<fc::pdo::PDOEntry> entries) noexcept
    : image_(std::move(image))
    , flagEntries_(std::move(flagEntries))
{
    // Create single PDO with all flag entries
    fc::pdo::PDO pdo;
    pdo.entries = std::move(entries);
    pdo.image = std::move(image_);

    // Set image pointers into each entry
    for (std::size_t i = 0; i < pdo.entries.size(); ++i) {
        pdo.entries[i].image = pdo.image.data() + i;
    }

    pdos_.push_back(std::move(pdo));
}

void MissionStatePDO::onBeforeReadInputs() noexcept
{
    // Project mission state flags into PDO image
    for (const auto& entry : flagEntries_) {
        if (entry.source && entry.imageSlot) {
            *entry.imageSlot = *entry.source ? 1 : 0;
        }
    }
}

} // namespace fc::mission
