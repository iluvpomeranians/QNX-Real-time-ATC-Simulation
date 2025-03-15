#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#define SHM_NAME "/air_traffic_shm"
#define MAX_AIRCRAFT 100

struct AircraftData {
    int id;
    double x, y, z;
    double speedX, speedY, speedZ;
    bool detected;
    bool responded;
};

AircraftData* aircrafts;
int shm_fd;

void fake_aircraft_data() {
    std::cout << "Initializing fake aircraft data...\n";

    AircraftData fakeData[MAX_AIRCRAFT] = {
        {101, 10000, 20000, 15000, 250, 300, 0, true, true},
        {102, 12000, 22000, 17000, -200, -250, 0, true, true},
        {103, 9000, 18000, 13000, 300, 200, 50, true, true},
        {104, 15000, 25000, 16000, 100, 150, -30, true, true},
        {105, 8000, 19000, 14000, 275, -225, 20, true, true},
        {106, 11000, 23000, 15500, 125, -175, 35, true, true},
        {107, 9500, 21000, 14500, 400, 350, -40, true, true},
        {108, 13000, 24000, 16500, 50, -100, 15, true, true},
        {109, 14000, 26000, 17500, -350, -275, 10, true, true},
        {110, 10500, 22000, 15800, 200, 300, -50, true, true}
    };

    for (int i = 0; i < 10; i++) {
        aircrafts[i] = fakeData[i];
        std::cout << "Loaded Fake Aircraft ID: " << fakeData[i].id
                  << " at (" << fakeData[i].x << ", " << fakeData[i].y
                  << ", " << fakeData[i].z << ")\n";
    }
}

void verify_aircraft_data() {
    std::cout << "Verifying shared memory contents...\n";
    for (int i = 0; i < 10; i++) {
        std::cout << "Aircraft ID: " << aircrafts[i].id
                  << " Position: (" << aircrafts[i].x << ", " << aircrafts[i].y << ", " << aircrafts[i].z << ")"
                  << " Speed: (" << aircrafts[i].speedX << ", " << aircrafts[i].speedY << ", " << aircrafts[i].speedZ << ")\n";
    }
    std::cout << "Verification complete.\n";
}

int main() {

    // create the shared mem
	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	    if (shm_fd == -1) {
	        perror("shm_open failed");
	        return 1;
	    }

    if (ftruncate(shm_fd, sizeof(AircraftData) * MAX_AIRCRAFT) == -1) {
            perror("ftruncate failed");
            return 1;
        }

    aircrafts = (AircraftData*) mmap(NULL,
    								sizeof(AircraftData) * MAX_AIRCRAFT,
                                    PROT_READ | PROT_WRITE,
									MAP_SHARED,
									shm_fd, 0);
    if (aircrafts == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    fake_aircraft_data();
    verify_aircraft_data();

    std::cout << "Radar System initialized. Shared memory ready.\n";

    while (true) sleep(10);

    munmap(aircrafts, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd);

    return 0;
}
