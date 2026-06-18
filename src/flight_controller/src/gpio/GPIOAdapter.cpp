#include "fc/gpio/GPIOAdapter.h"
#include "fc/gpio/BoardVariant.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Try to include libgpiod — fall back gracefully if unavailable
#ifdef GPIO_LIBGPIOD_AVAILABLE
    // libgpiod v2 API (newer, Pi 5)
    #ifdef LIBGPIOD_V2
    #include <gpiod.h>
    #define HAS_LIBGPIOD 2
    #else
    // libgpiod v1 API (older, Pi 4)
    #include <gpiod.h>
    #define HAS_LIBGPIOD 1
    #endif
#endif

#ifndef HAS_LIBGPIOD
#define HAS_LIBGPIOD 0
#endif

namespace fc::gpio {

// ---------------------------------------------------------------------------
// Constructor — auto-detect board variant
// ---------------------------------------------------------------------------
GPIOAdapter::GPIOAdapter()
{
    variant_ = detectBoardVariant();
    chipPath_ = gpioChipPath(variant_);
}

GPIOAdapter::GPIOAdapter(BoardVariant variant, std::string chipPath)
    : variant_(variant), chipPath_(std::move(chipPath))
{
}

// ---------------------------------------------------------------------------
// Initialize — open chip, request lines, create PDOs
// ---------------------------------------------------------------------------
bool GPIOAdapter::initialize()
{
    if (lines_.empty()) {
        std::fprintf(stderr, "[GPIOAdapter] No GPIO lines registered\n");
        return false;
    }

    // Detect board variant if still unknown
    if (variant_ == BoardVariant::UNKNOWN) {
        variant_ = detectBoardVariant();
        if (variant_ != BoardVariant::UNKNOWN) {
            chipPath_ = gpioChipPath(variant_);
        }
    }

#if HAS_LIBGPIOD
    // Try to open real GPIO chip
    if (!openChip()) {
        std::fprintf(stderr, "[GPIOAdapter] Cannot open %s — falling back to stub mode\n",
                     chipPath_.c_str());
        stubMode_ = true;
    }
#else
    std::fprintf(stderr, "[GPIOAdapter] libgpiod not available — stub mode\n");
    stubMode_ = true;
#endif

    if (stubMode_) {
        // Initialize stub states
        stubStates_.resize(lines_.size());
        for (size_t i = 0; i < lines_.size(); ++i) {
            stubStates_[i].value = lines_[i].initialVal;
            stubStates_[i].toggleCycle = 0;
        }
    } else {
        // Request each line from the GPIO chip
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (!requestLine(lines_[i], i)) {
                std::fprintf(stderr, "[GPIOAdapter] Cannot request line %u (%s) — will skip\n",
                             lines_[i].offset, lines_[i].name.c_str());
            }
        }
    }

    // Create PDOs for GPIO lines (single PDO with all lines)
    {
        fc::pdo::PDO pdo;
        for (auto& line : lines_) {
            if (line.entry) {
                pdo.entries.push_back(*line.entry);
            }
        }

        if (!pdo.entries.empty()) {
            // Allocate image buffer (1 byte per entry for digital I/O)
            pdo.image.resize(pdo.entries.size());

            // Set image pointers into each entry
            for (size_t i = 0; i < pdo.entries.size(); ++i) {
                pdo.entries[i].image = pdo.image.data() + i;
            }

            pdos_.push_back(std::move(pdo));
        }
    }

    if (variant_ != BoardVariant::UNKNOWN) {
        std::printf("[GPIOAdapter] %s — %zu lines (%s)\n",
                    boardVariantName(variant_).c_str(),
                    lines_.size(),
                    stubMode_ ? "stub mode" : "libgpiod");
    } else {
        std::printf("[GPIOAdapter] Unknown board — %zu lines (%s)\n",
                    lines_.size(),
                    stubMode_ ? "stub mode" : "libgpiod");
    }

    return true;
}

// ---------------------------------------------------------------------------
// RT cycle: read input lines
// ---------------------------------------------------------------------------
void GPIOAdapter::onBeforeReadInputs() noexcept
{
    if (stubMode_) {
        // Stub: toggle inputs at a simulated rate for testing
        static uint64_t cycleCount = 0;
        ++cycleCount;

        for (size_t i = 0; i < lines_.size(); ++i) {
            if (lines_[i].direction == LineDirection::INPUT) {
                // Simple toggle every 20 cycles for simulation
                bool val = (cycleCount % 40) < 20;
                stubStates_[i].value = val;

                if (lines_[i].entry && lines_[i].entry->image) {
                    *(uint8_t*)lines_[i].entry->image = val ? 1 : 0;
                }
            }
        }
        return;
    }

    // Real hardware: read GPIO input lines
#if HAS_LIBGPIOD
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].direction != LineDirection::INPUT) continue;
        if (!handles_[i].gpiod_line) continue;

        int val = 0;
        #if HAS_LIBGPIOD == 2
        // libgpiod v2 API
        val = gpiod_line_get_value(handles_[i].gpiod_line);
        #else
        // libgpiod v1 API
        val = gpiod_get_line_value(static_cast<struct gpiod_line*>(handles_[i].gpiod_line));
        #endif

        if (lines_[i].entry && lines_[i].entry->image) {
            *(uint8_t*)lines_[i].entry->image = static_cast<uint8_t>(val != 0);
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// RT cycle: write output lines
// ---------------------------------------------------------------------------
void GPIOAdapter::onAfterWriteOutputs() noexcept
{
    if (stubMode_) {
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (lines_[i].direction != LineDirection::OUTPUT) continue;

            if (lines_[i].entry && lines_[i].entry->image) {
                stubStates_[i].value = (*(uint8_t*)lines_[i].entry->image) != 0;
            }
        }
        return;
    }

    // Real hardware: write GPIO output lines
#if HAS_LIBGPIOD
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].direction != LineDirection::OUTPUT) continue;
        if (!handles_[i].gpiod_line) continue;

        int val = 0;
        if (lines_[i].entry && lines_[i].entry->image) {
            val = (*(uint8_t*)lines_[i].entry->image) != 0;
        }

        #if HAS_LIBGPIOD == 2
        // libgpiod v2 API
        gpiod_line_set_value(handles_[i].gpiod_line, val);
        #else
        // libgpiod v1 API
        gpiod_set_line_value(static_cast<struct gpiod_line*>(handles_[i].gpiod_line), val);
        #endif
    }
