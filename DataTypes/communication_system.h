/*
 * communication_system.h
 *
 *  Created on: Mar. 27, 2025
 *      Author: patel
 */

#ifndef COMMUNICATION_SYSTEM_H_
#define COMMUNICATION_SYSTEM_H_

#include "operator_command.h"  // For OperatorCommand
#include "airspace.h"          // For Airspace, Aircraft

#define COMMUNICATION_COMMAND_SHM_NAME "communication_commands_shm"
#define MAX_COMMANDS 100  // Example value, adjust as needed


struct CommunicationCommandMemory {
    pthread_mutex_t lock;
    int command_count;
    OperatorCommand commands[MAX_COMMANDS];
};


#endif /* COMMUNICATION_SYSTEM_H_ */
