#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/dispatch.h>
#include "../../DataTypes/operator_command.h"

OperatorCommandMemory* operator_cmd_mem = nullptr;

name_attach_t* init_operator_violation_channel() {
    name_attach_t* attach = name_attach(NULL, OPERATOR_VIOLATIONS_CHANNEL_NAME, 0);
    if (attach == NULL) {
        perror("[ComputerSystem] Failed to attach operator violations channel");
        exit(EXIT_FAILURE);
    }

    std::cout << "[ComputerSystem] IPC channel for operator violations established: "
              << OPERATOR_VIOLATIONS_CHANNEL_NAME << std::endl;

    return attach;
}

OperatorCommandMemory* connect_to_operator_command_memory() {
    int shm_fd;
    void* addr = MAP_FAILED;

    while (true) {
        shm_fd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_RDWR, 0666);
        if (shm_fd != -1) {
            addr = mmap(NULL, sizeof(OperatorCommandMemory),
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        shm_fd, 0);
            if (addr != MAP_FAILED) {
                std::cout << "[OperatorConsole] Connected to shared memory for commands.\n";
                close(shm_fd);
                return static_cast<OperatorCommandMemory*>(addr);
            } else {
                perror("[OperatorConsole] mmap failed");
                close(shm_fd);
            }
        } else {
            perror("[OperatorConsole] shm_open failed (retrying)");
        }

        sleep(1);
    }
}


void handle_received_command(const std::string& raw_cmd) {
    const double UNSET = -1.0;

    std::istringstream ss(raw_cmd);
    std::string type;
    int aircraft_id;
    double x = UNSET, y = UNSET, z = UNSET;

    ss >> type >> aircraft_id >> x >> y >> z;

    OperatorCommand cmd = {};
    cmd.aircraft_id = aircraft_id;
    cmd.timestamp = time(NULL);

    if (type == "ChangeSpeed") {
        cmd.type = CommandType::ChangeSpeed;
        cmd.speed = {x, y, z};
        cmd.position = {UNSET, UNSET, UNSET};
    } else if (type == "ChangePosition") {
        cmd.type = CommandType::ChangePosition;
        cmd.position = {x, y, z};
        cmd.speed = {UNSET, UNSET, UNSET};
    } else if (type == "RequestDetails") {
        cmd.type = CommandType::RequestDetails;
        cmd.position = {UNSET, UNSET, UNSET};
        cmd.speed = {UNSET, UNSET, UNSET};
    } else if (type == "ALERT:"){

    }
    else {
        std::cerr << "[OperatorConsole] Unknown command type: " << type << std::endl;
        return;
    }

    pthread_mutex_lock(&operator_cmd_mem->lock);
    if (operator_cmd_mem->command_count < MAX_OPERATOR_COMMANDS) {
        operator_cmd_mem->commands[operator_cmd_mem->command_count++] = cmd;
        operator_cmd_mem->updated = true;

        std::cout << "[OperatorConsole] Stored command for aircraft " << cmd.aircraft_id << std::endl;
    } else {
        std::cerr << "[OperatorConsole] Command memory full.\n";
    }
    pthread_mutex_unlock(&operator_cmd_mem->lock);
}


void* ipcListenerThread(void* arg) {
    name_attach_t* attach = static_cast<name_attach_t*>(arg);

    char msg[256];

    while (true) {
        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive failed");
            continue;
        }

        if (rcvid == 0) continue;

        msg[255] = '\0';
        std::string received_cmd(msg);

        if (received_cmd.find("ALERT") == std::string::npos) {
        	std::cout << "\n[OperatorConsole] Received command via IPC: " << received_cmd << std::endl;
            handle_received_command(received_cmd);  // Only called if "ALERT" is NOT found
        }
        else{
        	//TURN ON ALERT
        	//std::cout << "\n[OperatorConsole] Received Alert via IPC: " << received_cmd << std::endl;

        }

        MsgReply(rcvid, 0, NULL, 0);
    }

    return nullptr;
}


int main() {

    operator_cmd_mem = connect_to_operator_command_memory();

    // Attach channel
    name_attach_t* attach = name_attach(NULL, OPERATOR_CONSOLE_CHANNEL_NAME, 0);
    if (attach == NULL) {
        perror("name_attach failed");
        return 1;
    }

    std::cout << "[OperatorConsole] Listening for commands on channel ID " << attach->chid << "\n";

    pthread_t listenerThread;
    pthread_create(&listenerThread, nullptr, ipcListenerThread, attach);

    name_attach_t* operator_violation_channel = nullptr;
    operator_violation_channel = init_operator_violation_channel();
    pthread_t violationListenerThread;
    pthread_create(&violationListenerThread, nullptr, ipcListenerThread, operator_violation_channel);

    // Main thread handles stdin input for testing
    std::string input;
    while (true) {
        std::cout << "[OperatorConsole] > ";
        std::getline(std::cin, input);

        if (input == "exit") break;

        if (!input.empty()) {
            handle_received_command(input);
        }
    }

    name_detach(attach, 0);
    return 0;
}

