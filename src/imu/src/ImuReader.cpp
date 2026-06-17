#include "imu/ImuReader.h"

namespace imu {

ImuReader::ImuReader(void* accXEntry, void* accYEntry, void* accZEntry,
                     void* gyroXEntry, void* gyroYEntry, void* gyroZEntry,
                     const ImuCalibration& cal)
    : accXEntry_(accXEntry)
    , accYEntry_(accYEntry)
    , accZEntry_(accZEntry)
    , gyroXEntry_(gyroXEntry)
    , gyroYEntry_(gyroYEntry)
    , gyroZEntry_(gyroZEntry)
    , cal_(cal)
{
}

ImuRaw ImuReader::readRaw() const noexcept
{
    // Phase 1: Return zeros until PDOEntry integration is complete.
    // Phase 2: Read from PDOEntry::getRawAdc() for each axis.
    return ImuRaw{};
}

ImuCalibrated ImuReader::readCalibrated() const noexcept
{
    return cal_.calibrate(readRaw());
}

ImuHealth ImuReader::checkHealth() const noexcept
{
    return cal_.checkHealth(readCalibrated());
}

} // namespace imu
