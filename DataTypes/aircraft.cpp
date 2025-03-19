#include "aircraft.h"

AircraftData* Aircraft::shared_memory = nullptr;
static int aircraft_index = 0;

Aircraft::Aircraft(int time,
				   int id,
		           double x,
				   double y,
				   double z,
				   double speedX,
				   double speedY,
				   double speedZ,
				   AircraftData* shared_mem): time(time),
						   	   	   id(id),
						   	   	   speedX(speedX),
								   speedY(speedY),
								   speedZ(speedZ),
								   running(true),
								   attach(nullptr),
								   position_thread(0),
								   ipc_thread(0)
								   {

	if (Aircraft::shared_memory == nullptr){Aircraft::shared_memory = shared_mem;}

	std::cout << "[Aircraft] Using Shared Memory Address: " << Aircraft::shared_memory << std::endl;

	Aircraft::shared_memory[aircraft_index] = {time, id, x, y, z, speedX, speedY, speedZ, true, true};

	std::cout << "Aircraft Created: " << id
	          << " Stored at: " << &Aircraft::shared_memory[id]
	          << " ID in Memory: " << Aircraft::shared_memory[id].id
	          << std::endl;

	aircraft_index++;

}


void* Aircraft::updatePositionThread(void* arg){
	Aircraft* aircraft = static_cast<Aircraft*>(arg);
	while(aircraft->running){
		std::lock_guard<std::mutex> guard(aircraft->lock);
		Aircraft::shared_memory[aircraft->id].x += aircraft->speedX;
		Aircraft::shared_memory[aircraft->id].y += aircraft->speedY;
		Aircraft::shared_memory[aircraft->id].z += aircraft->speedZ;

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
	//pthread_create(&ipc_thread, nullptr, messageHandlerThread, this);
}

void Aircraft::stopThreads(){
	running = false;
	pthread_join(position_thread, nullptr);
	//pthread_join(ipc_thread, nullptr);
}

Aircraft::~Aircraft(){

	stopThreads();

	//Close IPCizzle comms
	if (attach){
		name_detach(attach, 0);
	}
}


