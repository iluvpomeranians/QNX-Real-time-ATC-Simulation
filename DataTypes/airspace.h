/*
 * airspace.h
 *
 *  Created on: Mar 24, 2025
 *      Author: timot
 */

#ifndef AIRSPACE_H_
#define AIRSPACE_H_

#include "aircraft_data.h"
#define MAX_AIRCRAFT 100

struct Airspace {
	pthread_mutex_t lock;
	int aircraft_count;
	bool updated;
	AircraftData aircraft_data[MAX_AIRCRAFT];
};



#endif /* AIRSPACE_H_ */
