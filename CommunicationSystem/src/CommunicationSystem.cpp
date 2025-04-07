#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <cstring>
#include <ctime>
#include <csignal>
#include "../../DataTypes/message_types.h"
#include "../../DataTypes/operator_command.h"
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/aircraft.h"
#include "../../DataTypes/communication_system.h"

using namespace std;

OperatorCommandMemory* operator_cmd_mem = nullptr;
CommunicationCommandMemory* comm_mem = nullptr;
volatile sig_atomic_t signal_received = 0;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int comm_fd;

//Tutorial #3
void signal_handler(int signo) {
    if (signo == SIGUSR1) {
    	pthread_cond_signal(&cond);
    }
}

OperatorCommandMemory* init_operator_command_memory() {
    int shm_fd;
    struct timespec one_sec = {1, 0};  // 1 second, 0 nanoseconds

    while (true) {
        shm_fd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_RDWR, 0666);
        if (shm_fd != -1) {
            void* addr = mmap(NULL, sizeof(OperatorCommandMemory),
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              shm_fd, 0);
            if (addr != MAP_FAILED) {
                std::cout << "[CommunicationSystem] Connected to OperatorCommand shared memory.\n";
                close(shm_fd);
                return static_cast<OperatorCommandMemory*>(addr);
            } else {
                perror("[CommunicationSystem] mmap failed for OperatorCommand");
                close(shm_fd);
            }
        }
        nanosleep(&one_sec, NULL);
    }
}

CommunicationCommandMemory* init_communication_command_memory() {
    struct timespec one_sec = {1, 0};  // 1 second, 0 nanoseconds

    while (true) {
        comm_fd = shm_open(COMMUNICATION_COMMAND_SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (comm_fd != -1) {
            if (ftruncate(comm_fd, sizeof(CommunicationCommandMemory)) == -1) {
                perror("[CommunicationSystem] ftruncate failed");
                exit(EXIT_FAILURE);
            }

            void* addr = mmap(NULL, sizeof(CommunicationCommandMemory),
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              comm_fd, 0);
            if (addr != MAP_FAILED) {
                auto* mem = static_cast<CommunicationCommandMemory*>(addr);

                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                pthread_mutex_init(&mem->lock, &attr);
                mem->command_count = 0;

                std::cout << "[CommunicationSystem] Created and initialized CommunicationCommand shared memory.\n";
                mem->comm_pid = getpid();
                close(comm_fd);
                return mem;
            } else {
                perror("[CommunicationSystem] mmap failed");
                close(comm_fd);
            }
        }
        nanosleep(&one_sec, NULL);
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

    message_t msg;
    msg.aircraft_id = aircraft_id;
    msg.type = OPERATOR_TYPE;
    msg.message.operator_command = cmd;

    int status = MsgSend(coid, &msg, sizeof(msg), nullptr, 0);
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
            std::cout << "[CommunicationSystem] Command sent to Aircraft ID " << cmd.aircraft_id << std::endl;

            std::cout << "[Received Command] Aircraft ID: " << cmd.aircraft_id
                      << " | Type: " << cmd.type
                      << " | Position: (" << cmd.position.x << ", " << cmd.position.y << ", " << cmd.position.z << ")"
                      << " | Speed: (" << cmd.speed.vx << ", " << cmd.speed.vy << ", " << cmd.speed.vz << ")"
                      << std::endl;

            send_command_to_aircraft(cmd.aircraft_id, cmd);
        }

        cmd_mem->command_count = 0;

        pthread_mutex_unlock(&cmd_mem->lock);
    }

    return NULL;
}

void cleanup_shared_memory(const char* shm_name, int shm_fd, void* addr, size_t size) {
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

using namespace std;

//Communicate via open channels to each aircraft
int main() {
    setup_signal_handlers();
    struct timespec one_sec = {1, 0};  // 1 second, 0 nanoseconds

    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        perror("[CommunicationSystem] Failed to set signal handler");
        exit(EXIT_FAILURE);
    }

    operator_cmd_mem = init_operator_command_memory();
    comm_mem = init_communication_command_memory();

    pthread_t command_thread;
    pthread_create(&command_thread, NULL, pollOperatorCommands, operator_cmd_mem);

    while (true) nanosleep(&one_sec, NULL);

    return 0;
}

