#pragma once
#include "fc/pdo/PDO.h"

#include <string>

// ============================================================
// BarometerWrapper — typed accessor for barometer PDOEntries
// representing pressure and altitude.
//
// Holds stable PDOEntry& references resolved once at init time.
// RT-safe: all accessors are noexcept, no allocation.
// ============================================================

namespace fc::wrapper {

class BarometerWrapper final {
public:
    BarometerWrapper(std::string name, PDOEntry& pressureEntry) noexcept
        : name_(std::move(name))
        , pressure_(pressureEntry)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Read pressure (hPa).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] float pressure() const noexcept { return pressure_.getBaroPressure(); }

    /// Read altitude (meters, derived from pressure).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] float altitude() const noexcept { return pressure_.getBaroAltitude(); }

private:
    std::string name_;
    PDOEntry& pressure_;
};

} // namespace fc::wrapper
