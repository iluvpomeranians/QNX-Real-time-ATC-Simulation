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
								   x(x),
								   y(y),
								   z(z),
						   	   	   speedX(speedX),
								   speedY(speedY),
								   speedZ(speedZ),
								   running(true),
								   attach(nullptr),
								   service_name{0},
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
	snprintf(aircraft->service_name, sizeof(aircraft->service_name),
			 "Aircraft%d", aircraft->id);
	aircraft->attach = name_attach(NULL, aircraft->service_name, 0);
	if (aircraft->attach == NULL) {
		perror("name attach");
		return nullptr;
	}

	std::cout << "[Aircraft " << aircraft->id << "] IPC channel registration: " << aircraft->service_name << std::endl;

	message_t msg;

    while (true) {
        int rcvid = MsgReceive(aircraft->attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
			perror("MsgReceive");
			continue;
        }
        if (rcvid > 0) {
            std::cout << "[Aircraft " << aircraft->id << "] Received message for Aircraft ID: " << msg.aircraft_id << std::endl;

            if (msg.type == OPERATOR_TYPE) {
            	aircraft->handle_operator_message(rcvid, &msg.message.operator_command);
            } else if (msg.type == RADAR_TYPE) {
            	aircraft->handle_radar_message(rcvid, &msg.message.radar_message);
            } else if (msg.type == TERMINATOR_TYPE) {
				MsgReply(rcvid, 0, NULL, 0);
				break;
            } else {
            	// Just in case
				MsgReply(rcvid, 0, NULL, 0);
				break;
            }
        }
    }
	return nullptr;
}

void Aircraft::startThreads(){
	pthread_create(&position_thread, nullptr, updatePositionThread, this);
	pthread_create(&ipc_thread, nullptr, messageHandlerThread, this);
}

// TODO: Delete prints
void Aircraft::stopThreads() {
	running = false;
	pthread_join(position_thread, nullptr);
	std::cout << "[DEBUG] position_thread joined\n";

	// ipc_thread can stay blocked on MsgReceive() under certain conditions.
	// Changed to infinite while loop that will wait for terminator message.
	send_terminator_message();
	pthread_join(ipc_thread, nullptr);
	std::cout << "[DEBUG] ipc_thread joined\n";
}

void Aircraft::send_terminator_message() {
	int coid = name_open(this->service_name, 0);
	if (coid == -1) {
		perror("name_open");
	}

	TerminatorMessage terminator_msg;
	message_t msg;
	msg.type                       = TERMINATOR_TYPE;
	msg.aircraft_id                = this->id;
	msg.message.terminator_message = terminator_msg;

	int status = MsgSend(coid, &msg, sizeof(msg), &terminator_msg, sizeof(terminator_msg));
	if (status == -1) {
		perror("MsgSend");
	}
}

void Aircraft::handle_operator_message(int rcvid, OperatorCommand* cmd) {
	// Lock shared memory before updating position
	pthread_mutex_lock(&shared_memory->lock);

	if (cmd->type == CommandType::ChangeSpeed) {
		this->speedX = cmd->speed.vx;
		this->speedY = cmd->speed.vy;
		this->speedZ = cmd->speed.vz;

        Aircraft::shared_memory->aircraft_data[this->shm_index].speedX = cmd->speed.vx;
        Aircraft::shared_memory->aircraft_data[this->shm_index].speedY = cmd->speed.vy;
        Aircraft::shared_memory->aircraft_data[this->shm_index].speedZ = cmd->speed.vz;
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
	else if (cmd->type == CommandType::RequestDetails) {

		std::cout << "[Aircraft] Position : (" << Aircraft::shared_memory->aircraft_data[this->shm_index].x << ", "
				  << shared_memory->aircraft_data[this->shm_index].y << ", " << shared_memory->aircraft_data[this->shm_index].z << ")" << std::endl;
		std::cout << "[Aircraft] Speed : (" << Aircraft::shared_memory->aircraft_data[this->shm_index].speedX << ", "
						  << Aircraft::shared_memory->aircraft_data[this->shm_index].speedY << ", " << Aircraft::shared_memory->aircraft_data[this->shm_index].speedZ << ")" << std::endl;
	}
	pthread_mutex_unlock(&shared_memory->lock);

	// TODO: Implement reply
	MsgReply(rcvid, 0, NULL, 0);
}

void Aircraft::handle_radar_message(int rcvid, RadarMessage* radar_message) {

	std::cout << "[Aircraft " << this->id << "] Sending identification to Radar...\n";

	RadarReply reply_msg = {
			time(NULL),
			"Here's my id and heading bro",
			this->id,
			this->x, this->y, this->z,
			this->speedX, this->speedY, this->speedZ
	};
	MsgReply(rcvid, 0, &reply_msg, sizeof(reply_msg));
}

Aircraft::~Aircraft(){

	stopThreads();

	if (attach){
		name_detach(attach, 0);
	}
}


