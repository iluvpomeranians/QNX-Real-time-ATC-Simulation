#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <stdbool.h>
#include "../../DataTypes/aircraft_data.h"

// Shared memory pointer and mutex for synchronization
AircraftData* aircrafts_shared_memory;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to store shared memory limits
struct SharedMemoryLimits {
    void* lower_limit;  // Pointer to the start of shared memory
    size_t size;        // Size of the shared memory
};

SharedMemoryLimits shm_limits_aircraft_data;

// Function to send alert in case of proximity violation
void sendAlert(int aircraft1, int aircraft2) {
    std::cout << "DEBUG Alert: Aircraft " << aircraft1 <<
                 " and Aircraft " << aircraft2 << " are too close" << std::endl;
    // TODO: Implement alert mechanism (e.g., notify operator)
}

// Function to send aircraft ID to display
void sendIdToDisplay(int aircraft_id) {
    std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
    // TODO: Implement display mechanism (e.g., show ID on screen)
}

// Thread function to check for violations
void* violationCheck(void* arg) {
    // Calculate the upper limit for shared memory
    void* upper_limit = (void*)((char*)shm_limits_aircraft_data.lower_limit + shm_limits_aircraft_data.size);

    while (1) {
        sleep(5);  // Sleep for 5 seconds
        pthread_mutex_lock(&shm_mutex);  // Lock shared memory before accessing

        std::cout << "\n\nChecking for Safety Violations..." << std::endl;
        std::cout << "[DEBUG] Shared Memory Base Address: " << shm_limits_aircraft_data.lower_limit << std::endl;

        // Pointer to the first aircraft data entry
        AircraftData* current_aircraft = (AircraftData*) shm_limits_aircraft_data.lower_limit;

        // Iterate through shared memory using pointer arithmetic
        while ((void*)current_aircraft < upper_limit) {
            // Check if the current aircraft pointer is valid
            if (current_aircraft->id == 0) {
                current_aircraft++;  // Move to next aircraft
                continue;  // Skip empty or invalid aircraft data
            }

            // Compare the current aircraft with others
            for (AircraftData* next_aircraft = current_aircraft + 1; (void*)next_aircraft < upper_limit; next_aircraft++) {
                if (next_aircraft->id == 0) continue;  // Skip empty or invalid aircraft data

                // Calculate the distance between the two aircraft
                double dx = std::fabs(current_aircraft->x - next_aircraft->x);
                double dy = std::fabs(current_aircraft->y - next_aircraft->y);
                double dz = std::fabs(current_aircraft->z - next_aircraft->z);

                std::cout << "Comparing: " << current_aircraft->id << " (" << current_aircraft->x << "," << current_aircraft->y << "," << current_aircraft->z << ")"
                          << " with " << next_aircraft->id << " (" << next_aircraft->x << "," << next_aircraft->y << "," << next_aircraft->z << ")" << std::endl;

                // Calculate horizontal distance
                double horizontalXYDiff = std::sqrt(dx * dx + dy * dy);

                // Check for proximity violation
                if (horizontalXYDiff < 3000 || dz < 1000) {
                    sendAlert(current_aircraft->id, next_aircraft->id);
                }
            }

            current_aircraft++;  // Move to next aircraft
        }

        pthread_mutex_unlock(&shm_mutex);  // Unlock shared memory after access
    }
}

int main() {
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return 1;
    }

    // Map the shared memory
    aircrafts_shared_memory = (AircraftData*) mmap(NULL,
                                                   sizeof(AircraftData) * MAX_AIRCRAFT,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED,
                                                   shm_fd, 0);

    if (aircrafts_shared_memory == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // Initialize shared memory limits
    shm_limits_aircraft_data.lower_limit = aircrafts_shared_memory;
    shm_limits_aircraft_data.size = sizeof(AircraftData) * MAX_AIRCRAFT;

    std::cout << "[ComputerSystem] Shared Memory Base Address: " << aircrafts_shared_memory << " with params: " << SHM_NAME << ", " << shm_fd << std::endl;

    // Create a monitor thread for checking violations
    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);

    while (true) sleep(10);  // Main thread loops indefinitely

    // Unmap and close shared memory before exit
    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd);

    // Tutorial 7 has this unlink thing, but I think we should only call this at the end in Radar Subsystem?
    // I think because we should only unlink once at the end of the program to delete shared mem
    // if (shm_unlink(SHM_NAME) == -1) { perror("can't unlink"); exit(1); }

    return 0;
}
