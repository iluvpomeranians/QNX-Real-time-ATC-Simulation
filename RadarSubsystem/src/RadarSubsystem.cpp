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
#include "RadarSubsystem.h"
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/aircraft.h"
#include "../../DataTypes/timing_logger.h"

using namespace std;

TimingLogger logger("radar.txt");

Airspace* airspace = nullptr;
std::vector<Aircraft*> active_aircrafts;
int shm_fd_airspace;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t airspace_thread;
pthread_t message_thread;


Airspace* init_airspace_shared_memory() {
    std::cout << "[RadarSubsystem] Waiting for Airspace shared memory to become available...\n";
    struct timespec wait_time = {1, 0};
    while (true) {
        shm_fd_airspace = shm_open(AIRSPACE_SHM_NAME, O_RDWR, 0666);
        if (shm_fd_airspace != -1) {
            break;
        }
        nanosleep(&wait_time, NULL);
    }

    void* addr = mmap(NULL, sizeof(Airspace), PROT_READ | PROT_WRITE,
                      MAP_SHARED, shm_fd_airspace, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed for aircraft data");
        exit(1);
    }

    std::cout << "[RadarSubsystem] Connected to Airspace shared memory at " << addr << "\n";
    return static_cast<Airspace*>(addr);
}

void clear_airspace_logfile() {
    std::ofstream logfile("/tmp/airspace_history.txt", std::ios::trunc);
    if (!logfile.is_open()) {
        perror("[RadarSubsystem] Failed to clear airspace history log");
        return;
    }
    logfile << "[RadarSubsystem] Airspace history log initialized.\n\n";
    logfile.close();
}


void log_airspace_history() {
    std::ofstream logfile("/tmp/airspace_history.txt", std::ios::app);
    if (!logfile.is_open()) {
        perror("[RadarSubsystem] Failed to open airspace history log");
        return;
    }

    time_t now = time(NULL);
    tm* timeinfo = localtime(&now);
    char timeBuffer[64];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    logfile << "=== Snapshot at " << timeBuffer << " ===\n";

    for (int i = 0; i < airspace->aircraft_count; ++i) {
        AircraftData& a = airspace->aircraft_data[i];
        if (a.id == 0) continue;

        logfile << "Aircraft " << a.id
                << " | Pos: (" << a.x << ", " << a.y << ", " << a.z << ")"
                << " | Vel: (" << a.speedX << ", " << a.speedY << ", " << a.speedZ << ")\n";
    }

    logfile << '\n';
    logfile.close();
}


void* updateAirspaceDetectionThread(void* arg) {
	struct timespec req;
		    req.tv_sec = 1;         // 1 second
		    req.tv_nsec = 0;        // 0 nanoseconds

    time_t last_log_time = time(NULL);

	while (true) {
		timespec start = logger.now();
		pthread_mutex_lock(&airspace->lock);

		for(int i = 0; i < airspace->aircraft_count; ++i) {
			AircraftData* aircraft = &airspace->aircraft_data[i];

			if (aircraft->x > 100000 || aircraft->y > 100000 ||
				aircraft->x < 0 || aircraft->y < 0 || aircraft->z > 25000 ||
				aircraft->z < 15000) {
				aircraft->detected = false;
			} else {
				aircraft->detected = true;
				if (aircraft->responded == false) {
					cout << "Aircraft " << aircraft->id << " has not been pinged, starting secondary radar...\n";
					pthread_create(&message_thread, NULL, send_message, (void*)&aircraft->id);

					pthread_join(message_thread, NULL);
					cout << "[RADAR] Thread joined\n";
					aircraft->responded = true;
				}
			}
		}

		time_t now = time(NULL);
		if (now - last_log_time >= 20) {
			log_airspace_history();
			last_log_time = now;
		}
		pthread_mutex_unlock(&airspace->lock);
		timespec end = logger.now();
		logger.logDuration("updateAirspaceDetectionThread", start, end);
		nanosleep(&req, NULL);
	}
	return nullptr;
}

void* send_message(void* arg) {
	int id = *(int*)arg;

	int coid;
	char server_name[20];
	struct timespec wait_time = {1, 0};

	snprintf(server_name, sizeof(server_name), "Aircraft%d", id);

	cout << "[DEBUG] server_name: " << server_name << endl;

	// Wait until the server becomes available
	while ((coid = name_open(server_name, 0)) == -1) {
		cout << "Waiting for server " << server_name << "to start...\n";
		nanosleep(&wait_time, NULL);
	}
	cout << "Connected to server '" << server_name << "'\n";

	RadarMessage radar_message;
	strcpy(radar_message.request_msg, "Plz identify yourself and send heading");

	message_t msg;
	msg.type = RADAR_TYPE;
	msg.aircraft_id = id;
	msg.message.radar_message = radar_message;


	RadarReply reply;

	int status = MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
    if (status == -1) {
        perror("MsgSend");
    }

    cout << fixed << setprecision(1);

    tm* timeinfo = localtime(&reply.timestamp);
    char timeBuffer[64];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    string delimiter = string(50, '=');

    cout << delimiter << '\n';
    cout << left;
    cout << setw(15) << "Timestamp:"    << timeBuffer << '\n';
    cout << setw(15) << "Aircraft ID:"  << reply.id << '\n';
    cout << setw(15) << "Position:"     << "(" << reply.x << ", " << reply.y << ", " << reply.z << ")" << '\n';
    cout << setw(15) << "Velocity:"     << "(" << reply.speedX << ", " << reply.speedY << ", " << reply.speedZ << ")" << '\n';
    cout << delimiter << '\n';

    name_close(coid);
    return nullptr;
}

void cleanUpOnExit() {
	pthread_join(airspace_thread, nullptr);
}

int main() {

	clear_airspace_logfile();

    airspace = init_airspace_shared_memory();

    std::cout << "[DEBUG] Shared Memory Base Address for Aircrafts: " << airspace << std::endl;

	pthread_create(&airspace_thread, nullptr, updateAirspaceDetectionThread, NULL);

	struct timespec req;
			    req.tv_sec = 10;
			    req.tv_nsec = 0;


    std::cout << "Radar System initialized. Shared memory ready.\n";

    while (true) nanosleep(&req, NULL);

    munmap(airspace, sizeof(Airspace));
    close(shm_fd_airspace);

    cleanUpOnExit();

    return 0;
}
