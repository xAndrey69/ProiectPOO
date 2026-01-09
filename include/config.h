#ifndef CONFIG_H
#define CONFIG_H

#include <string>

class Config {
private:
    static Config* instance;
    Config(); // Constructor privat

public:
    // Initializate cu 0 pentru a garanta ca nu exista valori hardcodate
    int mapHeight;
    int mapWidth;
    int maxTicks;
    int maxStations;
    int clientsCount;
    int dronesCount;
    int robotsCount;
    int scootersCount;
    int totalPackages;
    int spawnFrequency;

    static Config* getInstance();
    void loadFromFile(const std::string& filename);
};

#endif
