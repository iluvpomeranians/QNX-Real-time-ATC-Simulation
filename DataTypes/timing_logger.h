#ifndef TIMING_LOGGER_H
#define TIMING_LOGGER_H

#include <ctime>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

class TimingLogger {
    std::string full_path;

    void ensureTimingDirExists() {
        struct stat st;
        if (stat("/tmp/timing", &st) == -1) {
            mkdir("/tmp/timing", 0777);
        }
    }

public:
    TimingLogger(const std::string& filename) {
        ensureTimingDirExists();
        full_path = "/tmp/timing/" + filename;

        std::ofstream clear(full_path, std::ios::trunc);
        clear.close();
    }

    void logDuration(const std::string& label, const timespec& start, const timespec& end) {
        std::ofstream out(full_path, std::ios::app);
        if (!out.is_open()) return;

        double elapsed = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;

        // No need to print wall time if you're using CLOCK_MONOTONIC
        out << label << " took "
            << std::fixed << std::setprecision(9) << elapsed << " sec\n";

        out.close();
    }

    timespec now() const {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts;
    }
};

#endif // TIMING_LOGGER_H
