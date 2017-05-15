/*
 * npp_performance_profiler.cpp
 *
 *  Created on: Nov 29, 2016
 *      Author: asa
 */
#ifndef NPP_PERFORMANCE_PROFILER_CPP_
#define NPP_PERFORMANCE_PROFILER_CPP_

#include <chrono>
#include <string>
#include <vector>
#include "npp_log_utilities.cpp"
namespace npp_performance_profiler {


#ifdef PERFORMANCE_PROFILING
    static std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> time_start(0), time_end(0);
    static std::vector<std::string> labels(0);
    static uint16_t num_checkpoint = 0;
    inline uint16_t add_label(const std::string label) {
        std::chrono::high_resolution_clock::time_point empty_time_point = std::chrono::high_resolution_clock::now();
        labels.push_back(label);
        time_start.push_back(empty_time_point);
        time_end.push_back(empty_time_point);
        num_checkpoint++;
        log_utilities::performance("Assigning checkpoint %d to label '%s'",(labels.size() - 1),label.c_str());
        return (labels.size() - 1);
    }

    inline void start_checkpoint(const uint16_t checkpoint_idx) {
        time_start[checkpoint_idx] = std::chrono::high_resolution_clock::now();
    }

    inline void stop_checkpoint(const uint16_t checkpoint_idx) {

        time_end[checkpoint_idx] = std::chrono::high_resolution_clock::now();

    }

    inline void report_checkpoint(const uint16_t checkpoint_idx) {
        const double time_interval = std::chrono::duration_cast<std::chrono::microseconds>(time_end[checkpoint_idx] - time_start[checkpoint_idx]).count();
        log_utilities::performance(labels[checkpoint_idx] +": %f ms", time_interval/1000);
    }

    inline void report() {
        log_utilities::performance("Performance Report:");
        if (num_checkpoint != 0) {
            for (uint16_t checkpoint_idx = 0; checkpoint_idx < num_checkpoint; checkpoint_idx++) {
                report_checkpoint(checkpoint_idx);
            }
        } else {
            log_utilities::warning("No performance checkpoint to report");
        }
    }

#else

    inline uint16_t add_label(const std::string label) {
        return (0);
    }

    inline void start_checkpoint(const uint16_t checkpoint_idx) {

    }

    inline void stop_checkpoint(const uint16_t checkpoint_idx) {

    }

    inline void report() {

    }

    inline void report_checkpoint(const uint16_t checkpoint_idx) {

    }
#endif
}
#endif
