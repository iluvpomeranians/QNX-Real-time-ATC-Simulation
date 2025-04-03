#pragma once

#include <sys/dispatch.h>
#include "aircraft_data.h"
#include "airspace.h"
#include "message_types.h"

class Aircraft {

private:
	static Airspace* shared_memory;
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
			 Airspace* shared_mem);

	~Aircraft();

	void startThreads();
	void stopThreads();

	static void* updatePositionThread(void* arg);
	static void* messageHandlerThread(void* arg);
	void handle_operator_message(OperatorMessage*);
	void handle_radar_message(RadarMessage*);


};


