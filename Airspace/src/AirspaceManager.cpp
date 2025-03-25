#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
//#include <sys/dispatch.h>
#include <utility>
#include <vector>
#include <regex>
#include "../../DataTypes/aircraft.h"
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/airspace.h"

using namespace std;

#define AIRSPACE_SHM_NAME "/airspace_shm"

int shm_fd;
Airspace *airspace;

vector<std::pair<time_t, AircraftData>> aircraft_queue;

Airspace* init_shared_memory() {
	cout << "Initializing Shared Memory..." << endl;

	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1) {
		perror("shm_open failed");
		exit(EXIT_FAILURE);
	}

	if (ftruncate(shm_fd, sizeof(Airspace)) == -1) {
	        perror("ftruncate failed for aircraft data");
	        exit(EXIT_FAILURE);
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

time_t parseToTimeT(const std::string& input) {
    std::tm tm = {};
    time_t now = time(NULL);

    // epoch timestamp (digits only)
    if (std::regex_match(input, std::regex("^\\d{9,}$"))) {
        return static_cast<time_t>(std::stoll(input));
    }

    // "YYYY-MM-DD HH:MM:SS"
    if (strptime(input.c_str(), "%Y-%m-%d %H:%M:%S", &tm)) {
        return mktime(&tm);
    }

    // "YYYY-MM-DD"
    if (strptime(input.c_str(), "%Y-%m-%d", &tm)) {
        return mktime(&tm);
    }

    // "HH:MM:SS"
    if (strptime(input.c_str(), "%H:%M:%S", &tm)) {
        std::tm* now_tm = localtime(&now);
        tm.tm_year = now_tm->tm_year;
        tm.tm_mon = now_tm->tm_mon;
        tm.tm_mday = now_tm->tm_mday;
        return mktime(&tm);
    }

    // Relative seconds offset (e.g. "2", "15")
	if (std::regex_match(input, std::regex("^\\d+$"))) {
		int offset = std::stoi(input);
		return now + offset;
	}

    std::cerr << "Invalid time format: " << input << std::endl;
    return -1;
}

void load_aircraft_data_from_file(const std::string &file_path) {
    std::cout << "Reading aircraft data from file: " << file_path << std::endl;

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_path << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;

    while (std::getline(file, line)) {
        std::istringstream line_stream(line);
        AircraftData aircraft_data;
        std::string temp_time_str;

        line_stream >> temp_time_str >> aircraft_data.id >> aircraft_data.x >> aircraft_data.y >> aircraft_data.z
                    >> aircraft_data.speedX >> aircraft_data.speedY >> aircraft_data.speedZ;

        aircraft_data.entryTime = parseToTimeT(temp_time_str);
        aircraft_queue.emplace_back(aircraft_data.entryTime, aircraft_data);
        Aircraft* l_aircraft = new Aircraft(aircraft_data.entryTime,
											aircraft_data.id,
											aircraft_data.x,
											aircraft_data.y,
											aircraft_data.z,
											aircraft_data.speedX,
											aircraft_data.speedY,
											aircraft_data.speedZ,
											airspace);
        l_aircraft->startThreads();
        // l_aircraft->startThreads();
    }
    file.close();
}

void cleanup_shared_memory(const char* shm_name, int shm_fd, void* addr,
						   size_t size) {
	// Unmap shared memory from process address space
	if (munmap(addr, size) == 0) {
		cout << "Shared memory unmapped successfully." << endl;
	} else {
		perror("munmap failed");
	}

	// Close the shared mem file descriptor
	close(shm_fd);

	// Remove shared memory name from system
	shm_unlink(shm_name);
}

int main() {

	airspace = init_shared_memory();

	load_aircraft_data_from_file("/tmp/aircraft_data.txt");

	// Sort the aircraft in queue according to entry time

	cout << "Press Enter to start airspace simulation..." << endl;
	cin.get();

	// Start timer

	// Need to create threads here for this

	// Pop first airplane, set interrupt at entryTime

	// handle interrupt

	// Create Aircraft instance and start thread

	// sleep

	// Create funtion to unmap and unlink shared memory
	cleanup_shared_memory(AIRSPACE_SHM_NAME, shm_fd, (void *)airspace,
						  sizeof(Airspace));
	return 0;
}
