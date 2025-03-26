#include <iostream>
#include <cstdlib>
#include <unistd.h>

int main() {
    std::cout << "[TracomSystemLauncher] Launching all subsystems...\n";

    // 1. Launch ComputerSystem
    if (system("./ComputerSystem &") == -1) {
        std::cerr << "Failed to launch ComputerSystem\n";
    } else {
        std::cout << "✔️ Launched ComputerSystem\n";
    }

    sleep(1);  // slight buffer between launches

    // 2. Launch RadarSubsystem
    if (system("./RadarSubsystem &") == -1) {
        std::cerr << "Failed to launch RadarSubsystem\n";
    } else {
        std::cout << "✔️ Launched RadarSubsystem\n";
    }

    sleep(1);

    // 3. Launch OperatorConsoleSystem
    if (system("./OperatorConsoleSystem &") == -1) {
        std::cerr << "Failed to launch OperatorConsoleSystem\n";
    } else {
        std::cout << "✔️ Launched OperatorConsoleSystem\n";
    }

    sleep(1);

    // 4. Launch AirspaceManager
    if (system("./AirspaceManager &") == -1) {
        std::cerr << "Failed to launch AirspaceManager\n";
    } else {
        std::cout << "✔️ Launched AirspaceManager\n";
    }

    sleep(1);

    // 5. Launch DataDisplaySystem (foreground)
    std::cout << "▶️ Launching DataDisplaySystem (interactive)...\n";
    if (system("./DataDisplaySystem") == -1) {
        std::cerr << "Failed to launch DataDisplaySystem\n";
    }

    std::cout << "[TracomSystemLauncher] All subsystems launched.\n";
    return 0;
}
