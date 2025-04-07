#ifndef OPERATOR_COMMAND_H
#define OPERATOR_COMMAND_H

#include <pthread.h>
#include <ctime>

#define MAX_OPERATOR_COMMANDS 100
#define OPERATOR_COMMAND_SHM_NAME "/operator_commands"
#define OPERATOR_CONSOLE_CHANNEL_NAME "operator_console"
#define OPERATOR_VIOLATIONS_CHANNEL_NAME "operator_violations"

enum CommandType {
    ChangePosition,
    ChangeSpeed,
    RequestDetails
};

struct Position {
    double x;
    double y;
    double z;
};

struct Speed {
    double vx;
    double vy;
    double vz;
};

struct OperatorCommand {
    CommandType type;
    int aircraft_id;
    Position position;
    Speed speed;
    time_t timestamp;
};

struct OperatorCommandMemory {
    pthread_mutex_t lock;
    int command_count;
    bool updated;
    OperatorCommand commands[MAX_OPERATOR_COMMANDS];
};

#endif // OPERATOR_COMMAND_H
