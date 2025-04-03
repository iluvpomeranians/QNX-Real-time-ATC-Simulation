/*
 * aircraft_data.h
 *
 *  Created on: Mar 17, 2025
 *      Author: david
 */

#ifndef AIRCRAFT_DATA_H_
#define AIRCRAFT_DATA_H_

#include <time.h>

#define MAX_AIRCRAFT 100

struct AircraftData {
	time_t entryTime, lastupdatedTime;
    int id;
    double x, y, z;
    double speedX, speedY, speedZ;
    bool detected;
    bool responded;
};

#endif /* AIRCRAFT_DATA_H_ */
