#pragma once
#include "fc/pdo/IHardwareAdapter.h"
#include "fc/ethercat/HardwareCatalog.h"
#include <vector>
#include <atomic>
#include <cstdint>

// Guard IgH EtherCAT headers — only available when libethercat is installed.
#ifdef ETHERCAT_AVAILABLE
extern "C" {
#include <ecrt.h>
}
#endif

namespace fc::ethercat {

struct Config;

// ---- EthercatAdapter ---------------------------------------------------------
// Owns one EtherCAT domain (one PDO in pdos_[0]).
// onBeforeReadInputs()  — receive + domain_process  → fills domainData_ buffer.
// onAfterWriteOutputs() — domain_queue + send       → flushes domainData_ buffer.

class EthercatAdapter final : public fc::pdo::IHardwareAdapter {
public:
    explicit EthercatAdapter(uint32_t cycleNs = 1'000'000u) noexcept
        : cycleNs_(cycleNs) {}

    ~EthercatAdapter() override {
#ifdef ETHERCAT_AVAILABLE
        if (master_) ecrt_release_master(master_);
#endif
    }

    void setCatalog(fc::pdo::HardwareCatalog* catalog) noexcept { catalog_ = catalog; }
    void setConfig(const Config* config) noexcept { config_ = config; }

    bool initialize() override;
    void onBeforeReadInputs()  noexcept override;
    void onAfterWriteOutputs() noexcept override;

    [[nodiscard]] bool     isAvailable()          const noexcept { return master_ != nullptr; }
    [[nodiscard]] bool     isFullyCommunicating() const noexcept {
#ifdef ETHERCAT_AVAILABLE
        return lastDomainState_.wc_state == EC_WC_COMPLETE;
#else
        return lastDomainState_.wc_state == 0;
#endif
    }
    [[nodiscard]] int      slaveCount()           const noexcept { return nSlaves_; }
    [[nodiscard]] uint16_t workingCounter()       const noexcept {
        return lastDomainState_.working_counter;
    }
    [[nodiscard]] uint64_t cycleCount()           const noexcept {
        return cycleCount_.load(std::memory_order_relaxed);
    }

    bool waitForCommunication(uint32_t timeoutMs = 5000u);

private:
#ifdef ETHERCAT_AVAILABLE
    ec_master_t*      master_{nullptr};
    ec_domain_t*      domain_{nullptr};
    uint8_t*          domainData_{nullptr};
    ec_domain_state_t lastDomainState_{};
#else
    void*             master_{nullptr};
    void*             domain_{nullptr};
    uint8_t*          domainData_{nullptr};
    struct { uint16_t wc_state{0}; uint16_t working_counter{0}; } lastDomainState_{};
#endif
    uint32_t          cycleNs_;
    fc::pdo::HardwareCatalog*  catalog_{nullptr};
    const Config*     config_{nullptr};

    int               nSlaves_{0};
    std::atomic<uint64_t> cycleCount_{0u};

    bool discoverSlaves();
    void buildEntries();
    void applyConfig();
};

} // namespace fc::ethercat
