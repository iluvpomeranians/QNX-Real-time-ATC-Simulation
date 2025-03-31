#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <stdbool.h>
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/airspace.h"
#include "../../DataTypes/operator_command.h"

#define FUTURE_OFFSET_SEC 120

Airspace* airspace;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;
OperatorCommandMemory* operator_commands = nullptr;

struct ViolationArgs {
    Airspace* shm_ptr;
    int total_aircraft;
    double elapsedTime;
};

OperatorCommandMemory* init_operator_command_memory() {

    int cmd_fd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (cmd_fd == -1) {
        perror("shm_open failed for operator commands");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(cmd_fd, sizeof(OperatorCommandMemory)) == -1) {
        perror("ftruncate failed for operator commands");
        exit(EXIT_FAILURE);
    }

    void* addr = mmap(NULL, sizeof(OperatorCommandMemory), PROT_READ | PROT_WRITE, MAP_SHARED, cmd_fd, 0);
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

    std::cout << "[ComputerSystem] Initialized OperatorCommand shared memory\n";
    return cmd_mem;
}

void* pollOperatorCommands(void* arg) {
    OperatorCommandMemory* cmd_mem = (OperatorCommandMemory*) arg;

    std::cout << "[ComputerSystem] Polling Operator Commands...\n";

    while (true) {
        pthread_mutex_lock(&cmd_mem->lock);

        for (int i = 0; i < cmd_mem->command_count; ++i) {
            OperatorCommand& cmd = cmd_mem->commands[i];

            std::cout << "[Received Command] Aircraft ID: " << cmd.aircraft_id
                      << " | Type: " << cmd.type
                      << " | Position: (" << cmd.position.x << ", " << cmd.position.y << ", " << cmd.position.z << ")"
                      << " | Speed: (" << cmd.speed.vx << ", " << cmd.speed.vy << ", " << cmd.speed.vz << ")"
                      << std::endl;

            // TODO: Dispatch send(R,m) to Comm subsystem
        }

        // Clear command list after processing?
        // We'll probably want to send to a Logger prior to deletion
        cmd_mem->command_count = 0;

        pthread_mutex_unlock(&cmd_mem->lock);

        sleep(1);
    }

    return NULL;
}

Airspace* init_airspace_shared_memory() {
    int shm_fd;
    void* addr = MAP_FAILED;

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

        sleep(1);
    }
}




void sendAlert(int aircraft1, int aircraft2) {
//	std::cout << "[ALERT]: Aircraft " << aircraft1
//	              << " and Aircraft " << aircraft2 << " are too close"
//	              << std::endl;
    // TODO: Implement alert mechanism (e.g., notify operator)
}

void sendIdToDisplay(int aircraft_id) {
    std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
    // TODO: Implement display mechanism (e.g., show ID on screen)
}

void getProjectedPosition(AircraftData& aircraft, double time) {
    aircraft.x += aircraft.speedX * time / 3600;
    aircraft.y += aircraft.speedY * time / 3600;
    aircraft.z += aircraft.speedZ * time / 3600;
}

void* checkCurrentViolations(void* args) {
    struct ViolationArgs* data = (struct ViolationArgs*) args;
    Airspace* l_airspace = data->shm_ptr;

    // TODO: Use shared mem variable instead
    int max = data->total_aircraft;

    time_t now = time(NULL);

    // TODO: Consider copying from shared memory and then releasing lock and
    //       doing calculations after to not monopolize shm
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


int main() {
    airspace = init_airspace_shared_memory();
    OperatorCommandMemory* operator_cmd_mem = init_operator_command_memory();

    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);

    pthread_t cmdThread;
    pthread_create(&cmdThread, NULL, pollOperatorCommands, operator_cmd_mem);

    while (true) sleep(10);

    munmap(airspace, sizeof(Airspace));
    munmap(operator_cmd_mem, sizeof(OperatorCommandMemory));

    return 0;
}

