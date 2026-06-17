#pragma once
#include "fc/pdo/PDO.h"
#include "common/math/Math.h"

#include <string>

// ============================================================
// MagnetometerWrapper — typed accessor for 3× PDOEntries
// representing magnetometer X/Y/Z channels.
//
// Holds stable PDOEntry& references resolved once at init time.
// RT-safe: all accessors are noexcept, no allocation.
// ============================================================

namespace fc::wrapper {

class MagnetometerWrapper final {
public:
    MagnetometerWrapper(std::string name,
                        PDOEntry& magXEntry, PDOEntry& magYEntry, PDOEntry& magZEntry) noexcept
        : name_(std::move(name))
        , magX_(magXEntry), magY_(magYEntry), magZ_(magZEntry)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Read magnetometer data (micro-Tesla).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] common::math::Vec3f read() const noexcept
    {
        return common::math::Vec3f{magX_.getMagX(), magY_.getMagY(), magZ_.getMagZ()};
    }

    /// Individual axis accessors.
    [[nodiscard]] float x() const noexcept { return magX_.getMagX(); }
    [[nodiscard]] float y() const noexcept { return magY_.getMagY(); }
    [[nodiscard]] float z() const noexcept { return magZ_.getMagZ(); }

private:
    std::string name_;
    PDOEntry& magX_;
    PDOEntry& magY_;
    PDOEntry& magZ_;
};

} // namespace fc::wrapper
