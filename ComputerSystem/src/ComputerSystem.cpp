#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cmath>
#include <fstream>
#include <unistd.h>
#include <chrono>
#include <stdbool.h>
#include "../../DataTypes/aircraft_data.h"

// Shared memory pointer and mutex for synchronization
AircraftData* aircrafts_shared_memory;
pthread_mutex_t shm_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to store shared memory limits
struct SharedMemoryLimits {
    void* lower_limit;  // Pointer to the start of shared memory
    size_t size;        // Size of the shared memory
};

SharedMemoryLimits shm_limits_aircraft_data;

// Time points for current and previous timestamps
std::chrono::time_point<std::chrono::system_clock> current_time, previous_time;

// Function to send alert in case of proximity violation
void sendAlert(int aircraft1, int aircraft2) {
    std::cout << "DEBUG Alert: Aircraft " << aircraft1 <<
                 " and Aircraft " << aircraft2 << " are too close" << std::endl;
    // TODO: Implement alert mechanism (e.g., notify operator)
}

// Function to send aircraft ID to display
void sendIdToDisplay(int aircraft_id) {
    std::cout << "DEBUG SEND ID: " << aircraft_id << std::endl;
    // TODO: Implement display mechanism (e.g., show ID on screen)
}

// Function to calculate projected position in the next `time` seconds
void getProjectedPosition(AircraftData& aircraft, double time) {
    // Update the aircraft position based on its speed
    aircraft.x += aircraft.speedX * time / 3600;  // converting speed to distance
    aircraft.y += aircraft.speedY * time / 3600;
    aircraft.z += aircraft.speedZ * time / 3600;
}

// Thread function to check for violations
void* violationCheck(void* arg) {
    // Calculate the upper limit for shared memory
    void* upper_limit = (void*)((char*)shm_limits_aircraft_data.lower_limit + shm_limits_aircraft_data.size);
    previous_time = std::chrono::system_clock::now();
    double last_elapsedTime = 0;
    while (1) {
        current_time = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = current_time - previous_time;
        double elapsedTime = elapsed_seconds.count();  // Get the elapsed time in seconds

        pthread_mutex_lock(&shm_mutex);  // Lock shared memory before accessing

        std::cout << "\n\nChecking for Safety Violations..." << std::endl;
        std::cout << "[DEBUG] Shared Memory Base Address: " << shm_limits_aircraft_data.lower_limit << std::endl;

        AircraftData* current_aircraft = (AircraftData*) shm_limits_aircraft_data.lower_limit;

        while ((void*)current_aircraft < upper_limit) {
            if (current_aircraft->id == 0) {
                current_aircraft++;
                continue;
            }

            double diff_eplapsedTime = elapsedTime - last_elapsedTime;

            if (elapsedTime >= current_aircraft->time) {
				current_aircraft->x += current_aircraft->speedX * diff_eplapsedTime / 3600;
				current_aircraft->y += current_aircraft->speedY * diff_eplapsedTime / 3600;
				current_aircraft->z += current_aircraft->speedZ * diff_eplapsedTime / 3600;

				// Print the aircraft ID and new position
				std::cout << "Aircraft " << current_aircraft->id
						  << " updated to position (" << current_aircraft->x << ", "
						  << current_aircraft->y << ", " << current_aircraft->z << ")"
						  << " after " << elapsedTime << " seconds." << std::endl;

				for (AircraftData* next_aircraft = current_aircraft + 1; (void*)next_aircraft < upper_limit; next_aircraft++) {
					if (next_aircraft->id == 0) continue;  // Skip empty or invalid aircraft data

					// Update the position of next_aircraft based on the elapsed time
					next_aircraft->x += next_aircraft->speedX * diff_eplapsedTime / 3600;
					next_aircraft->y += next_aircraft->speedY * diff_eplapsedTime / 3600;
					next_aircraft->z += next_aircraft->speedZ * diff_eplapsedTime / 3600;

					if(elapsedTime >= next_aircraft->time){
						double dx = std::fabs(current_aircraft->x - next_aircraft->x);
						double dy = std::fabs(current_aircraft->y - next_aircraft->y);
						double dz = std::fabs(current_aircraft->z - next_aircraft->z);

						std::cout << "Comparing: " << current_aircraft->id << " (" << current_aircraft->x << "," << current_aircraft->y << "," << current_aircraft->z << ")"
								  << " with " << next_aircraft->id << " (" << next_aircraft->x << "," << next_aircraft->y << "," << next_aircraft->z << ")" << std::endl;

						double horizontalXYDiff = std::sqrt(dx * dx + dy * dy);

						// Check for proximity violation
						if (horizontalXYDiff < 3000 || dz < 1000) {
							sendAlert(current_aircraft->id, next_aircraft->id);
						}

						// Check if a violation will happen within the next 2 minutes
                        double time_to_violation = 0;
                        if (horizontalXYDiff >= 3000 && dz >= 1000) {
                            // Calculate the projected positions of both aircraft in 2 minutes (120 seconds)
                            AircraftData projected_current = *current_aircraft;
                            AircraftData projected_next = *next_aircraft;

                            getProjectedPosition(projected_current, 120);  // In 2 minutes
                            getProjectedPosition(projected_next, 120);  // In 2 minutes

                            // Calculate the projected distance between the two aircraft
                            double projected_dx = std::fabs(projected_current.x - projected_next.x);
                            double projected_dy = std::fabs(projected_current.y - projected_next.y);
                            double projected_dz = std::fabs(projected_current.z - projected_next.z);
                            double projected_horizontalXYDiff = std::sqrt(projected_dx * projected_dx + projected_dy * projected_dy);

                            if (projected_horizontalXYDiff < 3000 || projected_dz < 1000) {
                                sendAlert(current_aircraft->id, next_aircraft->id);  // Upcoming alert
                            }
                        }
					}
				}
            }

            current_aircraft++;  // Move to next aircraft
        }

        pthread_mutex_unlock(&shm_mutex);  // Unlock shared memory after access
        sleep(5);  // Sleep for 5 seconds
    }
}

int main() {
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return 1;
    }

    // Map the shared memory
    aircrafts_shared_memory = (AircraftData*) mmap(NULL,
                                                   sizeof(AircraftData) * MAX_AIRCRAFT,
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED,
                                                   shm_fd, 0);

    if (aircrafts_shared_memory == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // Initialize shared memory limits
    shm_limits_aircraft_data.lower_limit = aircrafts_shared_memory;
    shm_limits_aircraft_data.size = sizeof(AircraftData) * MAX_AIRCRAFT;

    std::cout << "[ComputerSystem] Shared Memory Base Address: " << aircrafts_shared_memory << " with params: " << SHM_NAME << ", " << shm_fd << std::endl;

    // Create a monitor thread for checking violations
    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, violationCheck, NULL);

    while (true) sleep(10);  // Main thread loops indefinitely

    // Unmap and close shared memory before exit
    munmap(aircrafts_shared_memory, sizeof(AircraftData) * MAX_AIRCRAFT);
    close(shm_fd);

    // Tutorial 7 has this unlink thing, but I think we should only call this at the end in Radar Subsystem?
    // I think because we should only unlink once at the end of the program to delete shared mem
    // if (shm_unlink(SHM_NAME) == -1) { perror("can't unlink"); exit(1); }

    return 0;
}
