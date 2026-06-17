#include "fc/pdo/HardwareRegistry.h"

namespace fc::pdo {

void HardwareRegistry::addBackend(std::unique_ptr<IHardwareAdapter> adapter)
{
    backends_.push_back(std::move(adapter));
}

void HardwareRegistry::buildUuidMap()
{
    uuidMap_.clear();
    for (const auto& backend : backends_) {
        for (auto& pdo : backend->getPDOs()) {
            for (auto& entry : pdo.entries) {
                if (!entry.uuid.empty()) {
                    uuidMap_[entry.uuid] = &entry;
                }
            }
        }
    }
}

void HardwareRegistry::freezeForRt()
{
    for (auto& backend : backends_) {
        for (auto& pdo : backend->getPDOs()) {
            pdo.freeze();
        }
    }
    // Rebuild map to include any backends added after buildUuidMap()
    buildUuidMap();
}

PDOEntry* HardwareRegistry::lookupByUuid(std::string_view uuid) noexcept
{
    auto it = uuidMap_.find(std::string(uuid));
    return (it != uuidMap_.end()) ? it->second : nullptr;
}

const PDOEntry* HardwareRegistry::lookupByUuid(std::string_view uuid) const noexcept
{
    auto it = uuidMap_.find(std::string(uuid));
    return (it != uuidMap_.end()) ? it->second : nullptr;
}

void HardwareRegistry::readAll() noexcept
{
    for (auto& backend : backends_) {
        backend->onBeforeReadInputs();
    }
    for (auto& backend : backends_) {
        for (auto& pdo : backend->getPDOs()) {
            for (auto& entry : pdo.entries) {
                entry.read();
            }
        }
    }
}

void HardwareRegistry::writeAll() noexcept
{
    for (auto& backend : backends_) {
        for (auto& pdo : backend->getPDOs()) {
            for (auto& entry : pdo.entries) {
                entry.write();
            }
        }
    }
    for (auto& backend : backends_) {
        backend->onAfterWriteOutputs();
    }
}

} // namespace fc::pdo
