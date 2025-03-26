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

OperatorCommandMemory* connect_to_operator_command_memory() {
    int shm_fd = shm_open(OPERATOR_COMMAND_SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    void* addr = mmap(NULL, sizeof(OperatorCommandMemory),
                      PROT_READ | PROT_WRITE, MAP_SHARED,
                      shm_fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "[OperatorConsole] Connected to shared memory for commands.\n";
    close(shm_fd);
    return static_cast<OperatorCommandMemory*>(addr);
}

void handle_received_command(const std::string& raw_cmd) {
    // Simple parser for command format:
    // "changeSpeed 101 100 0 0"
    std::istringstream ss(raw_cmd);
    std::string type;
    int aircraft_id;
    double x, y, z;

    ss >> type >> aircraft_id >> x >> y >> z;

    OperatorCommand cmd;
    if (type == "changeSpeed") {
        cmd.type = CommandType::ChangeSpeed;
        cmd.speed = {x, y, z};
    } else if (type == "changePosition") {
        cmd.type = CommandType::ChangePosition;
        cmd.position = {x, y, z};
    } else if (type == "requestDetails") {
        cmd.type = CommandType::RequestDetails;
        cmd.position = {0, 0, 0}; // not used
    } else {
        std::cerr << "[OperatorConsole] Unknown command type: " << type << std::endl;
        return;
    }

    cmd.aircraft_id = aircraft_id;
    cmd.timestamp = time(NULL);

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

int main() {
    operator_cmd_mem = connect_to_operator_command_memory();

    // Create IPC channel
    name_attach_t* attach = name_attach(NULL, OPERATOR_CONSOLE_CHANNEL_NAME, 0);
    if (attach == NULL) {
        perror("name_attach failed");
        return 1;
    }

    std::cout << "[OperatorConsole] Listening for commands on channel ID " << attach->chid << "\n";

    struct {
        struct _pulse pulse;
        char data[256];
    } msg;

    while (true) {
        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive failed");
            continue;
        }

        if (rcvid == 0) continue;

        msg.data[255] = '\0';
        std::string received_cmd(msg.data);
        std::cout << "[OperatorConsole] Received command: " << received_cmd << "\n";

        handle_received_command(received_cmd);

        MsgReply(rcvid, 0, NULL, 0);
    }

    return 0;
}
