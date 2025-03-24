#pragma once

#include <sys/dispatch.h>
#include "aircraft_data.h"


class Aircraft {

private:
	static AircraftData* shared_memory;
	static int aircraft_index;

public:
	time_t entryTime, lastupdatedTime;
	int id;
	double speedX, speedY, speedZ;
	bool running;
	name_attach_t* attach;
	pthread_t position_thread, ipc_thread;
	std::mutex lock;
	int shm_index;

	Aircraft(time_t entryTime,
			 int id,
			 double x,
			 double y,
			 double z,
			 double speedX,
			 double speedY,
			 double speedZ,
			 AircraftData* shared_mem);

	~Aircraft();

	void startThreads();
	void stopThreads();

	static void* updatePositionThread(void* arg);
	static void* messageHandlerThread(void* arg);

};


