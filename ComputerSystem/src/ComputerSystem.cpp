#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <stdbool.h>
#include "../../DataTypes/aircraft_data.h"

// Shared Mem Ptr to aircrafts
AircraftData* aircrafts_shared_memory;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

void sendAlert(int aircraft1, int aircraft2) {
    std::cout << "DEBUG Alert: Aircraft " << aircraft1 <<
        " and Aircraft " << aircraft2 << " are too close" << std::endl;
    // TODO: we'll fix this later where we send it to the operator
}

void sendIdToDisplay(int aircraft_id) {
    std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
    // TODO: fix later to send ID to display
}

void* violationCheck(void* arg) {
    while (1) {
        sleep(5);
        pthread_mutex_lock(&shm_mutex);

        std::cout << "\n\nChecking for Safety Violations..." << std::endl;
        std::cout << "[DEBUG] Shared Memory Base Address: " << aircrafts_shared_memory << std::endl;

        for (int i = 0; i < MAX_AIRCRAFT; i++) {
            AircraftData* aircraft1 = &aircrafts_shared_memory[i];

            std::cout << "Found Aircraft ID: " << aircraft1->id
                                  << " at Memory Address: " << aircraft1 << std::endl;

            if (aircraft1->id == 0) continue;

            // Compare against all other aircraft
            for (int j = i + 1; j < MAX_AIRCRAFT; j++) {
                AircraftData* aircraft2 = &aircrafts_shared_memory[j];

                if (aircraft2->id == 0) continue; // Skip empty slots

                double dx = std::fabs(aircraft1->x - aircraft2->x);
                double dy = std::fabs(aircraft1->y - aircraft2->y);
                double dz = std::fabs(aircraft1->z - aircraft2->z);

                std::cout << "Comparing: " << aircraft1->id << " with " << aircraft2->id << std::endl;

                // Euclidean horizontal distance
                double horizontalXYDiff = std::sqrt(dx * dx + dy * dy);

                if (horizontalXYDiff < 3000 || dz < 1000) {
                    sendAlert(aircraft1->id, aircraft2->id);
                }
            }
        }

        pthread_mutex_unlock(&shm_mutex);
    }
}



int main() {
    // From tutorial 7: HR reader
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return 1;
    }

    aircrafts_shared_memory = (AircraftData*) mmap(NULL,
                                    sizeof(AircraftData) * MAX_AIRCRAFT,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    shm_fd, 0);

    if (aircrafts_shared_memory == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // Create a monitor thread
    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);
    pthread_join(monitorThread, NULL);

    // Unmap the shared memory (but not unlink it)
    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd);

    // Tutorial 7 has this unlink thing, but I think we should only call this at the end in Radar Subsystem?
    // I think because we should only unlink once at the end of the program to delete shared mem
    // if (shm_unlink(SHM_NAME) == -1) { perror("can't unlink"); exit(1); }
}
