/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * High-resolution profiling class for bitbase generation.
 * Usage:
 *   auto& t = BitbaseProfiling::getStaticInstance();
 *   t.start("phase name");
 *   ... do work ...
 *   t.stop("phase name");
 *   t.printStatistics();  // prints accumulated totals across all calls
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <algorithm>

#define PROFILING_ENABLED 

namespace QaplaBitbase {

class BitbaseProfiling {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::nanoseconds;

    /**
     * Returns the single static instance that accumulates timings across all bitbases.
     */
    static BitbaseProfiling& getStaticInstance() {
        static BitbaseProfiling instance;
        return instance;
    }

    /**
     * Records the start of a named phase.
     * Thread-safe; multiple concurrent calls with the same name are permitted
     * (last start wins per-name — intended for single-threaded phase boundaries).
     */
    void start(const std::string& name) {
#ifdef PROFILING_ENABLED
        std::lock_guard<std::mutex> lock(_mutex);
        // Overwrite in place — never inserts after the first call per name,
        // so no heap allocation occurs in subsequent iterations.
        _startTimes[name] = Clock::now();
        // Ensure the entry exists in insertion order even if no stop follows.
        if (_order.find(name) == _order.end()) {
            _order[name] = static_cast<uint32_t>(_insertionOrder.size());
            _insertionOrder.push_back(name);
        }
#endif
    }

    /**
     * Records the end of a named phase and accumulates elapsed time.
     */
    void stop(const std::string& name) {
#ifdef PROFILING_ENABLED
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _startTimes.find(name);
        if (it != _startTimes.end()) {
            const auto elapsed = std::chrono::duration_cast<Duration>(now - it->second);
            _accumulated[name] += elapsed;
            ++_callCounts[name];
            // Do NOT erase — leave the node in place so subsequent start() calls
            // only overwrite the value (no heap alloc/dealloc per iteration).
            it->second = TimePoint{};
        }
#endif
    }

    /**
     * Resets all accumulated data (useful between independent generation runs).
     */
    void reset() {
#ifdef PROFILING_ENABLED
        std::lock_guard<std::mutex> lock(_mutex);
        _accumulated.clear();
        _callCounts.clear();
        _startTimes.clear();
        _order.clear();
        _insertionOrder.clear();
#endif
    }

    /**
     * Prints a formatted table of all accumulated phase timings, sorted by
     * total time descending.  Also shows call count and average per call.
     */
    void printStatistics() const {
#ifdef PROFILING_ENABLED
        std::lock_guard<std::mutex> lock(_mutex);
        if (_accumulated.empty()) return;

        // Build sortable list
        std::vector<std::pair<std::string, Duration>> entries;
        entries.reserve(_accumulated.size());
        for (const auto& [name, dur] : _accumulated) {
            entries.emplace_back(name, dur);
        }
        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Compute total
        Duration total{0};
        for (const auto& [name, dur] : entries) total += dur;
        const double totalMs = total.count() / 1e6;

        constexpr int W_NAME  = 32;
        constexpr int W_MS    = 12;
        constexpr int W_PCT   =  7;
        constexpr int W_CALLS =  8;
        constexpr int W_AVG   = 14;

        std::cout << "\n=== Bitbase Profiling Statistics ===\n";
        std::cout << std::left  << std::setw(W_NAME)  << "Phase"
                  << std::right << std::setw(W_MS)    << "Total (ms)"
                  << std::setw(W_PCT)                 << "%"
                  << std::setw(W_CALLS)               << "Calls"
                  << std::setw(W_AVG)                 << "Avg (mikros)"
                  << "\n"
                  << std::string(W_NAME + W_MS + W_PCT + W_CALLS + W_AVG, '-') << "\n";

        for (const auto& [name, dur] : entries) {
            const double ms  = dur.count() / 1e6;
            const uint32_t calls = _callCounts.at(name);
            const double avg_us  = dur.count() / 1e3 / calls;
            const double pct = totalMs > 0.0 ? ms * 100.0 / totalMs : 0.0;

            std::cout << std::left  << std::setw(W_NAME)  << name
                      << std::right << std::fixed << std::setprecision(1)
                      << std::setw(W_MS)   << ms
                      << std::setw(W_PCT)  << std::setprecision(1) << pct
                      << std::setw(W_CALLS) << calls
                      << std::setw(W_AVG)  << std::setprecision(1) << avg_us
                      << "\n";
        }

        std::cout << std::string(W_NAME + W_MS + W_PCT + W_CALLS + W_AVG, '-') << "\n";
        std::cout << std::left  << std::setw(W_NAME) << "TOTAL (sum of phases)"
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(W_MS) << totalMs
                  << "\n";
        std::cout << "(Note: nested phases are counted in both parent and child entries)\n"
                  << "====================================\n";
#endif
    }

private:
    BitbaseProfiling() = default;

    mutable std::mutex _mutex;
    std::unordered_map<std::string, Duration>  _accumulated;
    std::unordered_map<std::string, uint32_t>  _callCounts;
    std::unordered_map<std::string, TimePoint> _startTimes;

    // Insertion-order tracking (not used in the sorted output but available).
    std::unordered_map<std::string, uint32_t>  _order;
    std::vector<std::string>                   _insertionOrder;
};

} // namespace QaplaBitbase
