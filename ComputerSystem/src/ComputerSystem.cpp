#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <stdbool.h>
#include <csignal>
#include <sys/dispatch.h>
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/airspace.h"
#include "../../DataTypes/operator_command.h"
#include "../../DataTypes/communication_system.h"

#define FUTURE_OFFSET_SEC 120
Airspace* airspace;
OperatorCommandMemory* operator_cmd_mem = nullptr;
bool operator_cmd_initialized = false;
bool commands_available = false;

int comm_system_pid = -1;
int operator_cmd_fd;

struct ViolationArgs {
    Airspace* shm_ptr;
    int total_aircraft;
    double elapsedTime;
};


OperatorCommandMemory* init_operator_command_memory() {

    operator_cmd_fd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (operator_cmd_fd == -1) {
        perror("shm_open failed for operator commands");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(operator_cmd_fd, sizeof(OperatorCommandMemory)) == -1) {
        perror("ftruncate failed for operator commands");
        exit(EXIT_FAILURE);
    }

    void* addr = mmap(NULL, sizeof(OperatorCommandMemory), PROT_READ | PROT_WRITE, MAP_SHARED, operator_cmd_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed for operator commands");
        exit(EXIT_FAILURE);
    }

    OperatorCommandMemory* cmd_mem = static_cast<OperatorCommandMemory*>(addr);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&cmd_mem->lock, &attr);
    cmd_mem->command_count = 0;

    operator_cmd_initialized = true;

    std::cout << "[ComputerSystem] Initialized OperatorCommand shared memory\n";
    return cmd_mem;
}

void* pollOperatorCommands(void* arg) {
    OperatorCommandMemory* cmd_mem = (OperatorCommandMemory*) arg;

    std::cout << "[ComputerSystem] Polling Operator Commands...\n";

    int comm_fd;
    void* addr = MAP_FAILED;
    struct timespec wait_time = {1, 0};  // 1 second

    std::cout << "[ComputerSystem] Waiting for CommunicationCommand shared memory to become available...\n";

    while (1) {
        comm_fd = shm_open(COMMUNICATION_COMMAND_SHM_NAME, O_RDWR, 0666);
        if (comm_fd != -1) {
            addr = mmap(NULL, sizeof(CommunicationCommandMemory), PROT_READ | PROT_WRITE, MAP_SHARED, comm_fd, 0);
            if (addr != MAP_FAILED) {
                std::cout << "[ComputerSystem] Successfully connected to CommunicationCommand shared memory\n";
                close(comm_fd);
                break;
            } else {
                perror("[ComputerSystem] mmap failed for CommunicationCommand shared memory");
                close(comm_fd);
            }
        }
        nanosleep(&wait_time, NULL);

    }

    CommunicationCommandMemory* comm_mem = static_cast<CommunicationCommandMemory*>(addr);
    comm_system_pid = comm_mem->comm_pid;
    std::cout << "[ComputerSystem] Retrieved CommunicationSystem PID: " << comm_system_pid << "\n";

    while (true) {
        pthread_mutex_lock(&cmd_mem->lock);

        for (int i = 0; i < cmd_mem->command_count; ++i) {
            OperatorCommand& cmd = cmd_mem->commands[i];

            std::cout << "[Received Command] Aircraft ID: " << cmd.aircraft_id
                      << " | Type: " << cmd.type
                      << " | Position: (" << cmd.position.x << ", " << cmd.position.y << ", " << cmd.position.z << ")"
                      << " | Speed: (" << cmd.speed.vx << ", " << cmd.speed.vy << ", " << cmd.speed.vz << ")"
                      << std::endl;

            pthread_mutex_lock(&comm_mem->lock);
            comm_mem->commands[comm_mem->command_count] = cmd;
            printf("[CommunicationSystem] Command stored: Aircraft ID: %d | Type: %d | Position: (%.2f, %.2f, %.2f) | Speed: (%.2f, %.2f, %.2f)\n",
                   cmd.aircraft_id,
                   cmd.type,
                   cmd.position.x, cmd.position.y, cmd.position.z,
                   cmd.speed.vx, cmd.speed.vy, cmd.speed.vz);

            comm_mem->command_count+=1;
            pthread_mutex_unlock(&comm_mem->lock);

            // Set commands_available to true after storing the command
            commands_available = true;
        }

        if (commands_available && comm_system_pid > 0) {
            std::cout << "[ComputerSystem] Sending SIGUSR1 to PID " << comm_system_pid << "...\n";
            if (kill(comm_system_pid, SIGUSR1) == -1) {
                 perror("[ComputerSystem] Error sending SIGUSR1");
            }

            cmd_mem->command_count = 0;
            commands_available = false;
        }

        pthread_mutex_unlock(&cmd_mem->lock);

        nanosleep(&wait_time, NULL);

    }



    return NULL;
}


