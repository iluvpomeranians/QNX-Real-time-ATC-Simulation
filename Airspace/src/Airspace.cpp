#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
// #include <pthread.h>
//#include <sys/dispatch.h>
#include "Airspace.h"
#include "../../DataTypes/aircraft.h"

using namespace std;

#define AIRSPACE_SHM_NAME "/airspace_shm"

void load_aircraft_data_from_file(const std::string &file_path) {
    std::cout << "Reading aircraft data from file: " << file_path << std::endl;

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_path << std::endl;
        exit(EXIT_FAILURE);
    }

    // pthread_mutex_lock(&shm_mutex);

    std::string line;

    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        AircraftData aircraft_data;
        AircraftData* aircraft = &aircraft_data;
        std::string temp_time_str;

        line_stream >> temp_time_str >> aircraft->id >> aircraft->x >> aircraft->y >> aircraft->z
                    >> aircraft->speedX >> aircraft->speedY >> aircraft->speedZ;

        // TODO: Uncomment
        // aircraft->entryTime = parseToTimeT(temp_time_str);
        // Aircraft* l_aircraft = new Aircraft(aircraft->entryTime,aircraft->id,aircraft->x,aircraft->y,aircraft->z,aircraft->speedX,aircraft->speedY,aircraft->speedZ,aircrafts_shared_memory);
        // l_aircraft->startThreads();
    }

    file.close();

    // pthread_mutex_unlock(&shm_mutex);
}

Airspace* init_shared_memory() {
	int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1) {
		perror("shm_open failed");
		exit(EXIT_FAILURE);
	}

	if (ftruncate(shm_fd, sizeof(Airspace)) == -1) {
	        perror("ftruncate failed for aircraft data");
	        return nullptr;
	}

	void *airspace_shared_memory = mmap(NULL, sizeof(Airspace),
										PROT_READ | PROT_WRITE, MAP_SHARED,
										shm_fd, 0);
	if (airspace_shared_memory == MAP_FAILED) {
		perror("mmap failed");
		exit(EXIT_FAILURE);
	}

	Airspace *airspace = static_cast<Airspace*>(airspace_shared_memory);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&airspace->lock, &attr);

	// Initialize the shared memory to 0x0
	memset(airspace, 0, sizeof(Airspace));
	return airspace;
}

int main() {

	Airspace *airspace = init_shared_memory();


	cout << "Hello World!!!" << endl; // prints Hello World!!!
	return 0;
}
