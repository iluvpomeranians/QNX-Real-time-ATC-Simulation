#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/dispatch.h>
#include <unistd.h>
#include <ctime>
#include <vector>
#include <cstring>
#include <iomanip>
#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include "../../DataTypes/aircraft_data.h"
#include "../../DataTypes/airspace.h"
#include "../../DataTypes/operator_command.h"

#define AIRSPACE_WIDTH 100000
#define AIRSPACE_HEIGHT 100000
#define DISPLAY_WIDTH 50
#define DISPLAY_HEIGHT 14

Airspace* airspace = nullptr;
std::map<int, char> blipMap;
int operator_coid = -1;

void clearScreen() {
    for (int i = 0; i < 24; ++i)
        std::cout << '\n';
}

void connectToSharedMemory() {
    std::cout << "[DataDisplaySystem] Waiting for Airspace shared memory to be created...\n";
    while (true) {
        int shm_fd = shm_open(AIRSPACE_SHM_NAME, O_RDONLY, 0666);
        if (shm_fd == -1) {
            sleep(1);
            continue;
        }

        void* addr = mmap(nullptr, sizeof(Airspace), PROT_READ, MAP_SHARED, shm_fd, 0);
        if (addr == MAP_FAILED) {
            close(shm_fd);
            sleep(1);
            continue;
        }

        airspace = static_cast<Airspace*>(addr);
        std::cout << "[DataDisplaySystem] Shared memory successfully mapped\n";
        close(shm_fd);
        break;
    }
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
    std::vector<AircraftData> activeAircrafts;

    activeAircrafts.clear();

    // TODO: Consider copying from shared memory and then releasing lock and
    //       doing calculations after to not monopolize shm
    for (int i = 0; i < airspace->aircraft_count; ++i) {
        AircraftData& aircraft = airspace->aircraft_data[i];

        if (!aircraft.detected) continue;
        if (aircraft.z < 15000 || aircraft.z > 25000) continue;

        int x = static_cast<int>((aircraft.x / AIRSPACE_WIDTH) * DISPLAY_WIDTH);
        int y = static_cast<int>((aircraft.y / AIRSPACE_HEIGHT) * DISPLAY_HEIGHT);

        if (x == 0) x = 1;
        if (y < 1) y = 2;

        if (x >= 0 && x < DISPLAY_WIDTH && y >= 1 && y < DISPLAY_HEIGHT) {
            activeAircrafts.push_back(aircraft);

            // Assign BLIP symbol if new
            if (blipMap.find(aircraft.id) == blipMap.end()) {
                blipMap[aircraft.id] = 'a' + i;
            }

            if (screen[y][x] == '.') {
                screen[y][x] = blipMap[aircraft.id];
            } else {
                screen[y][x] = '+';
            }
        }
    }


    // Draw Top Border
       for (int col = 0; col < (DISPLAY_WIDTH * 2) + 31; ++col) std::cout << "=";
       std::cout << "===\n";

       std::cout << "|" << std::setw(DISPLAY_WIDTH) << std::left << " AIRSPACE ";
       std::cout << "||   "
          << std::setw(8) << std::left << "BLIP"
		  << std::setw(8) << "ID"
          << std::setw(8) << "X"
          << std::setw(8) << "Y"
          << std::setw(8) << "Z"
          << std::setw(6) << "VX"
          << std::setw(6) << "VY"
          << std::setw(4) << "VZ"
		  << std::setw(10) << "Entry"
		  << std::setw(10) << "LastUpdate"
          << " |\n";

       // Draw Grid + Dashboard
       for (int row = 0; row < DISPLAY_HEIGHT; ++row) {
           std::cout << "|";
           for (int col = 0; col < DISPLAY_WIDTH; ++col) {
               std::cout << screen[row][col];
           }

           std::cout << "|";

           if (row < static_cast<int>(activeAircrafts.size())) {
            const AircraftData& a = activeAircrafts[row];
            std::ostringstream entryTimeStr, lastUpdatedStr;
            entryTimeStr << std::put_time(std::localtime(&a.entryTime), "%H:%M:%S");
            lastUpdatedStr << std::put_time(std::localtime(&a.lastupdatedTime), "%H:%M:%S");
            std::cout << "|   "
                      << std::setw(8) << std::left << blipMap[a.id]
					  << std::setw(8) << a.id
                      << std::setw(8) << static_cast<int>(a.x)
                      << std::setw(8) << static_cast<int>(a.y)
                      << std::setw(8) << static_cast<int>(a.z)
                      << std::setw(6) << static_cast<int>(a.speedX)
                      << std::setw(6) << static_cast<int>(a.speedY)
                      << std::setw(4) << static_cast<int>(a.speedZ)
					  << std::setw(10) << entryTimeStr.str()
					  << std::setw(10) << lastUpdatedStr.str()
                      << " |\n";
            } else {
                std::cout << "|   "
                        << std::setw(8) << " "
						<< std::setw(8) << " "
                        << std::setw(8) << " "
                        << std::setw(8) << " "
                        << std::setw(8) << " "
                        << std::setw(6) << " "
                        << std::setw(6) << " "
                        << std::setw(4) << " "
						<< std::setw(10) << " "
						<< std::setw(10) << " "
                        << " |\n";
            }
        
           std::cout << "|\n";
       }

       for (int col = 0; col < (DISPLAY_WIDTH * 2) + 31; ++col) std::cout << "=";
       std::cout << "===\n";
  }

void setupOperatorConsoleConnection() {
	while ((operator_coid = name_open(OPERATOR_CONSOLE_CHANNEL_NAME, 0)) == -1) {
		std::cout << "Waiting for server " << OPERATOR_CONSOLE_CHANNEL_NAME << "to start...\n";

		// TODO: (Optional) Use better timer
		sleep(0.5);
	}
	std::cout << "Connected to server '" << OPERATOR_CONSOLE_CHANNEL_NAME << "'\n";

    std::cout << "[DataDisplay] Connected to OperatorConsole channel.\n";
}

void promptAndSendCommand() {
    std::string input;
    std::cout << "\n[Operator Console] > ";
    std::getline(std::cin, input);

    if (input.empty()) return;

    if (operator_coid != -1) {
        int status = MsgSend(operator_coid, input.c_str(), input.size() + 1, nullptr, 0);
        if (status == -1) {
            perror("[DataDisplay] Failed to send message to OperatorConsole");
        } else {
            std::cout << "[DataDisplay] Command sent successfully.\n";
        }
    }
}

void* commandInputThread(void*) {
    while (true) {
        promptAndSendCommand();
        usleep(100000);
    }
    return nullptr;
}


int main() {
    connectToSharedMemory();
    setupOperatorConsoleConnection();
    std::time_t lastUpdate = 0;
    pthread_t inputThread;
    pthread_create(&inputThread, nullptr, commandInputThread, nullptr);
    while (true) {
        std::time_t now = std::time(nullptr);
        if (now - lastUpdate >= 1) {
        	clearScreen();
            drawAirspace();
//            promptAndSendCommand();
            lastUpdate = now;
        }
        usleep(100000);
    }

    return 0;
}
