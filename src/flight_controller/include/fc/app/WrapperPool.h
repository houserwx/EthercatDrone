#pragma once

#include "fc/wrapper/DigitalInputWrapper.h"
#include "fc/wrapper/DigitalOutputWrapper.h"
#include "fc/wrapper/AnalogInputWrapper.h"
#include "fc/wrapper/AnalogOutputWrapper.h"
#include "fc/wrapper/MessageWrappers.h"
#include "fc/wrapper/IMUWrapper.h"
#include "fc/wrapper/MagnetometerWrapper.h"
#include "fc/wrapper/BarometerWrapper.h"
#include "fc/wrapper/GPSWrapper.h"

#include <string>
#include <string_view>
#include <vector>

// Forward declaration — avoids pulling in full MachineStateController.h here.
class MachineStateController;

// ============================================================================
// WrapperPool — per-segment hardware vocabulary, owned by a Queue.
//
// RT DESIGN PATTERN: Direct Member Access, No Inheritance, No Virtual Dispatch
//
// This class provides direct accessors to wrapper members instead of hiding
// them behind getter/setter methods, for RT determinism:
//
//   1. GUARANTEED INLINE: Trivial accessor compiles to zero instructions.
//   2. ZERO ABSTRACTION COST: `pool.output(idx)` is as cheap as direct member.
//   3. CONST-CORRECT: Returns const& where appropriate, mutable& where needed.
//   4. NOEXCEPT: All accessors are noexcept (guaranteed no exceptions in RT).
//   5. NO VIRTUAL DISPATCH: Each wrapper is a standalone concrete final class.
//
// Ownership model:
//   One WrapperPool per Queue.  Multiple function entries may share the
//   same output/input index.  The HardwareRegistry must outlive the pool.
//
// Lifecycle:
//   1. Construct with segment name and expected counts.
//   2. Call addOutput / addInput / addIMU etc. for every device — returns
//      stable int index for FunctionState.  No addEncoder / addDetect.
//   3. Call freeze() — shrink_to_fit on all vectors; no allocation after.
//   4. Pass WrapperPool& to FunctionEvaluator::tick() and RT path.
//
// RT safety:
//   All accessor methods are noexcept.  No allocation, no virtual dispatch,
//   no locks after freeze().
// ============================================================================

namespace fc::app {

class WrapperPool final {
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    WrapperPool(std::string_view segmentName,
                int expectedOutputCount  = 8,
                int expectedInputCount   = 4,
                int expectedAnalogInCount = 0,
                int expectedAnalogOutCount = 0,
                int expectedIMUCount     = 0,
                int expectedMagCount     = 0,
                int expectedBaroCount    = 0,
                int expectedGPSCount     = 0)
        : name_(segmentName)
    {
        outputs_.reserve(static_cast<std::size_t>(expectedOutputCount));
        inputs_.reserve(static_cast<std::size_t>(expectedInputCount));
        analogInputs_.reserve(static_cast<std::size_t>(expectedAnalogInCount));
        analogOutputs_.reserve(static_cast<std::size_t>(expectedAnalogOutCount));
        msgOuts_.reserve(4);
        msgIns_.reserve(4);
        imus_.reserve(static_cast<std::size_t>(expectedIMUCount));
        magnetometers_.reserve(static_cast<std::size_t>(expectedMagCount));
        barometers_.reserve(static_cast<std::size_t>(expectedBaroCount));
        gps_.reserve(static_cast<std::size_t>(expectedGPSCount));
    }

    // -----------------------------------------------------------------------
    // Registration — call at init time, before freeze().
    // Returns the integer index to store in FunctionState.
    // All returned indices are stable after freeze().
    // -----------------------------------------------------------------------

