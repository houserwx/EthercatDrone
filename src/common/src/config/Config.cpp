#include "common/config/Config.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace common::config {

namespace {

PdoEntryDef parseEntryDef(const json& entry_j)
{
    PdoEntryDef def;
    def.name = entry_j.value("name", "");
    def.hwUuid = entry_j.value("hwUuid", "");
    def.channelType = entry_j.value("channelType", "");
    def.pulseMs = entry_j.value("pulseMs", 0);
    def.debounceMs = entry_j.value("debounceMs", 0);
    def.pin = entry_j.value("pin", -1);

    if (entry_j.contains("sim")) {
        const auto& sim_j = entry_j["sim"];
        def.sim.rpm = sim_j.value("rpm", 0.0f);
        def.sim.rollerDiamMm = sim_j.value("rollerDiamMm", 0.0f);
        def.sim.resolutionPpr = sim_j.value("resolutionPpr", 0);
        def.sim.quadrature = sim_j.value("quadrature", false);
        def.sim.partsPerMin = sim_j.value("partsPerMin", 0.0f);
        def.sim.partWidthMm = sim_j.value("partWidthMm", 0.0f);
        def.sim.variancePercent = sim_j.value("variancePercent", 0.0f);
    }

    return def;
}

} // anonymous namespace

Config Config::loadFromJson(const std::string& path)
{
    Config cfg;
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    json j = json::parse(ifs);

    cfg.cycleTimeUs = j.value("cycleTimeUs", cfg.cycleTimeUs);
    cfg.demoCycles = j.value("demoCycles", cfg.demoCycles);
    cfg.hardwareCatalogPath = j.value("hardwareCatalogPath", cfg.hardwareCatalogPath);

    // Prefer flat pdoEntries array if present
    if (j.contains("pdoEntries")) {
        for (const auto& entry_j : j["pdoEntries"]) {
            cfg.pdoEntries.push_back(parseEntryDef(entry_j));
        }
    }

    // Also merge grouped hardwareConfiguration (if no flat pdoEntries yet, or supplement)
    if (j.contains("hardwareConfiguration")) {
        const auto& hc = j["hardwareConfiguration"];
        bool addedAny = false;

        auto mergeCategory = [&](const char* key, const std::string& channelType) {
            if (!hc.contains(key)) return;
            for (const auto& entry_j : hc[key]) {
                PdoEntryDef def = parseEntryDef(entry_j);
                if (def.channelType.empty()) def.channelType = channelType;
                cfg.pdoEntries.push_back(std::move(def));
                addedAny = true;
            }
        };

        if (!j.contains("pdoEntries")) {
            mergeCategory("encoders", "Encoder");
            mergeCategory("digitalInputs", "DigitalInput");
            mergeCategory("digitalOutputs", "DigitalOutput");
            mergeCategory("analogInputs", "AnalogInput");
            mergeCategory("analogOutputs", "AnalogOutput");
        }
    }

    return cfg;
}

bool Config::saveToJson(const std::string& path) const
{
    json j;
    j["cycleTimeUs"]             = cycleTimeUs;
    j["demoCycles"]              = demoCycles;
    j["hardwareCatalogPath"]     = hardwareCatalogPath;

    // Group entries by category for the hardwareConfiguration block
    json encoders, digitalInputs, digitalOutputs, analogInputs, analogOutputs;

    for (const auto& e : pdoEntries) {
        json ej;
        ej["name"]        = e.name;
        ej["hwUuid"]      = e.hwUuid;
        ej["channelType"] = e.channelType;
        if (e.pulseMs > 0)    ej["pulseMs"]    = e.pulseMs;
        if (e.debounceMs > 0) ej["debounceMs"] = e.debounceMs;
        if (e.pin >= 0)       ej["pin"]        = e.pin;

        // Write sim params if any non-default value is set
        const auto& s = e.sim;
        if (s.rpm != 0.0f || s.rollerDiamMm != 0.0f || s.resolutionPpr != 0 ||
            s.quadrature || s.partsPerMin != 0.0f || s.partWidthMm != 0.0f ||
            s.variancePercent != 0.0f) {
            json sj;
            sj["rpm"]             = s.rpm;
            sj["rollerDiamMm"]    = s.rollerDiamMm;
            sj["resolutionPpr"]   = s.resolutionPpr;
            sj["quadrature"]      = s.quadrature;
            sj["partsPerMin"]     = s.partsPerMin;
            sj["partWidthMm"]     = s.partWidthMm;
            sj["variancePercent"] = s.variancePercent;
            ej["sim"] = sj;
        }

        const auto& ct = e.channelType;
        if (ct == "Encoder")        encoders.push_back(ej);
        else if (ct == "DigitalInput")   digitalInputs.push_back(ej);
        else if (ct == "DigitalOutput")  digitalOutputs.push_back(ej);
        else if (ct == "AnalogInput")    analogInputs.push_back(ej);
        else if (ct == "AnalogOutput")   analogOutputs.push_back(ej);
        else                             digitalInputs.push_back(ej); // fallback
    }

    json hwCfg;
    hwCfg["encoders"]       = encoders;
    hwCfg["digitalInputs"]  = digitalInputs;
    hwCfg["digitalOutputs"] = digitalOutputs;
    hwCfg["analogInputs"]   = analogInputs;
    hwCfg["analogOutputs"]  = analogOutputs;
    j["hardwareConfiguration"] = hwCfg;

    // Also write flat pdoEntries for backward compat with loadFromJson
    // Write pdoEntries manually (PdoEntryDef has no NLOHMANN_DEFINE_TYPE)
    json pdoArr = json::array();
    for (const auto& e : pdoEntries) {
        json ej;
        ej["name"]        = e.name;
        ej["hwUuid"]      = e.hwUuid;
        ej["channelType"] = e.channelType;
        ej["pulseMs"]     = e.pulseMs;
        ej["debounceMs"]  = e.debounceMs;
        ej["pin"]         = e.pin;
        json sj;
        sj["rpm"]             = e.sim.rpm;
        sj["rollerDiamMm"]    = e.sim.rollerDiamMm;
        sj["resolutionPpr"]   = e.sim.resolutionPpr;
        sj["quadrature"]      = e.sim.quadrature;
        sj["partsPerMin"]     = e.sim.partsPerMin;
        sj["partWidthMm"]     = e.sim.partWidthMm;
        sj["variancePercent"] = e.sim.variancePercent;
        ej["sim"] = sj;
        pdoArr.push_back(ej);
    }
    j["pdoEntries"] = pdoArr;

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << j.dump(2) << std::endl;
    return ofs.good();
}

} // namespace common::config
