#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctime>
#include <cstring>
#include "../../DataTypes/aircraft_data.h"

#define AIRSPACE_WIDTH 100000
#define AIRSPACE_HEIGHT 100000
#define DISPLAY_WIDTH 50
#define DISPLAY_HEIGHT 25

AircraftData* aircrafts = nullptr;

void clearScreen() {
    for (int i = 0; i < 24; ++i)
        std::cout << '\n';
}

void connectToSharedMemory() {
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        std::cerr << "Failed to open shared memory" << std::endl;
        exit(1);
    }

    void* addr = mmap(nullptr, sizeof(AircraftData) * MAX_AIRCRAFT,
                      PROT_READ, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory" << std::endl;
        close(shm_fd);
        exit(1);
    }

    aircrafts = static_cast<AircraftData*>(addr);
    std::cout << "[DataDisplaySystem] Shared memory successfully mapped\n";
    close(shm_fd);
}

char getDirectionArrow(float vx, float vy) {
    if (vx == 0 && vy > 0) return '^';
    if (vx == 0 && vy < 0) return 'v';
    if (vx > 0 && vy == 0) return '>';
    if (vx < 0 && vy == 0) return '<';
    if (vx > 0 && vy > 0) return '/';
    if (vx < 0 && vy > 0) return '\\';
    if (vx > 0 && vy < 0) return '\\';
    if (vx < 0 && vy < 0) return '/';
    return '+';
}

void drawAirspace() {
    char screen[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    memset(screen, '.', sizeof(screen));

    for (int i = 0; i < MAX_AIRCRAFT; ++i) {
        if (!aircrafts[i].detected) continue;

        int x = static_cast<int>((aircrafts[i].x / AIRSPACE_WIDTH) * DISPLAY_WIDTH);
        int y = static_cast<int>((aircrafts[i].y / AIRSPACE_HEIGHT) * DISPLAY_HEIGHT);

        if (x >= 0 && x < DISPLAY_WIDTH && y >= 1 && y < DISPLAY_HEIGHT) {
            std::string idStr = std::to_string(aircrafts[i].id);
            int len = idStr.length();

            int startX = x - len / 2;

            for (int j = 0; j < len; ++j) {
                int charX = startX + j;
                if (charX >= 0 && charX < DISPLAY_WIDTH) {
                    screen[y - 1][charX] = idStr[j];
                }
            }

            screen[y][x] = '+';
        }
    }

    for (int col = 0; col < DISPLAY_WIDTH; ++col){
    	std::cout << "=";
    }

    std::cout << "==\n";

    for (int row = 0; row < DISPLAY_HEIGHT; ++row) {
    	std::cout << "|" ;
        for (int col = 0; col < DISPLAY_WIDTH; ++col) {
            std::cout << screen[row][col];
            if (col == DISPLAY_WIDTH - 1){
            	std::cout << "|";
            }
        }
        std::cout << '\n';
    }

    for (int col = 0; col < DISPLAY_WIDTH; ++col){
        	std::cout << "=";
        }

        std::cout << "==\n";
}

int main() {
    connectToSharedMemory();
    drawAirspace();
    std::time_t lastUpdate = 0;
    while (true) {
        std::time_t now = std::time(nullptr);
        if (now - lastUpdate >= 1) {
        	clearScreen();
            drawAirspace();
            lastUpdate = now;
        }
        usleep(100000);
    }

    return 0;
}
