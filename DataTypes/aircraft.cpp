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
#include "operator_command.h"
#include "message_types.h"

Airspace* Aircraft::shared_memory = nullptr;
int Aircraft::aircraft_index = 0;

// TODO: Per-instance mutex for locking member values when reading/writing them
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

	Aircraft::shared_memory->aircraft_data[shm_index] = {entryTime, lastupdatedTime, id, x, y, z, speedX, speedY, speedZ, true, false};

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

        pthread_mutex_lock(&shared_memory->lock);

        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].x += aircraft->speedX;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].y += aircraft->speedY;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].z += aircraft->speedZ;
        Aircraft::shared_memory->aircraft_data[aircraft->shm_index].lastupdatedTime = time(nullptr);

        pthread_mutex_unlock(&shared_memory->lock);

        nanosleep(&req, NULL);
    }

    return nullptr;
}

void* Aircraft::messageHandlerThread(void* arg) {
	Aircraft* aircraft = static_cast<Aircraft*>(arg);

	//Tutorial #4 IPC message channel creation
	char service_name[20];
	snprintf(service_name, sizeof(service_name), "Aircraft%d", aircraft->id);
	aircraft->attach = name_attach(NULL, service_name, 0);

	if (aircraft->attach == NULL) {
		perror("name attach");
		return nullptr;
	}

	std::cout << "[Aircraft " << aircraft->id << "] IPC channel registration: " << service_name << std::endl;

	message_t msg;

    while (aircraft->running) {
        int rcvid = MsgReceive(aircraft->attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive");
            continue;
        }
        if (rcvid > 0) {
            std::cout << "[Aircraft] Received message for Aircraft ID: " << msg.aircraft_id << std::endl;

            if (msg.type == OPERATOR_TYPE) aircraft->handle_operator_message(&msg.message.operator_command);
            if (msg.type == RADAR_TYPE)    aircraft->handle_radar_message(&msg.message.radar_message);


            //TODO: might have to send a reply -- ack message/signal
            MsgReply(rcvid, 0, NULL, 0);
        }
    }

	return nullptr;
}

void Aircraft::startThreads(){
	pthread_create(&position_thread, nullptr, updatePositionThread, this);
	pthread_create(&ipc_thread, nullptr, messageHandlerThread, this);
}

void Aircraft::stopThreads(){
	running = false;
	pthread_join(position_thread, nullptr);
	pthread_join(ipc_thread, nullptr);
}

void Aircraft::handle_operator_message(OperatorCommand* cmd) {
	// Lock shared memory before updating position
	pthread_mutex_lock(&shared_memory->lock);

	if (cmd->type == CommandType::ChangeSpeed) {
		this->speedX = cmd->speed.vx;
		this->speedY = cmd->speed.vy;
		this->speedZ = cmd->speed.vz;

		// Aircraft::shared_memory->aircraft_data[this->shm_index].speedX = msg->cmd.speed.vx;
		// Aircraft::shared_memory->aircraft_data[this->shm_index].speedY = msg->cmd.speed.vy;
		// Aircraft::shared_memory->aircraft_data[this->shm_index].speedZ = msg->cmd.speed.vz;
		std::cout << "[Aircraft] Speed updated to: (" << this->speedX << ", "
				  << this->speedY << ", " << this->speedZ << ")" << std::endl;
	} else if (cmd->type == CommandType::ChangePosition) {
		//TODO: possibly revise
		//This instantly changes the heading
		Aircraft::shared_memory->aircraft_data[this->shm_index].x = cmd->position.x;
		Aircraft::shared_memory->aircraft_data[this->shm_index].y = cmd->position.y;
		Aircraft::shared_memory->aircraft_data[this->shm_index].z = cmd->position.z;
		std::cout << "[Aircraft] Position updated to: (" << cmd->position.x << ", "
				  << cmd->position.y << ", " << cmd->position.z << ")" << std::endl;
	}
	else if (msg->cmd.type == CommandType::RequestDetails) {

		std::cout << "[Aircraft] Position : (" << Aircraft::shared_memory->aircraft_data[this->shm_index].x << ", "
				  << shared_memory->aircraft_data[this->shm_index].y << ", " << shared_memory->aircraft_data[this->shm_index].z << ")" << std::endl;
		std::cout << "[Aircraft] Speed : (" << Aircraft::shared_memory->aircraft_data[this->shm_index].speedX << ", "
						  << Aircraft::shared_memory->aircraft_data[this->shm_index].speedY << ", " << Aircraft::shared_memory->aircraft_data[this->shm_index].speedZ << ")" << std::endl;
	}
	pthread_mutex_unlock(&shared_memory->lock);
}

void Aircraft::handle_radar_message(RadarMessage* radar_message) {


}

Aircraft::~Aircraft(){

	stopThreads();

	if (attach){
		name_detach(attach, 0);
	}
}