#endif
}

// ---------------------------------------------------------------------------
// Register a GPIO line
// ---------------------------------------------------------------------------
int GPIOAdapter::registerLine(uint32_t gpio_offset,
                              LineDirection direction,
                              std::string name,
                              fc::pdo::EntryType entryType)
{
    GPIOLine line;
    line.offset = gpio_offset;
    line.direction = direction;
    line.name = std::move(name);

    // Create PDO entry for this line
    fc::pdo::PDOEntry entry;
    entry.type = entryType;
    entry.uuid = "GPIO|" + std::to_string(gpio_offset);
    line.entry = &entry;

    const int idx = static_cast<int>(lines_.size());
    lines_.push_back(std::move(line));

    // Pre-allocate handle stubs
    handles_.resize(lines_.size());
    return idx;
}

// ---------------------------------------------------------------------------
// Open GPIO chip
// ---------------------------------------------------------------------------
bool GPIOAdapter::openChip() noexcept
{
#if HAS_LIBGPIOD
    #if HAS_LIBGPIOD == 2
    // libgpiod v2 API
    struct gpiod_chip* chip = gpiod_chip_open(chipPath_.c_str());
    #else
    // libgpiod v1 API
    struct gpiod_chip* chip = gpiod_chip_open_by_path(chipPath_.c_str());
    #endif

    if (!chip) {
        return false;
    }

    // Store chip handle in first position (shared by all lines)
    handles_[0].gpiod_chip = chip;
    return true;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Request a single GPIO line
// ---------------------------------------------------------------------------
bool GPIOAdapter::requestLine(GPIOLine& line, size_t index) noexcept
{
#if HAS_LIBGPIOD
    if (!handles_[0].gpiod_chip) return false;

    // Configure consumer label and direction
    const char* consumer = "EtherCatDrone";
    unsigned int flags = 0;

    if (line.direction == LineDirection::OUTPUT) {
        #if HAS_LIBGPIOD == 2
        struct gpiod_line_request_config req_config = GPIOD_LINE_REQUEST_CONFIG_INITIALIZER;
        req_config.consumer = consumer;
        req_config.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;

        struct gpiod_line_settings* settings = gpiod_line_settings_alloc(1);
        if (!settings) return false;
        gpiod_line_settings_set_output_mode(settings, line.offset,
                                             line.initialVal ? GPIOD_LINE_SETTINGS_BIAS_PULL_UP : GPIOD_LINE_SETTINGS_BIAS_PULL_DOWN);

        struct gpiod_line_request* req = gpiod_chip_get_line_request(
            static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip),
            &req_config);
        if (!req) {
            gpiod_line_settings_free(settings);
            return false;
        }

        struct gpiod_line* l = gpiod_chip_get_line(
            static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip),
            line.offset);
        if (!l) {
            gpiod_line_request_put(req);
            gpiod_line_settings_free(settings);
            return false;
        }

        handles_[index].gpiod_line = l;
        gpiod_line_settings_free(settings);
        #else
        // libgpiod v1 API
        struct gpiod_line* l = gpiod_chip_get_line(
            static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip),
            line.offset);
        if (!l) return false;

        if (gpiod_line_request_output(l, consumer, line.initialVal ? 1 : 0) != 0) {
            return false;
        }

        handles_[index].gpiod_line = l;
        #endif
    } else {
        // Input line
        #if HAS_LIBGPIOD == 2
        struct gpiod_line_request_config req_config = GPIOD_LINE_REQUEST_CONFIG_INITIALIZER;
        req_config.consumer = consumer;
        req_config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;

        struct gpiod_line* l = gpiod_chip_get_line(
            static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip),
            line.offset);
        if (!l) return false;

        handles_[index].gpiod_line = l;
        #else
        // libgpiod v1 API
        struct gpiod_line* l = gpiod_chip_get_line(
            static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip),
            line.offset);
        if (!l) return false;

        if (gpiod_line_request_input(l, consumer) != 0) {
            return false;
        }

        handles_[index].gpiod_line = l;
        #endif
    }

    return true;
#else
    (void)line;
    (void)index;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Close GPIO chip
// ---------------------------------------------------------------------------
void GPIOAdapter::closeChip() noexcept
{
#if HAS_LIBGPIOD
    if (handles_.size() > 0 && handles_[0].gpiod_chip) {
        #if HAS_LIBGPIOD == 2
        gpiod_chip_close(static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip));
        #else
        gpiod_chip_close(static_cast<struct gpiod_chip*>(handles_[0].gpiod_chip));
        #endif
        handles_[0].gpiod_chip = nullptr;
    }
#endif
}

} // namespace fc::gpio
