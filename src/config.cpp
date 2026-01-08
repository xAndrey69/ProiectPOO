#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

Config* Config::instance = nullptr;

Config::Config() : 
    mapHeight(0), mapWidth(0), maxTicks(0), maxStations(0), 
    clientsCount(0), dronesCount(0), robotsCount(0), scootersCount(0), 
    totalPackages(0), spawnFrequency(0) {}

Config* Config::getInstance() {
    if (instance == nullptr) instance = new Config();
    return instance;
}

void Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Eroare Fatala: Nu pot deschide " << filename << ". Aplicatia se va inchide.\n";
        exit(1);
    }

    std::string line, key;
    while (std::getline(file, line)) {
        if (line.empty() || line.find("//") == 0) continue;
        std::stringstream ss(line);
        ss >> key;
        if (key.back() == ':') key.pop_back();

        if (key == "MAP_SIZE") ss >> mapHeight >> mapWidth;
        else if (key == "MAX_TICKS") ss >> maxTicks;
        else if (key == "MAX_STATIONS") ss >> maxStations;
        else if (key == "CLIENTS_COUNT") ss >> clientsCount;
        else if (key == "DRONES") ss >> dronesCount;
        else if (key == "ROBOTS") ss >> robotsCount;
        else if (key == "SCOOTERS") ss >> scootersCount;
        else if (key == "TOTAL_PACKAGES") ss >> totalPackages;
        else if (key == "SPAWN_FREQUENCY") ss >> spawnFrequency;
    }
    file.close();
}
