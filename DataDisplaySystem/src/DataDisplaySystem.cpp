#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
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

#define AIRSPACE_WIDTH 100000
#define AIRSPACE_HEIGHT 100000
#define DISPLAY_WIDTH 50
#define DISPLAY_HEIGHT 14

AircraftData* aircrafts = nullptr;
std::map<int, char> blipMap;

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
    std::vector<AircraftData> activeAircrafts;

    activeAircrafts.clear();
    for (int i = 0; i < MAX_AIRCRAFT; ++i) {
        if (!aircrafts[i].detected){
        	continue;
        }

        activeAircrafts.push_back(aircrafts[i]);

        int x = static_cast<int>((aircrafts[i].x / AIRSPACE_WIDTH) * DISPLAY_WIDTH);
        int y = static_cast<int>((aircrafts[i].y / AIRSPACE_HEIGHT) * DISPLAY_HEIGHT);

        if (aircrafts[i].z < 15000 && aircrafts[i].z > 25000){
        	activeAircrafts.erase(
        	    std::remove_if(
        	        activeAircrafts.begin(),
        	        activeAircrafts.end(),
        	        [&](const AircraftData& a) {
        	            return a.id == aircrafts[i].id;
        	        }
        	    ),
        	    activeAircrafts.end()
        	);
        }

        if (x == 0 ){
        	x = 1;
        }

        if (y < 1){
        	y = 2;
        }

        if (x >= 0 && x < DISPLAY_WIDTH && y >= 1 && y < DISPLAY_HEIGHT
        	&& (aircrafts[i].z >= 15000 && aircrafts[i].z <= 25000)) {
            std::string idStr = std::to_string(aircrafts[i].id);
            int len = idStr.length();

            int startX = x - len / 2;

            bool tag_space_available = true;

            for (int h = 0; h < len; ++h) {
                int charX = startX + h;
                if (charX < 0 || charX >= DISPLAY_WIDTH || screen[y - 1][charX] != '.') {
                    tag_space_available = false;
                    break;
                }
            }

			if (tag_space_available){
				for (int j = 0; j < len; ++j) {
					int charX = startX + j;
					if (charX >= 0 && charX < DISPLAY_WIDTH) {
//						screen[y - 1][charX] = idStr[j]; //TODO: floating ID's above
					}
				}

			}
//			screen[y][x] = '+';

			if (blipMap.find(aircrafts[i].id) == blipMap.end()) {
			    blipMap[aircrafts[i].id] = 'a' + i;
			}

			if (screen[y][x] == '.'){
				screen[y][x] = blipMap[aircrafts[i].id];
			}
			else{
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

int main() {
    connectToSharedMemory();
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
