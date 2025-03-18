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

extern AircraftData* aircrafts_shared_memory;

class Aircraft {
public:

	int id;
	double speedX, speedY, speedZ;
	bool running;
	name_attach_t* attach;
	pthread_t position_thread, ipc_thread;
	std::mutex lock;

	Aircraft(int id,
			 double x,
			 double y,
			 double z,
			 double speedX,
			 double speedY,
			 double speedZ);

	~Aircraft();

	void startThreads();
	void stopThreads();

private:
	static void* updatePositionThread(void* arg);
	static void* messageHandlerThread(void* arg);
};


