#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/aircraft.h"

Airspace* airspace = nullptr;
std::vector<Aircraft*> active_aircrafts;
int shm_fd_airspace;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t airspace_thread;

void* updateAirspaceDetectionThread(void* arg) {
	struct timespec req;
		    req.tv_sec = 1;         // 1 second
		    req.tv_nsec = 0;        // 0 nanoseconds

	while (true) {
		pthread_mutex_lock(&airspace->lock);

		// TODO: Changed the iterator here, validate that this works still
		for(int i = 0; i < airspace->aircraft_count; ++i) {
			AircraftData* aircraft = &airspace->aircraft_data[i];

			if (aircraft->x > 100000 || aircraft->y > 100000 ||
				aircraft->x < 0 || aircraft->y < 0 || aircraft->z > 25000 ||
				aircraft->z < 15000) {
				aircraft->detected = false;
			} else {
				aircraft->detected = true;
			}
		}
		pthread_mutex_unlock(&airspace->lock);
		nanosleep(&req, NULL);
	}
	return nullptr;
}

void cleanUpOnExit() {
	pthread_join(airspace_thread, nullptr);
}

int main() {
    std::cout << "Connecting to Airspace Shared Memory...\n";

    shm_fd_airspace = shm_open(AIRSPACE_SHM_NAME, O_RDWR, 0666);
    if (shm_fd_airspace == -1) {
        perror("shm_open failed for aircraft data");
        return 1;
    }

    airspace = (Airspace*)mmap(NULL, sizeof(Airspace), PROT_READ | PROT_WRITE,
                               MAP_SHARED, shm_fd_airspace, 0);

    if (airspace == MAP_FAILED) {
        perror("mmap failed for aircraft data");
        return 1;
    }

    std::cout << "[DEBUG] Shared Memory Base Address for Aircrafts: " << airspace
              << std::endl;

	pthread_create(&airspace_thread, nullptr, updateAirspaceDetectionThread, NULL);


    std::cout << "Radar System initialized. Shared memory ready.\n";

    while (true) sleep(10);

    munmap(airspace, sizeof(Airspace));
    close(shm_fd_airspace);

    cleanUpOnExit();

    return 0;
}