    /// Register a digital output device. Returns stable index.
    [[nodiscard]] int addOutput(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(outputs_.size());
        outputs_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register a digital input device. Returns stable index.
    [[nodiscard]] int addInput(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(inputs_.size());
        inputs_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register an analog input device. Returns stable index.
    [[nodiscard]] int addAnalogInput(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(analogInputs_.size());
        analogInputs_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register an analog output device. Returns stable index.
    [[nodiscard]] int addAnalogOutput(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(analogOutputs_.size());
        analogOutputs_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register a message output channel (GrpcWrapper MessageOut entry).
    [[nodiscard]] int addMsgOut(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(msgOuts_.size());
        msgOuts_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register a message input channel (GrpcWrapper MessageIn entry).
    [[nodiscard]] int addMsgIn(std::string name, dynamichardware::pdo::PDOEntry& entry)
    {
        const int idx = static_cast<int>(msgIns_.size());
        msgIns_.emplace_back(std::move(name), entry);
        return idx;
    }

    /// Register an IMU wrapper. Returns stable index.
    [[nodiscard]] int addIMU(std::string name,
                             dynamichardware::pdo::PDOEntry& gyroX, dynamichardware::pdo::PDOEntry& gyroY, dynamichardware::pdo::PDOEntry& gyroZ,
                             dynamichardware::pdo::PDOEntry& accelX, dynamichardware::pdo::PDOEntry& accelY, dynamichardware::pdo::PDOEntry& accelZ,
                             const imu::ImuCalibration& cal)
    {
        const int idx = static_cast<int>(imus_.size());
        imus_.emplace_back(std::move(name), gyroX, gyroY, gyroZ, accelX, accelY, accelZ, cal);
        return idx;
    }

    /// Register a magnetometer wrapper. Returns stable index.
    [[nodiscard]] int addMagnetometer(std::string name,
                                      dynamichardware::pdo::PDOEntry& magX, dynamichardware::pdo::PDOEntry& magY, dynamichardware::pdo::PDOEntry& magZ)
    {
        const int idx = static_cast<int>(magnetometers_.size());
        magnetometers_.emplace_back(std::move(name), magX, magY, magZ);
        return idx;
    }

    /// Register a barometer wrapper. Returns stable index.
    [[nodiscard]] int addBarometer(std::string name, dynamichardware::pdo::PDOEntry& pressure)
    {
        const int idx = static_cast<int>(barometers_.size());
        barometers_.emplace_back(std::move(name), pressure);
        return idx;
    }

    /// Register a GPS wrapper. Returns stable index.
    [[nodiscard]] int addGPS(std::string name,
                             dynamichardware::pdo::PDOEntry& latitude, dynamichardware::pdo::PDOEntry& longitude,
                             dynamichardware::pdo::PDOEntry& altitude, dynamichardware::pdo::PDOEntry& heading,
                             dynamichardware::pdo::PDOEntry& fixQuality)
    {
        const int idx = static_cast<int>(gps_.size());
        gps_.emplace_back(std::move(name), latitude, longitude, altitude, heading, fixQuality);
        return idx;
    }

    // -----------------------------------------------------------------------
    // freeze() — called after all wrappers have been registered and
    // before the RT thread starts.  Releases excess capacity.  No allocation
    // or vector modification is permitted after this call.
    // -----------------------------------------------------------------------
    void freeze()
    {
        outputs_.shrink_to_fit();
        inputs_.shrink_to_fit();
        analogInputs_.shrink_to_fit();
        analogOutputs_.shrink_to_fit();
        msgOuts_.shrink_to_fit();
        msgIns_.shrink_to_fit();
        imus_.shrink_to_fit();
        magnetometers_.shrink_to_fit();
        barometers_.shrink_to_fit();
        gps_.shrink_to_fit();
        frozen_ = true;
    }

    [[nodiscard]] bool isFrozen() const noexcept { return frozen_; }

    // -----------------------------------------------------------------------
    // Identity
    // -----------------------------------------------------------------------
    [[nodiscard]] std::string_view name() const noexcept { return name_; }

    // -----------------------------------------------------------------------
    // N-instance RT accessors — O(1) direct index, noexcept.
    // UB if index is out of bounds; caller is responsible for valid indices.
    // -----------------------------------------------------------------------
    [[nodiscard]]       fc::wrapper::DigitalOutputWrapper& output       (int idx)       noexcept { return outputs_      [static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::DigitalInputWrapper&  input        (int idx) const noexcept { return inputs_      [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::AnalogOutputWrapper&  analogOutput (int idx)       noexcept { return analogOutputs_[static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::AnalogInputWrapper&   analogInput  (int idx) const noexcept { return analogInputs_ [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::MessageOutWrapper&    msgOut       (int idx)       noexcept { return msgOuts_    [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::MessageInWrapper&     msgIn        (int idx)       noexcept { return msgIns_     [static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::MessageInWrapper&     msgIn        (int idx) const noexcept { return msgIns_     [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::IMUWrapper&           imu          (int idx)       noexcept { return imus_       [static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::IMUWrapper&           imu          (int idx) const noexcept { return imus_       [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::MagnetometerWrapper&  magnetometer (int idx)       noexcept { return magnetometers_[static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::MagnetometerWrapper&  magnetometer (int idx) const noexcept { return magnetometers_[static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::BarometerWrapper&     barometer    (int idx)       noexcept { return barometers_ [static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::BarometerWrapper&     barometer    (int idx) const noexcept { return barometers_ [static_cast<std::size_t>(idx)]; }
    [[nodiscard]]       fc::wrapper::GPSWrapper&           gps          (int idx)       noexcept { return gps_        [static_cast<std::size_t>(idx)]; }
    [[nodiscard]] const fc::wrapper::GPSWrapper&           gps          (int idx) const noexcept { return gps_        [static_cast<std::size_t>(idx)]; }

    // -----------------------------------------------------------------------
    // Size queries (init/diagnostic use only)
    // -----------------------------------------------------------------------
    [[nodiscard]] int outputCount()       const noexcept { return static_cast<int>(outputs_.size()); }
    [[nodiscard]] int inputCount()        const noexcept { return static_cast<int>(inputs_.size());  }
    [[nodiscard]] int analogOutputCount() const noexcept { return static_cast<int>(analogOutputs_.size()); }
    [[nodiscard]] int analogInputCount()  const noexcept { return static_cast<int>(analogInputs_.size());  }
    [[nodiscard]] int msgOutCount()       const noexcept { return static_cast<int>(msgOuts_.size()); }
    [[nodiscard]] int msgInCount()        const noexcept { return static_cast<int>(msgIns_.size());  }
    [[nodiscard]] int imuCount()          const noexcept { return static_cast<int>(imus_.size());    }
    [[nodiscard]] int magnetometerCount() const noexcept { return static_cast<int>(magnetometers_.size()); }
    [[nodiscard]] int barometerCount()    const noexcept { return static_cast<int>(barometers_.size()); }
    [[nodiscard]] int gpsCount()          const noexcept { return static_cast<int>(gps_.size()); }

    // -----------------------------------------------------------------------
    // Machine state controller — injected by Application::addQueue() after
    // Queue::loadFromJson().  May be nullptr during unit tests or init phase.
    // -----------------------------------------------------------------------
    void setStateMachine(MachineStateController& msc) noexcept { stateMachine_ = &msc; }
    [[nodiscard]] MachineStateController* stateMachine() const noexcept { return stateMachine_; }

    // -----------------------------------------------------------------------
    // Safe state — RT-safe; called when halt/E-Stop active.
    // Iterates all outputs and sets them inactive.
    // -----------------------------------------------------------------------
    void safeStateAllOutputs() noexcept
    {
        for (auto& out : outputs_) {
            out.setActive(false);
        }
        for (auto& out : analogOutputs_) {
            out.setValue(0);
        }
    }

private:
    std::string  name_;  // segment identity (e.g. "Drone-Primary")

    // N-instance — registered at init, frozen before RT:
    std::vector<fc::wrapper::DigitalOutputWrapper>  outputs_;
    std::vector<fc::wrapper::DigitalInputWrapper>   inputs_;
    std::vector<fc::wrapper::AnalogOutputWrapper>   analogOutputs_;
    std::vector<fc::wrapper::AnalogInputWrapper>    analogInputs_;
    std::vector<fc::wrapper::MessageOutWrapper>     msgOuts_;
    std::vector<fc::wrapper::MessageInWrapper>      msgIns_;
    std::vector<fc::wrapper::IMUWrapper>            imus_;
    std::vector<fc::wrapper::MagnetometerWrapper>   magnetometers_;
    std::vector<fc::wrapper::BarometerWrapper>      barometers_;
    std::vector<fc::wrapper::GPSWrapper>            gps_;

    // Machine state controller — injected post-construction.
    MachineStateController* stateMachine_{nullptr};

    bool frozen_{false};
};

} // namespace fc::app
