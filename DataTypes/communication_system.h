/*
 * communication_system.h
 *
 *  Created on: Mar. 27, 2025
 *      Author: patel
 */

#ifndef COMMUNICATION_SYSTEM_H_
#define COMMUNICATION_SYSTEM_H_

#include <pthread.h>
#include <sys/types.h>
#include "operator_command.h"

#define COMMUNICATION_COMMAND_SHM_NAME "communication_commands_shm"
#define MAX_COMMANDS 100

struct CommunicationCommandMemory {
    pthread_mutex_t lock;
    int command_count;
    OperatorCommand commands[MAX_COMMANDS];
    pid_t comm_pid;
    bool pid_ready;
};

#endif
