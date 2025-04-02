#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <cstring>
#include <ctime>
#include <csignal>
#include "../../DataTypes/operator_command.h"
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/aircraft.h"
#include "../../DataTypes/communication_system.h"

OperatorCommandMemory* operator_cmd_mem = nullptr;
volatile sig_atomic_t signal_received = 0;

//Tutorial #3
void signal_handler(int signo) {
    if (signo == SIGUSR1) {
    	signal_received = 1;
    }
}

// Function to send a command to a specific aircraft via IPC
void send_command_to_aircraft(int aircraft_id, const OperatorCommand& cmd) {
    char service_name[20];
    snprintf(service_name, sizeof(service_name), "Aircraft%d", aircraft_id);

    int coid = name_open(service_name, 0);
    if (coid == -1) {
        perror("[CommunicationSystem] Failed to connect to aircraft IPC channel");
        return;
    }

    int status = MsgSend(coid, &cmd, sizeof(cmd), nullptr, 0);
    if (status == -1) {
        perror("[CommunicationSystem] Failed to send command to aircraft");
    } else {
        std::cout << "[CommunicationSystem] Command sent to Aircraft ID " << aircraft_id << std::endl;
    }

    name_close(coid);
}

// Function to poll operator commands from shared memory and forward them to the aircraft
void* pollOperatorCommands(void* arg) {
    OperatorCommandMemory* cmd_mem = (OperatorCommandMemory*) arg;

    std::cout << "[CommunicationSystem] Polling Operator Commands from Shared Memory...\n";

    while (true) {

        while (!signal_received) { // Busy-wait for the signal
            usleep(1000);
        }

        signal_received = 0;  // Reset the flag after handling the signal

        pthread_mutex_lock(&cmd_mem->lock);

        for (int i = 0; i < cmd_mem->command_count; ++i) {
            OperatorCommand& cmd = cmd_mem->commands[i];

            std::cout << "[Received Command] Aircraft ID: " << cmd.aircraft_id
                      << " | Type: " << cmd.type
                      << " | Position: (" << cmd.position.x << ", " << cmd.position.y << ", " << cmd.position.z << ")"
                      << " | Speed: (" << cmd.speed.vx << ", " << cmd.speed.vy << ", " << cmd.speed.vz << ")"
                      << std::endl;

            send_command_to_aircraft(cmd.aircraft_id, cmd);
        }

        //TODO: We should run a size-check on commands[] arr to make sure it aligns
        //with command count
        cmd_mem->command_count = 0;

        pthread_mutex_unlock(&cmd_mem->lock);

        sleep(1);
    }

    return NULL;
}

int main() {

	// Register signal handler for SIGUSR1
    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        perror("[CommunicationSystem] Signal handler registration failed");
        exit(EXIT_FAILURE);
    }

    // Connect to shared memory where the operator commands are stored
    int shm_fd_cmd;
    while (1) {
        	shm_fd_cmd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_RDWR, 0666);

            if (shm_fd_cmd != -1) {
            	 operator_cmd_mem = (OperatorCommandMemory*) mmap(NULL,
            			 	 	 	 	 	 	 	 	 	 	 sizeof(OperatorCommandMemory),
            	                                                 PROT_READ | PROT_WRITE,
																 MAP_SHARED,
																 shm_fd_cmd, 0);

            	 if (operator_cmd_mem != MAP_FAILED) {
            		 printf("[CommunicationSystem] Successfully connected to operator command shared memory");
                    close(shm_fd_cmd);
                    break;
                } else {
                    perror("[CommunicationSystem] mmap failed to operator command shared memory");
                    close(shm_fd_cmd);
                }

            }

            sleep(1);
        }


    std::cout << "[CommunicationSystem] Connected to operator command shared memory." << std::endl;
    close(shm_fd_cmd);

    // Start the command polling thread to handle commands from the computer system
    pthread_t command_thread;
    pthread_create(&command_thread, NULL, pollOperatorCommands, operator_cmd_mem);

    while (true) {
        sleep(1);
    }

    return 0;
}
