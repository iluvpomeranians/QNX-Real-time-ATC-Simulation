#include "aircraft.h"

AircraftData* aircrafts_shared_memory = nullptr;

Aircraft::Aircraft(int id,
		           double x,
				   double y,
				   double z,
				   double speedX,
				   double speedY,
				   double speedZ): id(id),
						   	   	   speedX(speedX),
								   speedY(speedY),
								   speedZ(speedZ),
								   running(true),
								   attach(nullptr),
								   position_thread(0),
								   ipc_thread(0){

	if (aircrafts_shared_memory == nullptr){
		int shm_fd = shm_open(SHM_NAME, O_RDWR , 0666);
		if (shm_fd == -1){
			perror("shm_open failed");
			return;
		}

		aircrafts_shared_memory = (AircraftData*) mmap(NULL,
										sizeof(AircraftData) * MAX_AIRCRAFT,
										PROT_READ | PROT_WRITE,
										MAP_SHARED,
										shm_fd, 0);

		if (aircrafts_shared_memory == MAP_FAILED){
			perror("mmap failed");
			return;
		}


	}

	aircrafts_shared_memory[id] = {id, x, y, z, speedX, speedY, speedZ, true, true};

	std::cout << "Aircraft Created: " << id
	          << " Stored at: " << &aircrafts_shared_memory[id]
	          << " ID in Memory: " << aircrafts_shared_memory[id].id
	          << std::endl;

}


void* Aircraft::updatePositionThread(void* arg){
	Aircraft* aircraft = static_cast<Aircraft*>(arg);
	while(aircraft->running){
		std::lock_guard<std::mutex> guard(aircraft->lock);
		aircrafts_shared_memory[aircraft->id].x += aircraft->speedX;
		aircrafts_shared_memory[aircraft->id].y += aircraft->speedY;
		aircrafts_shared_memory[aircraft->id].z += aircraft->speedZ;

		sleep(1);
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

    while(aircraft->running){
    	//AircraftMsg msg;
    	//int rcvid = MsgReceive(aircraft->attach->chid, msg, sizeof(msg), NULL);

    	//if (rcvid == -1) {
		//	perror("MsgReceive");
		//	continue;
		//}

    	//if (rcvid > 0){
    		//TODO: will need to finish implementation for received message
    	//}

    	//AircraftMsg response;
    	// TODO: will need to finish implementation for reply message
    	// MsgReply(rcvid, 0, response, sizeof(response));


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

Aircraft::~Aircraft(){

	stopThreads();

	//Close IPCizzle comms
	if (attach){
		name_detach(attach, 0);
	}

	//Unmap shared mem
	if (aircrafts_shared_memory){
		munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
	}
}


