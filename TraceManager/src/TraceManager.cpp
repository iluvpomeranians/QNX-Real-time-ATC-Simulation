#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

volatile sig_atomic_t stop_trace = 0;

void signal_handler(int signum) {
    stop_trace = 1;
}

int main() {
    // Set up signal handler
    signal(SIGINT, signal_handler);

    std::cout << "[TraceManager] Starting QNX system tracing..." << std::endl;

    // Start the trace using absolute path
    int start_status = system("which tracecontrol");
    if (start_status != 0) {
        std::cerr << "[TraceManager] Failed to start trace. Make sure tracecontrol is available.\n";
        return 1;
    }

    std::cout << "[TraceManager] Tracing... Press Ctrl+C to stop.\n";

    // Wait until the user sends Ctrl+C
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 500000000; // 500 ms

    while (!stop_trace) {
        nanosleep(&delay, NULL);
    }

    std::cout << "\n[TraceManager] Signal received. Stopping trace..." << std::endl;

    // Stop the trace
    system("/usr/bin/tracecontrol -off");

    std::cout << "[TraceManager] Trace stopped. Dumping trace to text..." << std::endl;

    // Dump the trace to a text file
    int dump_status = system("/usr/bin/traceprinter -o /tmp/trace_dump.txt");
    if (dump_status != 0) {
        std::cerr << "[TraceManager] Failed to dump trace. Make sure traceprinter is available.\n";
        return 1;
    }

    std::cout << "[TraceManager] Done. Trace saved to /tmp/trace_dump.txt\n";
    return 0;
}
