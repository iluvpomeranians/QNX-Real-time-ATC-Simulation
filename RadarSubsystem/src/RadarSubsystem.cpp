#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <string>
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

// Function to read aircraft data from file and load into shared memory
void load_aircraft_data_from_file(const std::string& file_path) {
    std::cout << "Reading aircraft data from file: " << file_path << std::endl;

    // Open the file for reading
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_path << std::endl;
        return;
    }

    // Lock shared memory before writing
    pthread_mutex_lock(&shm_mutex);

    std::string line;

    // Read the file line by line
    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        AircraftData aircraft_data;
        AircraftData* aircraft = &aircraft_data;

        // Assuming the file contains data in the format: id, x, y, z, speedX, speedY, speedZ
        line_stream >> aircraft->id >>  aircraft->time >> aircraft->x >> aircraft->y >> aircraft->z
                    >> aircraft->speedX >> aircraft->speedY >> aircraft->speedZ;

        Aircraft* l_aircraft = new Aircraft(aircraft->time,aircraft->id,aircraft->x,aircraft->y,aircraft->z,aircraft->speedX,aircraft->speedY,aircraft->speedZ,aircrafts_shared_memory);
        l_aircraft->startThreads();
    }

    // Close the file
    file.close();

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
        		  << " Entry Time: " << aircraft->time
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

    // Load aircraft data from file into shared memory
    load_aircraft_data_from_file("/tmp/aircraft_data.txt");
    sleep(1);
    verify_aircraft_data();

    std::cout << "Radar System initialized. Shared memory ready.\n";

    // Use the shared memory limits to iterate over shared memory
    void* start = shm_limits_aircraft_data.lower_limit;
    size_t size = shm_limits_aircraft_data.size;

    AircraftData* aircraft_pointer = (AircraftData*)start;

    std::cout << "\nIterating through aircraft data using shared memory limits:\n";
    for (size_t i = 0; i < size / sizeof(AircraftData); i++) {
    	if(aircraft_pointer[i].id == 0) break;
        std::cout << "Aircraft ID: " << aircraft_pointer[i].id << std::endl;
    }

    while (true) sleep(10);

    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd_aircraft_data);

    return 0;
}
