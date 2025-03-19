/*
 * aircraft_data.h
 *
 *  Created on: Mar 17, 2025
 *      Author: david
 */

#ifndef AIRCRAFT_DATA_H_
#define AIRCRAFT_DATA_H_

#include <ctime>

#define SHM_NAME "/air_traffic_shm"
#define MAX_AIRCRAFT 100

struct AircraftData {
    int time, id;
    double x, y, z;
    double speedX, speedY, speedZ;
    bool detected;
    bool responded;
};

struct AircraftMsg{
    time_t timestamp;
    char message_content[128];
    int id;
	double x, y, z;
	double speedX, speedY, speedZ;
	bool detected;
	bool responded;
} ;


#endif /* AIRCRAFT_DATA_H_ */
