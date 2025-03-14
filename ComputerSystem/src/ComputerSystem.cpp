#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <stdbool.h>

#define SHM_NAME "/air_traffic_shm"
#define MAX_AIRCRAFT 100

struct AircraftData {
	int id;
	double x,y,z;
	double speedX, speedY, speedZ;
	bool detected;
	bool responded;
};

//Shared Mem Ptr to aircrafts
AircraftData* aircrafts;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

void sendAlert(int aircraft1, int aircraft2){
	std::cout << "DEBUG Alert: Aircraft " << aircraft1 <<
			" and Aircraft " << aircraft2 << " are too close" << std::endl;
	//TODO: we'll fix this later where we send it to the operator
}

void sendIdToDisplay(int aircraft_id){
	std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
	//TODO: fix later to send ID to display
}

void* violationCheck(void* arg){//this void arg is required for pthread apparently
	while(1){
		sleep(5);//Not sure - says we should update display every 5 sec
		pthread_mutex_lock(&shm_mutex);

		std::cout << "Checking for Safety Violations..." << std::endl;

		for (int i = 0; i < MAX_AIRCRAFT; i++){
			for (int j = i + 1; j < MAX_AIRCRAFT; j++){
				double dx = std::fabs(aircrafts[i].x - aircrafts[j].x);
				double dy = std::fabs(aircrafts[i].y - aircrafts[j].y);
				double dz = std::fabs(aircrafts[i].z - aircrafts[j].z);

				std::cout << "Aircraft 1: "<< aircrafts[i].id << " Aircraft 2: " << aircrafts[j].id << std::endl;

				//Euclid horizontal distance d = sqrt((x1-x2)^2 + (y1 - y2)^2)
				double horizontalXYDiff = std::sqrt(dx*dx + dy*dy);

				//Aircraft must have a distance no less than 1000 units in height
				//and 3000 units width/length from other aircraft
				if (horizontalXYDiff < 3000 || dz < 1000){
					sendAlert(aircrafts[i].id, aircrafts[j].id);
				}
			}
		}

		pthread_mutex_unlock(&shm_mutex);

	}
}

int main() {

	//From tutorial 7: HR reader
	//We have O_CREAT but in reality i think radar subsystem should create the shared mem initially
	int shm_fd = shm_open(SHM_NAME, O_RDWR , 0666);
	if (shm_fd == -1){
		perror("shm_open failed");
		return 1;
	}

	aircrafts = (AircraftData*) mmap(0,
									sizeof(AircraftData) * MAX_AIRCRAFT,
									PROT_READ | PROT_WRITE,
									MAP_SHARED,
									shm_fd, 0);

	if (aircrafts == MAP_FAILED){
		perror("mmap failed");
		return 1;
	}


	//create a montior thread
	pthread_t monitorThread;
	pthread_create(&monitorThread, NULL, violationCheck, NULL);
	pthread_join(monitorThread, NULL);

	//unmap the shared memory (but not unlink it)
	munmap(aircrafts, sizeof(AircraftData) * MAX_AIRCRAFT);
	close(shm_fd);

	//Tutorial 7 has this unlink thing, but i think we should only call this at the end in Radar Subsystem?
	//I think because we should only unlink once at the end of the program to delete shared mem
	// if(shm_unlink(SHM_NAME) == -1{ perror("can't unlink"); exit(1);}
}
