#include "fc/ethercat/EthercatAdapter.h"
#include "fc/ethercat/HardwareCatalog.h"

#include <cstdio>
#include <thread>

namespace fc::ethercat {

bool EthercatAdapter::initialize()
{
#ifdef ETHERCAT_AVAILABLE
    master_ = ecrt_request_master(0);
    if (!master_) {
        std::fprintf(stderr, "[EthercatAdapter] Cannot request master 0\n");
        return false;
    }

    domain_ = ecrt_master_create_domain(master_);
    if (!domain_) {
        std::fprintf(stderr, "[EthercatAdapter] Cannot create domain\n");
        return false;
    }

    if (!ecrt_master_activate(master_)) {
        std::fprintf(stderr, "[EthercatAdapter] Cannot activate master\n");
        return false;
    }

    if (!discoverSlaves()) {
        return false;
    }

    buildEntries();
    applyConfig();

    ecrt_domain_activate(domain_);

    return waitForCommunication();
#else
    std::fprintf(stderr, "[EthercatAdapter] EtherCAT not available — stub mode\n");
    return false;
#endif
}

void EthercatAdapter::onBeforeReadInputs() noexcept
{
    if (!master_) return;
#ifdef ETHERCAT_AVAILABLE
    ecrt_master_receive(master_);
    ecrt_domain_process(domain_);
    lastDomainState_ = ecrt_domain_state(domain_);
#endif
}

void EthercatAdapter::onAfterWriteOutputs() noexcept
{
    if (!master_) return;
#ifdef ETHERCAT_AVAILABLE
    ecrt_domain_queue(domain_);
    ecrt_master_send(master_);
#endif
    ++cycleCount_;
}

bool EthercatAdapter::waitForCommunication(uint32_t timeoutMs)
{
#ifdef ETHERCAT_AVAILABLE
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        ecrt_master_receive(master_);
        ecrt_domain_process(domain_);
        lastDomainState_ = ecrt_domain_state(domain_);

        if (lastDomainState_.wc_state == EC_WC_COMPLETE) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    (void)timeoutMs;
#endif
    return false;
}

bool EthercatAdapter::discoverSlaves()
{
#ifdef ETHERCAT_AVAILABLE
    nSlaves_ = ecrt_master_slave_count(master_, EC_AL_STATE_SAFE_OP);
#endif
    return nSlaves_ > 0;
}

void EthercatAdapter::buildEntries()
{
    // Phase 1: Stub — will be populated from catalog + ESI XML parsing.
    // pdos_[0].entries populated from EcEntryReg records.
}

void EthercatAdapter::applyConfig()
{
    // Phase 1: Stub — apply pulseMs/debounceMs from Config to PDOEntry signal machines.
}

} // namespace fc::ethercat
