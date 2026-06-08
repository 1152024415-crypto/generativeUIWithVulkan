#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <chrono>
#include <string>
#include "logger_common.h"

/**
 * RAII scope timer. Prints elapsed time on destruction.
 * Controlled by g_perfEnabled.
 *
 * Usage:
 *   {
 *       PerfTimer _("AgenUI:ProbeJSON");
 *       ... work ...
 *   }  // <- prints "[AgenUI_PERF] AgenUI:ProbeJSON 0.42ms"
 *
 * Search keyword: "AgenUI_PERF"
 */
class PerfTimer {
public:
    explicit PerfTimer(const char* label)
        : m_label(label), m_start(std::chrono::steady_clock::now()) {}

    ~PerfTimer() {
        if (!g_perfEnabled) return;
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - m_start).count();
        LOGE("[AgenUI_PERF] %s %.2fms", m_label, ms);
    }

    PerfTimer(const PerfTimer&) = delete;
    PerfTimer& operator=(const PerfTimer&) = delete;

private:
    const char* m_label;
    std::chrono::steady_clock::time_point m_start;
};

#endif // PERF_TIMER_H
