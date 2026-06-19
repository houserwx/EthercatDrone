#pragma once
#include "dynamichardware/pdo/PDO.h"
#include "common/math/Math.h"

#include <string>

// ============================================================
// MagnetometerWrapper — typed accessor for 3 PDOEntries
// representing magnetometer X/Y/Z channels.
//
// Domain semantics (axis mapping, units) live here,
// not in the hardware library.
// Holds stable PDOEntry& references resolved once at init time.
// RT-safe: all accessors are noexcept, no allocation.
// ============================================================

namespace fc::wrapper {

class MagnetometerWrapper final {
public:
    MagnetometerWrapper(std::string name,
                        dynamichardware::pdo::PDOEntry& magXEntry, dynamichardware::pdo::PDOEntry& magYEntry, dynamichardware::pdo::PDOEntry& magZEntry) noexcept
        : name_(std::move(name))
        , magX_(magXEntry), magY_(magYEntry), magZ_(magZEntry)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Read magnetometer data (micro-Tesla, stored as float in PDOEntry).
    [[nodiscard]] common::math::Vec3f read() const noexcept
    {
        return common::math::Vec3f{magX_.getFloat(), magY_.getFloat(), magZ_.getFloat()};
    }

    /// Individual axis accessors.
    [[nodiscard]] float x() const noexcept { return magX_.getFloat(); }
    [[nodiscard]] float y() const noexcept { return magY_.getFloat(); }
    [[nodiscard]] float z() const noexcept { return magZ_.getFloat(); }

private:
    std::string name_;
    dynamichardware::pdo::PDOEntry& magX_;
    dynamichardware::pdo::PDOEntry& magY_;
    dynamichardware::pdo::PDOEntry& magZ_;
};

} // namespace fc::wrapper
