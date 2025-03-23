#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <stdbool.h>
#include "../../DataTypes/aircraft_data.h"

AircraftData* aircrafts_shared_memory;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

#define FUTURE_OFFSET_SEC 120

struct ViolationArgs {
    AircraftData* shm_ptr;
    int total_aircraft;
    double elapsedTime;
};

struct SharedMemoryLimits {
    void* lower_limit;  // Pointer to the start of shared memory
    size_t size;        // Size of the shared memory
};

SharedMemoryLimits shm_limits_aircraft_data;

void sendAlert(int aircraft1, int aircraft2) {
	std::cout << "[ALERT]: Aircraft " << aircraft1
	              << " and Aircraft " << aircraft2 << " are too close"
	              << std::endl;
    // TODO: Implement alert mechanism (e.g., notify operator)
}

void sendIdToDisplay(int aircraft_id) {
    std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
    // TODO: Implement display mechanism (e.g., show ID on screen)
}

void getProjectedPosition(AircraftData& aircraft, double time) {
    aircraft.x += aircraft.speedX * time / 3600;
    aircraft.y += aircraft.speedY * time / 3600;
    aircraft.z += aircraft.speedZ * time / 3600;
}

void* checkCurrentViolations(void* args) {
    struct ViolationArgs* data = (struct ViolationArgs*) args;
    AircraftData* shm_ptr = data->shm_ptr;
    int max = data->total_aircraft;

    time_t now = time(NULL);

    pthread_mutex_lock(&shm_mutex);

    for (int i = 0; i < max; i++) {
        AircraftData* a1 = &shm_ptr[i];
        if (a1->id == 0 || now < a1->entryTime) continue;

        for (int j = i + 1; j < max; j++) {
            AircraftData* a2 = &shm_ptr[j];
            if (a2->id == 0 || now < a2->entryTime) continue;

            double dx = fabs(a1->x - a2->x);
            double dy = fabs(a1->y - a2->y);
            double dz = fabs(a1->z - a2->z);
            double horizontal = sqrt(dx * dx + dy * dy);

            std::cout << "[ComputerSystem] Comparing Aircraft " << a1->id << " and " << a2->id << "\n"
                                  << "    A1 Pos: (" << a1->x << ", " << a1->y << ", " << a1->z << ")\n"
                                  << "    A2 Pos: (" << a2->x << ", " << a2->y << ", " << a2->z << ")\n"
                                  << "    Horizontal: " << horizontal << "m, Vertical (Z): " << dz << "m\n";

            if (horizontal < 3000 || dz < 1000) {
                sendAlert(a1->id, a2->id);
            }
        }
    }

    pthread_mutex_unlock(&shm_mutex);
    return NULL;
}


void* checkFutureViolations(void* args) {
    struct ViolationArgs* data = (struct ViolationArgs*) args;
    AircraftData* shm_ptr = data->shm_ptr;
    int max = data->total_aircraft;

    time_t now = time(NULL);

    pthread_mutex_lock(&shm_mutex);

    for (int i = 0; i < max; i++) {
        AircraftData* a1 = &shm_ptr[i];

        if (a1->id == 0 || now < a1->entryTime) continue;

        for (int j = i + 1; j < max; j++) {
            AircraftData* a2 = &shm_ptr[j];
            if (a2->id == 0 || now < a2->entryTime) continue;

            // Project both aircraft 2 minutes ahead
            a1->x += a1->speedX * FUTURE_OFFSET_SEC / 3600;
            a1->y += a1->speedY * FUTURE_OFFSET_SEC / 3600;
            a1->z += a1->speedZ * FUTURE_OFFSET_SEC / 3600;

            a2->x += a2->speedX * FUTURE_OFFSET_SEC / 3600;
            a2->y += a2->speedY * FUTURE_OFFSET_SEC / 3600;
            a2->z += a2->speedZ * FUTURE_OFFSET_SEC / 3600;

            double dx = fabs(a1->x - a2->x);
            double dy = fabs(a1->y - a2->y);
            double dz = fabs(a1->z - a2->z);
            double horizontal = sqrt(dx * dx + dy * dy);

            std::cout << "[ComputerSystem] Comparing Aircraft " << a1->id << " and " << a2->id << "\n"
                      << "    A1 Pos: (" << a1->x << ", " << a1->y << ", " << a1->z << ")\n"
                      << "    A2 Pos: (" << a2->x << ", " << a2->y << ", " << a2->z << ")\n"
                      << "    Horizontal: " << horizontal << "m, Vertical (Z): " << dz << "m\n";


            if (horizontal < 3000 || dz < 1000) {
                sendAlert(a1->id, a2->id);
            }
        }
    }

    pthread_mutex_unlock(&shm_mutex);
    return NULL;
}


void* violationCheck(void* arg) {
    struct timespec ts = {5, 0};

    std::cout << "[ComputerSystem] Checking Violations...\n";

    while (1) {

        struct ViolationArgs args = {
            .shm_ptr = aircrafts_shared_memory,
            .total_aircraft = MAX_AIRCRAFT,
        };

        pthread_t currentThread, futureThread;

        pthread_create(&currentThread, NULL, checkCurrentViolations, &args);
        pthread_create(&futureThread, NULL, checkFutureViolations, &args);

        pthread_join(currentThread, NULL);
        pthread_join(futureThread, NULL);

        nanosleep(&ts, NULL);
    }

    return NULL;
}


int main() {
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

    // Initialize shared memory limits
    shm_limits_aircraft_data.lower_limit = aircrafts_shared_memory;
    shm_limits_aircraft_data.size = sizeof(AircraftData) * MAX_AIRCRAFT;

    std::cout << "[ComputerSystem] Shared Memory Base Address: " << aircrafts_shared_memory << " with params: " << SHM_NAME << ", " << shm_fd << std::endl;

    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);

    while (true) sleep(10);

    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd);

    return 0;
}
