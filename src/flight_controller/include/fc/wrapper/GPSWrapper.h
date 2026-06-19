#pragma once

#include "dynamichardware/pdo/PDO.h"
#include "common/math/Math.h"

#include <cstdint>
#include <string>

// ============================================================
// GPSWrapper — typed accessor for GPS/GNSS PDOEntries.
//
// Holds stable PDOEntry& references resolved once at init time.
// In the RT loop each accessor is a single struct member read — zero lookup,
// zero virtual dispatch.
//
// GPS devices sit on the other side of a backend (UART NMEA,
// SPI, or I2C). The backend reads raw data and writes into
// the PDO image; this wrapper provides typed access.
//
// Lifetime: the referenced PDOEntries live in the frozen PDO owned
// by a backend.  Backends outlive all wrappers.
// ============================================================

namespace fc::wrapper {

class GPSWrapper final {
public:
    /// Construct with raw PDOEntry references (resolved at init time).
    /// @param name        Human-readable identifier (e.g. "GPS-Primary")
    /// @param latitude    PDOEntry for latitude (degrees, float)
    /// @param longitude   PDOEntry for longitude (degrees, float)
    /// @param altitude    PDOEntry for altitude (m MSL, float)
    /// @param heading     PDOEntry for heading (degrees true, float)
    /// @param fixQuality  PDOEntry for fix quality (0=none, 1=GPS, 2=DGPS, int16)
    GPSWrapper(std::string name,
               dynamichardware::pdo::PDOEntry& latitude, dynamichardware::pdo::PDOEntry& longitude,
               dynamichardware::pdo::PDOEntry& altitude, dynamichardware::pdo::PDOEntry& heading,
               dynamichardware::pdo::PDOEntry& fixQuality) noexcept
        : name_(std::move(name))
        , latitude_(latitude), longitude_(longitude)
        , altitude_(altitude), heading_(heading)
        , fixQuality_(fixQuality)
    {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /// Current position as Vec3f (latitude, longitude, 0).
    /// RT-safe: noexcept, no allocation.
    [[nodiscard]] common::math::Vec3f position() const noexcept;

    /// Current altitude (m MSL).
    [[nodiscard]] float altitude() const noexcept;

    /// Current heading (degrees true, -180 to 180).
    [[nodiscard]] float heading() const noexcept;

    /// Fix quality: 0=none, 1=GPS fix, 2=DGPS fix, 3=PPPS fix.
    [[nodiscard]] uint8_t fixQuality() const noexcept;

    /// True if we have at least a GPS fix (quality >= 1).
    [[nodiscard]] bool hasFix() const noexcept;

    /// True if we have DGPS or better (quality >= 2).
    [[nodiscard]] bool hasDGPS() const noexcept;

private:
    std::string name_;

    // PDOEntry references (resolved at init, stable after freeze).
    dynamichardware::pdo::PDOEntry& latitude_;
    dynamichardware::pdo::PDOEntry& longitude_;
    dynamichardware::pdo::PDOEntry& altitude_;
    dynamichardware::pdo::PDOEntry& heading_;
    dynamichardware::pdo::PDOEntry& fixQuality_;
};

// ---------------------------------------------------------------------------
// Inline implementations for maximum inlining in RT path
// ---------------------------------------------------------------------------
inline common::math::Vec3f GPSWrapper::position() const noexcept
{
    return common::math::Vec3f{
        latitude_.getFloat(),
        longitude_.getFloat(),
        0.0f
    };
}

inline float GPSWrapper::altitude() const noexcept
{
    return altitude_.getFloat();
}

inline float GPSWrapper::heading() const noexcept
{
    return heading_.getFloat();
}

inline uint8_t GPSWrapper::fixQuality() const noexcept
{
    return static_cast<uint8_t>(fixQuality_.getInt16());
}

inline bool GPSWrapper::hasFix() const noexcept
{
    return fixQuality() >= 1;
}

inline bool GPSWrapper::hasDGPS() const noexcept
{
    return fixQuality() >= 2;
}

} // namespace fc::wrapper
