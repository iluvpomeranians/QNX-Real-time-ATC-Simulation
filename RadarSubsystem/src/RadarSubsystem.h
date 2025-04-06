/*
 * RadarSubsystem.h
 *
 *  Created on: Mar 17, 2025
 *      Author: david
 */

#ifndef SRC_RADARSUBSYSTEM_H_
#define SRC_RADARSUBSYSTEM_H_

#include "../../DataTypes/aircraft.h"

Airspace* init_airspace_shared_memory();
void* updateAirspaceDetectionThread(void*);
void* send_message(void*);
void fake_aircraft_data();
void verify_aircraft_data();

#endif /* SRC_RADARSUBSYSTEM_H_ */
