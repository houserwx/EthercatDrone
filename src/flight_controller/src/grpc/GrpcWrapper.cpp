#include "fc/grpc/GrpcWrapper.h"

namespace fc::grpc {

GrpcWrapper::GrpcWrapper(std::string name)
    : name_(std::move(name))
{
}

void GrpcWrapper::reserve(std::size_t n)
{
    entries_.reserve(n * 2);
    channels_.reserve(n);
}

int GrpcWrapper::addChannel(std::string name, uint32_t simFailMod)
{
    const int idx = static_cast<int>(channels_.size());

    // MessageOut entry
    dynamichardware::pdo::PDOEntry outEntry;
    outEntry.type = dynamichardware::pdo::EntryType::MessageOut;
    outEntry.uuid = name_ + ".out." + std::to_string(idx);

    // MessageIn entry
    dynamichardware::pdo::PDOEntry inEntry;
    inEntry.type = dynamichardware::pdo::EntryType::MessageIn;
    inEntry.uuid = name_ + ".in." + std::to_string(idx);

    entries_.push_back(std::move(outEntry));
    entries_.push_back(std::move(inEntry));

    auto ch = std::make_unique<Channel>();
    ch->name = std::move(name);
    ch->simFailMod = simFailMod;
    channels_.push_back(std::move(ch));

    return idx;
}

dynamichardware::pdo::PDOEntry& GrpcWrapper::outEntry(int ch) noexcept
{
    return entries_[static_cast<std::size_t>(ch) * 2];
}

dynamichardware::pdo::PDOEntry& GrpcWrapper::inEntry(int ch) noexcept
{
    return entries_[static_cast<std::size_t>(ch) * 2 + 1];
}

void GrpcWrapper::flushInputs() noexcept
{
    // Pop results from resultIn queues, write to PDOEntry inbound message slots.
    // Call before ctx->readAll() so application logic sees fresh results.
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        auto& ch = channels_[i];
        dynamichardware::pdo::PDOEntry& inEntry = entries_[i * 2 + 1];

        GrpcResultMessage msg;
        if (ch->resultIn.tryPop(msg)) {
            inEntry.setInMessageRaw(&msg, sizeof(msg));
        }
    }
}

void GrpcWrapper::drainOutputs() noexcept
{
    // Read armed outbound messages from PDOEntry message slots.
    // Call after ctx->writeAll() so we drain what the RT thread armed this cycle.
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        auto& ch = channels_[i];
        dynamichardware::pdo::PDOEntry& outEntry = entries_[i * 2];

        GrpcTriggerMessage trig;
        if (outEntry.tryConsumeOutMessage(trig)) {
            if (ch->simFailMod > 0) {
                // Sim relay: synthesize result immediately
                ch->simSeq++;
                GrpcResultMessage result;
                result.seq = trig.seq;
                result.result = (ch->simSeq % ch->simFailMod == 0)
                    ? InspectionResult::Fail : InspectionResult::Pass;
                (void)ch->resultIn.tryPush(result);
            } else {
                // gRPC relay: push to triggerOut for service thread
                (void)ch->triggerOut.tryPush(trig);
            }
        }
    }
}

} // namespace fc::grpc
