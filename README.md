# COEN320 Air Traffic Control System

This project is an **air traffic control (ATC) simulation** developed for COEN320. It models real-time aircraft monitoring and safety violation detection using **QNX RTOS**. The system consists of multiple subsystems that communicate using **POSIX shared memory (shm_open, mmap)**.

## Project Structure

The system is broken down into five main subsystems:

1. **RadarSubsystem**  
   - Simulates **primary and secondary radar**.
   - Reads aircraft data (currently hardcoded but will later receive live updates).
   - Stores aircraft position and speed in **shared memory**.

2. **ComputerSystem**  
   - Reads aircraft data from **shared memory**.
   - Checks for **safety violations** (e.g., aircraft getting too close).
   - Sends alerts when a potential collision is detected.

3. **Operator Console**  
   - Provides a **user interface** for air traffic controllers.
   - Displays aircraft positions and alerts.

4. **Data Display**  
   - Visualizes the airspace in real time.

5. **Communication System**  
   - Simulates sending commands to aircraft.

## Implementation Details

- Each aircraft is simulated as a **periodic task** (thread or process).
- Shared memory is used for **inter-process communication (IPC)** between subsystems.
- The system is designed to run in **QNX Momentics IDE** and executes in the **QNX VM**.