Airspace* init_airspace_shared_memory() {
    int shm_fd;
    void* addr = MAP_FAILED;
    struct timespec wait_time = {1, 0};  // 1 second

    printf("[ComputerSystem] Waiting for Airspace shared memory to become available...\n");

    while (1) {
        shm_fd = shm_open(AIRSPACE_SHM_NAME, O_RDWR, 0666);
        if (shm_fd != -1) {
            addr = mmap(NULL, sizeof(Airspace), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (addr != MAP_FAILED) {
                printf("[ComputerSystem] Successfully connected to Airspace shared memory\n");
                close(shm_fd);
                return (Airspace*)addr;
            } else {
                perror("[Airspace] mmap failed");
                close(shm_fd);
            }
        }

        nanosleep(&wait_time, NULL);

    }
}




void sendAlert(int aircraft1, int aircraft2) {
    char message[100];
    snprintf(message, sizeof(message),
             "ALERT: Aircraft %d and Aircraft %d are too close!", aircraft1, aircraft2);

    int coid = name_open(OPERATOR_VIOLATIONS_CHANNEL_NAME, 0);
    if (coid == -1) {
        perror("[ComputerSystem] Failed to connect to OperatorConsole IPC channel");
        return;
    }

    int status = MsgSend(coid, message, sizeof(message), NULL, 0);
    if (status == -1) {
        perror("[ComputerSystem] Failed to send alert to OperatorConsole");
    } else {
    	//TURN ON ALERTS
        //std::cout << "[ComputerSystem] Alert sent to OperatorConsole: " << message << std::endl;
    }

    name_close(coid);
}

void getProjectedPosition(AircraftData& aircraft, double time) {
    aircraft.x += aircraft.speedX * time / 3600;
    aircraft.y += aircraft.speedY * time / 3600;
    aircraft.z += aircraft.speedZ * time / 3600;
}

void* checkCurrentViolations(void* args) {
    struct ViolationArgs* data = (struct ViolationArgs*) args;
    Airspace* l_airspace = data->shm_ptr;

    int max = data->total_aircraft;

    time_t now = time(NULL);

    pthread_mutex_lock(&airspace->lock);

    for (int i = 0; i < max; i++) {
        AircraftData* a1 = &l_airspace->aircraft_data[i];
        if (a1->id == 0 || now < a1->entryTime) continue;

        for (int j = i + 1; j < max; j++) {
            AircraftData* a2 = &l_airspace->aircraft_data[j];
            if (a2->id == 0 || now < a2->entryTime) continue;

            double dx = fabs(a1->x - a2->x);
            double dy = fabs(a1->y - a2->y);
            double dz = fabs(a1->z - a2->z);
            double horizontal = sqrt(dx * dx + dy * dy);

//            std::cout << "[ComputerSystem] Comparing Aircraft " << a1->id << " and " << a2->id << "\n"
//                                  << "    A1 Pos: (" << a1->x << ", " << a1->y << ", " << a1->z << ")\n"
//                                  << "    A2 Pos: (" << a2->x << ", " << a2->y << ", " << a2->z << ")\n"
//                                  << "    Horizontal: " << horizontal << "m, Vertical (Z): " << dz << "m\n";

            if (horizontal < 3000 || dz < 1000) {
                sendAlert(a1->id, a2->id);
            }
        }
    }

    pthread_mutex_unlock(&airspace->lock);
    return NULL;
}


void* checkFutureViolations(void* args) {
    struct ViolationArgs* data = (struct ViolationArgs*) args;
    Airspace* l_airspace = data->shm_ptr;
    int max = data->total_aircraft;

    time_t now = time(NULL);

    pthread_mutex_lock(&airspace->lock);

    for (int i = 0; i < max; i++) {
        AircraftData* a1 = &l_airspace->aircraft_data[i];

        if (a1->id == 0 || now < a1->entryTime) continue;

        for (int j = i + 1; j < max; j++) {
            AircraftData* a2 = &l_airspace->aircraft_data[j];
            if (a2->id == 0 || now < a2->entryTime) continue;

            // Project both aircraft 2 minutes ahead
            a1->x += a1->speedX * FUTURE_OFFSET_SEC / 3600;
            a1->y += a1->speedY * FUTURE_OFFSET_SEC / 3600;
            a1->z += a1->speedZ * FUTURE_OFFSET_SEC / 3600;

            a2->x += a2->speedX * FUTURE_OFFSET_SEC / 3600;
            a2->y += a2->speedY * FUTURE_OFFSET_SEC / 3600;
            a2->z += a2->speedZ * FUTURE_OFFSET_SEC / 3600;

            double dx = fabs(a1->x - a2->x);
            double dy = fabs(a1->y - a2->y);
            double dz = fabs(a1->z - a2->z);
            double horizontal = sqrt(dx * dx + dy * dy);

//            std::cout << "[ComputerSystem] Comparing Aircraft " << a1->id << " and " << a2->id << "\n"
//                      << "    A1 Pos: (" << a1->x << ", " << a1->y << ", " << a1->z << ")\n"
//                      << "    A2 Pos: (" << a2->x << ", " << a2->y << ", " << a2->z << ")\n"
//                      << "    Horizontal: " << horizontal << "m, Vertical (Z): " << dz << "m\n";


            if (horizontal < 3000 || dz < 1000) {
                sendAlert(a1->id, a2->id);
            }
        }
    }

    pthread_mutex_unlock(&airspace->lock);
    return NULL;
}


void* violationCheck(void* arg) {
    struct timespec ts = {5, 0};

    std::cout << "[ComputerSystem] Checking Violations...\n";

    while (1) {

        struct ViolationArgs args = {
            .shm_ptr = airspace,
            .total_aircraft = MAX_AIRCRAFT,
        };

        pthread_t currentThread, futureThread;

        pthread_create(&currentThread, NULL, checkCurrentViolations, &args);
        pthread_create(&futureThread, NULL, checkFutureViolations, &args);

        pthread_join(currentThread, NULL);
        pthread_join(futureThread, NULL);

        nanosleep(&ts, NULL);
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

	if (operator_cmd_initialized == true){
		if (shm_unlink(shm_name) == 0) {
				std::cout << "Shared memory unlinked successfully." << std::endl;
		} else {
			perror("shm_unlink failed");
		}
	}

}

void handle_termination(int signum) {
    std::cout << "[ComputerSystem]" << " cleaning up operator shared memory...\n";
    cleanup_shared_memory(OPERATOR_COMMAND_SHM_NAME, operator_cmd_fd, (void*) operator_cmd_mem, sizeof(OperatorCommandMemory));
    exit(0);
}

void setup_signal_handlers() {
    signal(SIGINT, handle_termination);   // Ctrl+C
    signal(SIGTERM, handle_termination);  // kill
}

int main() {

	setup_signal_handlers();
    airspace = init_airspace_shared_memory();
    operator_cmd_mem = init_operator_command_memory();

    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);

    pthread_t cmdThread;
    pthread_create(&cmdThread, NULL, pollOperatorCommands, operator_cmd_mem);

    struct timespec sleep_forever = {10, 0};
    while (true) nanosleep(&sleep_forever, NULL);

    munmap(airspace, sizeof(Airspace));
    munmap(operator_cmd_mem, sizeof(OperatorCommandMemory));

    return 0;
}

