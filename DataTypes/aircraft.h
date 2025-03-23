#pragma once

#include "aircraft_data.h"
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sys/dispatch.h>
#include <mutex>
#include <time.h>

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


