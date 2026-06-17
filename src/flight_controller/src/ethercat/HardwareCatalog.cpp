#include "fc/ethercat/HardwareCatalog.h"

#include <fstream>
#include <random>
#include <iomanip>
#include <sstream>

namespace fc::pdo {

// ---------------------------------------------------------------------------
// UUID generation (simple RFC-4122 v4 implementation)
// ---------------------------------------------------------------------------
std::string HardwareCatalog::generateUuid() noexcept
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    uint64_t a = dist(gen);
    uint64_t b = dist(gen);

    // Set version 4 and variant bits
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << static_cast<uint32_t>(a >> 32)
        << "-" << std::setw(4) << static_cast<uint16_t>((a >> 16) & 0xFFFF)
        << "-" << std::setw(4) << static_cast<uint16_t>(a & 0xFFFF)
        << "-" << std::setw(4) << static_cast<uint16_t>((b >> 48) & 0xFFFF)
        << "-" << std::setw(12) << static_cast<uint64_t>(b & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

// ---------------------------------------------------------------------------
// Load / Save
// ---------------------------------------------------------------------------
bool HardwareCatalog::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return false;

    nlohmann::json j;
    try {
        f >> j;
        for (auto& e : j["channels"]) {
            CatalogEntry entry;
            entry.key              = e.value("key", std::string{});
            entry.uuid             = e.value("uuid", std::string{});
            entry.channelType      = e.value("channelType", std::string{});
            entry.name             = e.value("name", std::string{});
            entry.slaveName        = e.value("slaveName", std::string{});
            entry.slavePos         = static_cast<uint16_t>(e.value("slavePos", 0));
            entry.productCode      = static_cast<uint32_t>(e.value("productCode", 0u));
            entry.revisionNumber   = static_cast<uint32_t>(e.value("revisionNumber", 0u));
            entry.pdoIndex         = static_cast<uint16_t>(e.value("pdoIndex", 0));
            entry.pdoSubindex      = static_cast<uint8_t>(e.value("pdoSubindex", 0));
            entry.isOutput         = e.value("isOutput", false);
            entries_.push_back(std::move(entry));
        }
        // Rebuild key index
        keyIndex_.clear();
        for (size_t i = 0; i < entries_.size(); ++i) {
            keyIndex_[entries_[i].key] = i;
        }
    } catch (...) {
        return false;
    }
    return true;
}

void HardwareCatalog::save(const std::string& path) const
{
    nlohmann::json j;
    for (const auto& e : entries_) {
        j["channels"].push_back({
            {"key", e.key},
            {"uuid", e.uuid},
            {"channelType", e.channelType},
            {"name", e.name},
            {"slaveName", e.slaveName},
            {"slavePos", e.slavePos},
            {"productCode", e.productCode},
            {"revisionNumber", e.revisionNumber},
            {"pdoIndex", e.pdoIndex},
            {"pdoSubindex", e.pdoSubindex},
            {"isOutput", e.isOutput},
        });
    }
    std::ofstream f(path);
    f << j.dump(2);
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
const CatalogEntry* HardwareCatalog::findByKey(const std::string& key) const noexcept
{
    auto it = keyIndex_.find(key);
    if (it != keyIndex_.end()) {
        return &entries_[it->second];
    }
    return nullptr;
}

const CatalogEntry* HardwareCatalog::findByUuid(const std::string& uuid) const noexcept
{
    for (const auto& e : entries_) {
        if (e.uuid == uuid) {
            return &e;
        }
    }
    return nullptr;
}

const CatalogEntry* HardwareCatalog::find(uint32_t vendorId, uint32_t productCode) const noexcept
{
    // Backward compatibility: search for EtherCAT entries by vendorId + productCode
    for (const auto& e : entries_) {
        if (e.productCode == productCode && e.key.rfind("EC|", 0) == 0) {
            // Parse vendorId from key (format: EC|{vendor_id:08X}|...)
            if (e.key.size() > 12) {
                std::string vendorStr = e.key.substr(3, 8);
                uint32_t vid = 0;
                for (char c : vendorStr) {
                    vid = (vid << 4) | (c < '0' ? (c < 'A' ? 0 : ((c < 'a') ? (c - 'A' + 10) : (c - 'a' + 10))) : (c - '0'));
                }
                if (vid == vendorId) {
                    return &e;
                }
            }
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void HardwareCatalog::addEntry(CatalogEntry entry)
{
    // Check if key already exists — update UUID if so
    auto it = keyIndex_.find(entry.key);
    if (it != keyIndex_.end()) {
        // Key exists — preserve existing UUID
        if (entry.uuid.empty()) {
            entry.uuid = entries_[it->second].uuid;
        }
        entries_[it->second] = std::move(entry);
    } else {
        // New key — generate UUID if not provided
        if (entry.uuid.empty()) {
            entry.uuid = generateUuid();
        }
        keyIndex_[entry.key] = entries_.size();
        entries_.push_back(std::move(entry));
    }
}

std::string HardwareCatalog::getOrCreateUuid(const std::string& key) noexcept
{
    auto it = keyIndex_.find(key);
    if (it != keyIndex_.end()) {
        return entries_[it->second].uuid;
    }
    return generateUuid();
}

} // namespace fc::pdo
