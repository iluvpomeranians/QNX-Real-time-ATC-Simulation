#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <mutex>
#include <time.h>
#include "aircraft.h"

Airspace* Aircraft::shared_memory = nullptr;
int Aircraft::aircraft_index = 0;

Aircraft::Aircraft(time_t entryTime,
				   int id,
		           double x,
				   double y,
				   double z,
				   double speedX,
				   double speedY,
				   double speedZ,
				   Airspace* shared_mem):
								   entryTime(entryTime),
								   id(id),
						   	   	   speedX(speedX),
								   speedY(speedY),
								   speedZ(speedZ),
								   running(true),
								   attach(nullptr),
								   position_thread(0),
								   ipc_thread(0),
								   shm_index(aircraft_index)
								   {

	if (Aircraft::shared_memory == nullptr){ Aircraft::shared_memory = shared_mem; }

//	std::cout << "[Aircraft] Using Shared Memory Address: " << Aircraft::shared_memory << std::endl;

	lastupdatedTime = time(NULL);

	Aircraft::shared_memory->aircraft_data[shm_index] = {entryTime, lastupdatedTime, id, x, y, z, speedX, speedY, speedZ, true, true};

//	std::cout << "Aircraft Created: " << id
//	          << " Stored at: " << &Aircraft::shared_memory->aircraft_data[shm_index]
//	          << " ID in Memory: " << Aircraft::shared_memory->aircraft_data[shm_index].id
//	          << std::endl;

	aircraft_index++;

}


void* Aircraft::updatePositionThread(void* arg) {
    Aircraft* aircraft = static_cast<Aircraft*>(arg);

    struct timespec req;
    req.tv_sec = 1;         // 1 second
    req.tv_nsec = 0;        // 0 nanoseconds

    while (aircraft->running) {

        // Acquire lock on shared memory mutex
        pthread_mutex_lock(&shared_memory->lock);

        // Update aircraft position based on speed
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].x += aircraft->speedX;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].y += aircraft->speedY;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].z += aircraft->speedZ;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].lastupdatedTime = time(nullptr);

        // Unlock shared memory
        pthread_mutex_unlock(&shared_memory->lock);

        nanosleep(&req, NULL);
    }

    return nullptr;
}

void* Aircraft::messageHandlerThread(void* arg){
	Aircraft* aircraft = static_cast<Aircraft*>(arg);

	//Tutorial #4 IPC message channel creation
	char service_name[20];
	snprintf(service_name, sizeof(service_name), "Aircraft%d", aircraft->id);
	aircraft->attach = name_attach(NULL, service_name, 0);

	if(aircraft->attach == NULL){
		perror("name attach");
		return nullptr;
	}

	std::cout << "[Aircraft " << aircraft->id << "] IPC channel registration: " << service_name << std::endl;

	struct {
        struct _pulse pulse;
        OperatorCommand cmd;
    } msg;

    while(aircraft->running){
        int rcvid = MsgReceive(aircraft->attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive");
            continue;
        }
        if (rcvid > 0){
            std::cout << "[Aircraft] Received command for Aircraft ID: " << msg.cmd.aircraft_id << std::endl;
            if (msg.cmd.type == CommandType::ChangeSpeed) {
                aircraft->speedX = msg.cmd.speed.vx;
                aircraft->speedY = msg.cmd.speed.vy;
                aircraft->speedZ = msg.cmd.speed.vz;
                std::cout << "[Aircraft] Speed updated to: (" << aircraft->speedX << ", "
                          << aircraft->speedY << ", " << aircraft->speedZ << ")" << std::endl;
            } else if (msg.cmd.type == CommandType::ChangePosition) {
                Aircraft::shared_memory->aircraft_data[aircraft->shm_index].x = msg.cmd.position.x;
                Aircraft::shared_memory->aircraft_data[aircraft->shm_index].y = msg.cmd.position.y;
                Aircraft::shared_memory->aircraft_data[aircraft->shm_index].z = msg.cmd.position.z;
                std::cout << "[Aircraft] Position updated to: (" << msg.cmd.position.x << ", "
                          << msg.cmd.position.y << ", " << msg.cmd.position.z << ")" << std::endl;
            }
            MsgReply(rcvid, 0, NULL, 0);
        }
    }

	return nullptr;
}

void Aircraft::startThreads(){
	pthread_create(&position_thread, nullptr, updatePositionThread, this);
	//pthread_create(&ipc_thread, nullptr, messageHandlerThread, this);
}

void Aircraft::stopThreads(){
	running = false;
	pthread_join(position_thread, nullptr);
	//pthread_join(ipc_thread, nullptr);
}

Aircraft::~Aircraft(){

	stopThreads();

	if (attach){
		name_detach(attach, 0);
	}
}


