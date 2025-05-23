/*
 * message_types.h
 *
 *  Created on: Apr 3, 2025
 *      Author: timot
 */

#ifndef MESSAGE_TYPES_H_
#define MESSAGE_TYPES_H_

#include "aircraft_data.h"
#include "operator_command.h"

#define OPERATOR_TYPE   0
#define RADAR_TYPE      1
#define TERMINATOR_TYPE 2


//typedef struct {
//	struct _pulse pulse;
//	OperatorCommand cmd;
//} OperatorMessage;

typedef struct {
	char request_msg[128];
} RadarMessage;

typedef struct {} TerminatorMessage;

typedef struct {
	int type;
	int aircraft_id;
	union {
		OperatorCommand operator_command;
		RadarMessage radar_message;
		TerminatorMessage terminator_message;
	} message;
} message_t;

typedef struct {
    time_t timestamp;
    char message_content[128];
    int id;
	double x, y, z;
	double speedX, speedY, speedZ;
} RadarReply;

#endif /* MESSAGE_TYPES_H_ */
