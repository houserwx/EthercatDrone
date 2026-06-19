#pragma once
#include "dynamichardware/pdo/PDO.h"

#include <string>

// ============================================================
// BarometerWrapper — typed accessor for barometer PDOEntries
// representing pressure and altitude.
//
// Domain semantics (pressure→altitude conversion) live here,
// not in the hardware library.
// Holds stable PDOEntry& references resolved once at init time.
// RT-safe: all accessors are noexcept, no allocation.
// ============================================================

namespace fc::wrapper {

class BarometerWrapper final {
public:
    BarometerWrapper(std::string name, dynamichardware::pdo::PDOEntry& pressureEntry) noexcept
        : name_(std::move(name))
        , pressure_(pressureEntry)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Read pressure (hPa, stored as float in PDOEntry).
    [[nodiscard]] float pressure() const noexcept { return pressure_.getFloat(); }

    /// Read altitude (meters, derived from pressure using barometric formula).
    [[nodiscard]] float altitude() const noexcept {
        const float p0 = 1013.25f;  // Sea-level standard pressure (hPa)
        const float L  = 0.0065f;   // Temperature lapse rate (K/m)
        const float T0 = 288.15f;   // Sea-level standard temperature (K)
        const float g  = 9.80665f;  // Gravity (m/s^2)
        const float R  = 287.05f;   // Gas constant for dry air (J/(kg·K))

        const float ratio = pressure() / p0;
        if (ratio <= 0.0f) return 0.0f;
        return T0 / L * (1.0f - std::pow(ratio, R * L / (g * T0)));
    }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& pressure_;
};

} // namespace fc::wrapper
