#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <utility>
#include <vector>
#include <signal.h>
#include <regex>
#include "../../DataTypes/aircraft.h"
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/airspace.h"

using namespace std;

int shm_fd;
Airspace *airspace = nullptr;
vector<pair<time_t, AircraftData>> aircraft_queue;
vector<Aircraft*> active_aircrafts;

Airspace* init_shared_memory() {
	cout << "Initializing Shared Memory..." << endl;

	shm_fd = shm_open(AIRSPACE_SHM_NAME, O_CREAT | O_RDWR, 0666);
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

	cout << "Airspace shared memory initialized successfully...\n";
	return airspace;
}

time_t parseToTimeT(const string& input) {
    std::tm tm = {};
    time_t now = time(NULL);

    // epoch timestamp (digits only)
    if (regex_match(input, regex("^\\d{9,}$"))) {
        return static_cast<time_t>(stoll(input));
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
	if (regex_match(input, regex("^\\d+$"))) {
		int offset = stoi(input);
		return now + offset;
	}

    cerr << "Invalid time format: " << input << endl;
    return -1;
}

void load_aircraft_data_from_file(const string &file_path) {
    cout << "Reading aircraft data from file: " << file_path << endl;

    ifstream file(file_path);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file: " << file_path << endl;
        exit(EXIT_FAILURE);
    }

    string line;
    int aircraft_count = 0;

    while (getline(file, line)) {
        istringstream line_stream(line);
        AircraftData aircraft_data;
        string temp_time_str;

        line_stream >> temp_time_str >> aircraft_data.id >> aircraft_data.x
		            >> aircraft_data.y >> aircraft_data.z
					>> aircraft_data.speedX >> aircraft_data.speedY
					>> aircraft_data.speedZ;

        aircraft_data.entryTime = parseToTimeT(temp_time_str);
        aircraft_queue.emplace_back(aircraft_data.entryTime, aircraft_data);
        aircraft_count++;
    }

    // Update number of aircrafts
    pthread_mutex_lock(&airspace->lock);
    airspace->aircraft_count = aircraft_count;
    pthread_mutex_unlock(&airspace->lock);

    file.close();
}

void cleanup_shared_memory(const char* shm_name, int shm_fd, void* addr,
						   size_t size) {
	if (munmap(addr, size) == 0) {
		cout << "Shared memory unmapped successfully." << endl;
	} else {
		perror("munmap failed");
	}

	close(shm_fd);

	if (shm_unlink(shm_name) == 0) {
		std::cout << "Shared memory unlinked successfully." << std::endl;
	} else {
		perror("shm_unlink failed");
	}

}

void verify_aircraft_data() {
    cout << "Verifying shared memory contents...\n";

    pthread_mutex_lock(&airspace->lock);

    for (int i = 0; i < MAX_AIRCRAFT; i++) {
        AircraftData* aircraft = &airspace->aircraft_data[i];

        if (aircraft->id == 0) continue;

//        cout << "Aircraft ID: " << aircraft->id
//        		  << " Entry Time: " << aircraft->entryTime
//                  << " Position: (" << aircraft->x << ", " << aircraft->y << ", " << aircraft->z << ")"
//                  << " Speed: (" << aircraft->speedX << ", " << aircraft->speedY << ", " << aircraft->speedZ << ")"
//                  << " Stored at: " << &airspace->aircraft_data[i] << endl;

    }

    pthread_mutex_unlock(&airspace->lock);
}

void spawn_aircrafts_by_time() {
    std::cout << "[Airspace Manager] Starting timed aircraft injection...\n";
    size_t nextAircraftIndex = 0;

    while (nextAircraftIndex < aircraft_queue.size()) {
        time_t currentTime = time(nullptr);

        // Check if it's time to inject the next aircraft
        if (currentTime >= aircraft_queue[nextAircraftIndex].first) {
            const AircraftData& data = aircraft_queue[nextAircraftIndex].second;

//            std::cout << "[AirspaceManager] Injecting aircraft ID: " << data.id
//                      << " at time: " << currentTime << std::endl;

            Aircraft* a = new Aircraft(data.entryTime,
                                       data.id,
                                       data.x,
                                       data.y,
                                       data.z,
                                       data.speedX,
                                       data.speedY,
                                       data.speedZ,
                                       airspace);
            active_aircrafts.push_back(a);
            a->startThreads();

            nextAircraftIndex++;
        }

        usleep(100000);
    }

    std::cout << "[Airspace Manager] All aircrafts injected.\n";
}


// TODO: Temporary function to start all the aircrafts after key press
void start_aircrafts() {
	for (Aircraft *a : active_aircrafts) {
		a->startThreads();
	}
}

void clean_up_aircrafts() {
	for (Aircraft *a : active_aircrafts) {
		delete a;
	}
	active_aircrafts.clear();
}

void handle_termination(int signum) {
    std::cout << "[AirspaceManager] received signal " << signum << ", cleaning up...\n";
    cleanup_shared_memory(AIRSPACE_SHM_NAME, shm_fd, (void*) airspace, sizeof(Airspace));
    clean_up_aircrafts();
    exit(0);
}

void setup_signal_handlers() {
    signal(SIGINT, handle_termination);   // Ctrl+C
    signal(SIGTERM, handle_termination);  // kill
}

int main() {
	setup_signal_handlers();
	airspace = init_shared_memory();

	load_aircraft_data_from_file("/tmp/aircraft_data.txt");
	sleep(1);
	verify_aircraft_data();

	cout << "\nIterating through aircraft data using shared memory limits:\n";
	for (size_t i = 0; i < MAX_AIRCRAFT; i++) {
		if (airspace->aircraft_data[i].id == 0) continue;
		cout << "Aircraft ID: " << airspace->aircraft_data[i].id << endl;
	}

	// Sort the aircraft in queue according to entry time
	std::sort(aircraft_queue.begin(), aircraft_queue.end(),
		          [](const std::pair<time_t, AircraftData>& a,
		             const std::pair<time_t, AircraftData>& b) {
		              return a.first < b.first;
		          });

//	cout << "Press Enter to start airspace simulation..." << endl;
//	cin.get();

	spawn_aircrafts_by_time();


	cout << "Press Enter to end airspace simulation..." << endl;
	cin.get();

	cleanup_shared_memory(AIRSPACE_SHM_NAME, shm_fd, (void *)airspace,
						  sizeof(Airspace));
	clean_up_aircrafts();
	return 0;
}
