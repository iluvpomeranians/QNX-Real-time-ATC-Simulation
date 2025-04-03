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
CommunicationCommandMemory* comm_mem = nullptr;
volatile sig_atomic_t signal_received = 0;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;  // Condition variable for signaling
int comm_fd;

//Tutorial #3
void signal_handler(int signo) {
    if (signo == SIGUSR1) {
    	pthread_cond_signal(&cond);  // Signal condition variable
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

        pthread_mutex_lock(&cmd_mem->lock);
    	// Wait for the signal to process commands
    	pthread_cond_wait(&cond, &cmd_mem->lock);


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
    }

    return NULL;
}

void cleanup_shared_memory(const char* shm_name, int shm_fd, void* addr,
						   size_t size) {
	if (munmap(addr, size) == 0) {
		std::cout << "Shared memory unmapped successfully." << std::endl;
	} else {
		perror("munmap failed");
	}

	close(shm_fd);

	if (shm_unlink(shm_name) == 0) {
		std::cout << "Shared memory unlinked successfully." << std::endl;
	} else {
		perror("shm_unlink failed");
	}

}


void handle_termination(int signum) {
    std::cout << "[CommunicationSystem]" << signum << ", cleaning up...\n";
    cleanup_shared_memory(COMMUNICATION_COMMAND_SHM_NAME, comm_fd, (void*) comm_mem, sizeof(CommunicationCommandMemory));
    exit(0);
}

void setup_signal_handlers() {
    signal(SIGINT, handle_termination);   // Ctrl+C
    signal(SIGTERM, handle_termination);  // kill
}

int main() {
	setup_signal_handlers();

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
    while (1) {
        comm_fd = shm_open(COMMUNICATION_COMMAND_SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (comm_fd != -1) {
            if (ftruncate(comm_fd, sizeof(CommunicationCommandMemory)) == -1) {
                perror("[CommunicationSystem] ftruncate failed for communication commands");
                exit(EXIT_FAILURE);
            }

            void* comm_addr = mmap(NULL, sizeof(CommunicationCommandMemory), PROT_READ | PROT_WRITE, MAP_SHARED, comm_fd, 0);
            if (comm_addr != MAP_FAILED) {
                comm_mem = static_cast<CommunicationCommandMemory*>(comm_addr);

                // Initialize the mutex for shared memory
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                pthread_mutex_init(&comm_mem->lock, &attr);
                comm_mem->command_count = 0;

                std::cout << "[CommunicationSystem] Successfully created and initialized CommunicationCommand shared memory\n";
                close(comm_fd);
                break;
            } else {
                perror("[CommunicationSystem] mmap failed for communication commands");
                close(comm_fd);
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
