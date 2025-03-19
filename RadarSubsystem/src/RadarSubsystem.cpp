#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/aircraft.h"

AircraftData* aircrafts_shared_memory = nullptr;
int shm_fd_aircraft_data;

pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex to synchronize access to shared memory

// Structure to store the shared memory limits
struct SharedMemoryLimits {
    void* lower_limit;  // Pointer to the start of shared memory
    size_t size;        // Size of the shared memory
};

SharedMemoryLimits shm_limits_aircraft_data;

void fake_aircraft_data() {
    std::cout << "Initializing fake aircraft data...\n";

    AircraftData fakeData[MAX_AIRCRAFT] = {
        {101, 10000, 20000, 15000, 250, 300, 0, true, true},
        {102, 12000, 22000, 17000, -200, -250, 0, true, true},
        {103, 9000, 18000, 13000, 300, 200, 50, true, true},
        {104, 15000, 25000, 16000, 100, 150, -30, true, true},
        {105, 8000, 19000, 14000, 275, -225, 20, true, true},
        {106, 11000, 23000, 15500, 125, -175, 35, true, true},
        {107, 9500, 21000, 14500, 400, 350, -40, true, true},
        {108, 13000, 24000, 16500, 50, -100, 15, true, true},
        {109, 14000, 26000, 17500, -350, -275, 10, true, true},
        {110, 10500, 22000, 15800, 200, 300, -50, true, true}
    };

    // Lock shared memory before writing
    pthread_mutex_lock(&shm_mutex);

    // Write the aircraft data
    for (int i = 0; i < 10; i++) {
        aircrafts_shared_memory[i] = fakeData[i];
    }

    // Unlock shared memory after writing
    pthread_mutex_unlock(&shm_mutex);
}

void verify_aircraft_data() {
    std::cout << "Verifying shared memory contents...\n";

    // Lock shared memory before reading
    pthread_mutex_lock(&shm_mutex);

    // Verify Aircraft Data
    for (int i = 0; i < MAX_AIRCRAFT; i++) {
        AircraftData* aircraft = &aircrafts_shared_memory[i];

        if (aircraft->id == 0) continue;

        std::cout << "Aircraft ID: " << aircraft->id
                  << " Position: (" << aircraft->x << ", " << aircraft->y << ", " << aircraft->z << ")"
                  << " Speed: (" << aircraft->speedX << ", " << aircraft->speedY << ", " << aircraft->speedZ << ")"
                  << " Stored at: " << &aircrafts_shared_memory[i] << std::endl;
    }

    // Unlock shared memory after reading
    pthread_mutex_unlock(&shm_mutex);
}

int main() {
    std::cout << "Initializing Shared Memory...\n";

    // Create shared memory for aircraft data
    shm_fd_aircraft_data = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd_aircraft_data == -1) {
        perror("shm_open failed for aircraft data");
        return 1;
    }

    if (ftruncate(shm_fd_aircraft_data, sizeof(AircraftData) * MAX_AIRCRAFT) == -1) {
        perror("ftruncate failed for aircraft data");
        return 1;
    }

    aircrafts_shared_memory = (AircraftData*) mmap(NULL,
                                                   sizeof(AircraftData) * MAX_AIRCRAFT,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED,
                                                   shm_fd_aircraft_data, 0);

    if (aircrafts_shared_memory == MAP_FAILED) {
        perror("mmap failed for aircraft data");
        return 1;
    }

    // Store the shared memory limits for aircraft data
    shm_limits_aircraft_data.lower_limit = aircrafts_shared_memory;
    shm_limits_aircraft_data.size = sizeof(AircraftData) * MAX_AIRCRAFT;

    std::cout << "[DEBUG] Shared Memory Base Address for Aircrafts: " << aircrafts_shared_memory
              << std::endl;

    fake_aircraft_data();
    sleep(1);
    verify_aircraft_data();

    std::cout << "Radar System initialized. Shared memory ready.\n";

    // Use the shared memory limits to iterate over shared memory
    void* start = shm_limits_aircraft_data.lower_limit;
    size_t size = shm_limits_aircraft_data.size;

    AircraftData* aircraft_pointer = (AircraftData*)start;

    std::cout << "\nIterating through aircraft data using shared memory limits:\n";
    for (size_t i = 0; i < size / sizeof(AircraftData); i++) {
        std::cout << "Aircraft ID: " << aircraft_pointer[i].id << std::endl;
    }

    while (true) sleep(10);

    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd_aircraft_data);

    return 0;
}
