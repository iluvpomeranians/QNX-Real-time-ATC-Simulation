/*
 * RadarSubsystem.h
 *
 *  Created on: Mar 17, 2025
 *      Author: david
 */

#ifndef SRC_RADARSUBSYSTEM_H_
#define SRC_RADARSUBSYSTEM_H_

#include <unordered_map>
#include "../../DataTypes/aircraft.h"

class RadarSubsystem {
private:
	std::unordered_map<int, Aircraft*> all_aircrafts;

public:
	static AircraftData* aircrafts_shared_memory;

	RadarSubsystem();
	~RadarSubsystem();

	void fake_aircraft_data();
	void verify_aircraft_data();
};

#endif /* SRC_RADARSUBSYSTEM_H_ */
